#include "TimeBuffer.h"

namespace vk
{
// ===========================================================================
void TimeBuffer::prepare (double sampleRate, double seconds)
{
    const int size = nextPow2 ((int) std::ceil (sampleRate * seconds) + 16);
    left.assign ((size_t) size, 0.0f);
    right.assign ((size_t) size, 0.0f);
    mask = size - 1;
    writeIndex = 0;
}

void TimeBuffer::clear()
{
    std::fill (left.begin(), left.end(), 0.0f);
    std::fill (right.begin(), right.end(), 0.0f);
}

// ===========================================================================
void TimeEngine::Head::startGrains() noexcept
{
    // Slot 1 is mid-grain (full window gain) and currently reads exactly `pos`,
    // so a grain head started at "now" is sample-identical to the dry signal.
    grainAge[0] = 0;
    grainSrc[0] = pos;
    grainAge[1] = len / 2;
    grainSrc[1] = pos - dir * (double) (len / 2);
    grainCounter = len / 2;
}

void TimeEngine::Head::render (const TimeBuffer& buf, float& l, float& r) noexcept
{
    if (! grain)
    {
        buf.read (pos, l, r);
        return;
    }

    l = r = 0.0f;
    const float invLen = 1.0f / (float) len;
    for (int s = 0; s < 2; ++s)
    {
        const float x = (float) grainAge[s] * invLen;
        const float sn = std::sin (kPi * x);
        const float w = sn * sn;
        float gl, gr;
        buf.read (grainSrc[s] + dir * (double) grainAge[s], gl, gr);
        l += gl * w;
        r += gr * w;
    }
}

double TimeEngine::Head::bestGrainStart (const TimeBuffer& buf, double src, int other) const noexcept
{
    // WSOLA-style: start the new grain where its waveform best matches what the other (fading) grain
    // is about to play, so the overlap adds up in phase -> no comb filtering / "phasey" smear.
    if (! align || dir != 1 || alignRange <= 0)
        return src;

    // fixed cost at any sample rate: the search grid scales with the rate (~11 ms window, ±10 ms range)
    const int stride = std::max (2, alignRange / 220);
    const int M = std::min (len / 4, alignRange * 11 / 10);
    const auto ref = (int64_t) std::floor (grainSrc[other] + (double) grainAge[other]);
    const auto base = (int64_t) std::floor (src);
    const int kMin = -alignRange;
    const int kMax = (int) std::min<int64_t> (alignRange, buf.newest() - base - M);   // never read the future
    if (kMax < kMin)
        return src;

    // coarse search on the grid, then refine around the best lag
    auto score = [&] (int k)
    {
        double s = 0.0;
        for (int j = 0; j < M; j += stride)
            s += (double) buf.monoAt (ref + j) * (double) buf.monoAt (base + k + j);
        return s;
    };
    double best = -1.0e30;
    int bestK = 0;
    for (int k = kMin; k <= kMax; k += stride)
        if (const double s = score (k); s > best) { best = s; bestK = k; }
    const int lo = std::max (kMin, bestK - stride), hi = std::min (kMax, bestK + stride);
    for (int k = lo; k <= hi; ++k)
        if (const double s = score (k); s > best) { best = s; bestK = k; }
    return src + (double) bestK;
}

void TimeEngine::Head::advance (const TimeBuffer& buf) noexcept
{
    if (! grain)
        return;

    ++grainAge[0];
    ++grainAge[1];
    if (--grainCounter <= 0)
    {
        const int slot = grainAge[0] >= grainAge[1] ? 0 : 1;
        grainAge[slot] = 0;
        grainSrc[slot] = bestGrainStart (buf, pos, 1 - slot);
        grainCounter = len / 2;
    }
}

// ===========================================================================
void TimeEngine::prepare (double sampleRate, double bufferSeconds)
{
    sr = sampleRate;
    buffer.prepare (sampleRate, bufferSeconds);
    grainLen = std::max (256, (int) std::round (sampleRate * 0.046) & ~1);
    holdGrainLen = std::max (512, (int) std::round (sampleRate * 0.080) & ~1);
    alignRange = std::max (32, (int) std::round (sampleRate * 0.010));
    xfadeSamples = std::max (32, (int) std::round (sampleRate * 0.010));
    muteSmoother.setTime (0.002f, sampleRate);
    reset();
}

void TimeEngine::reset()
{
    buffer.clear();
    current = Head {};
    previous = Head {};
    previousActive = false;
    started = false;
    currentKey = Key {};
    forceNewHead = true;
    repeatSegment = -1;
    onSamples = 0;
    muteSmoother.reset (1.0f);
    lastOffsetFromNow = 0.0;
}

TimeEngine::Target TimeEngine::computeTarget (const TimeParams& p, double e, double L, double spb, int64_t segKey) noexcept
{
    Target t;
    const bool follow = p.pitchFollow < 0 ? timeModePitchFollowDefault (p.mode) : p.pitchFollow == 1;
    const double pr = L > 0.0 ? std::clamp (e / L, 0.0, 1.0) : 0.0;
    const double k = 1.0 + 3.0 * std::clamp ((double) p.curve, 0.0, 1.0);
    const double R = std::max (16.0, (double) p.rate * spb);

    auto slowGain = [] (double v) { return (float) std::min (1.0, std::abs (v) * 4.0); };

    switch (p.mode)
    {
        case TimeMode::none:
        case TimeMode::count:
            t.offset = e;
            break;

        case TimeMode::half:
        case TimeMode::quarter:
        {
            // Halftime Engine: TAPE / KEY = varispeed read (pitch follows), HOLD = aligned grains (pitch kept)
            const double r = 1.0 / std::max (1.0, (double) halftimeFactor (p));
            const bool hold = halftimeMode (p) == htHold;
            if (p.reverse)
            {
                t.offset = -e * r;          // the recent past, backwards, slowed
                t.speed = -r;
                t.dir = -1;
            }
            else
            {
                const double B = std::min ((double) p.brake * spb, L * 0.5);
                if (B > 1.0 && e < B)
                {
                    // brake: speed glides 1 -> r, so the loop start is seamless (no jump)
                    t.speed = 1.0 - (1.0 - r) * (e / B);
                    t.offset = e - (1.0 - r) * e * e / (2.0 * B);
                }
                else if (B > 1.0)
                {
                    t.speed = r;
                    t.offset = B * (1.0 + r) * 0.5 + r * (e - B);
                }
                else
                {
                    t.speed = r;
                    t.offset = e * r;
                }
            }
            t.grain = hold;
            t.align = hold && ! p.reverse;
            t.grainLen = hold ? holdGrainLen : 0;
            if (p.smoothMs > 0.0f)
                t.xfade = (int) std::clamp ((double) p.smoothMs * 0.001 * sr, 32.0, sr * 0.5);
            juce::ignoreUnused (follow);
            break;
        }

        case TimeMode::dbl:
            t.offset = 2.0 * e - L;
            t.speed = 2.0;
            t.grain = ! follow;
            break;

        case TimeMode::stop:
        {
            const double v = std::pow (1.0 - pr, k);
            t.offset = L * (1.0 - std::pow (1.0 - pr, k + 1.0)) / (k + 1.0);
            t.speed = v;
            t.gain = slowGain (v);
            t.grain = ! follow;
            break;
        }

        case TimeMode::start:
        {
            const double v = std::pow (pr, k);
            t.offset = L * std::pow (pr, k + 1.0) / (k + 1.0);
            t.speed = v;
            t.gain = slowGain (v);
            t.grain = ! follow;
            break;
        }

        case TimeMode::stopstart:
        {
            const double h = L * 0.5;
            if (e < h)
            {
                const double q = std::clamp (e / h, 0.0, 1.0);
                const double v = std::pow (1.0 - q, k);
                t.offset = h * (1.0 - std::pow (1.0 - q, k + 1.0)) / (k + 1.0);
                t.speed = v;
                t.gain = slowGain (v);
            }
            else
            {
                const double q = std::clamp ((e - h) / h, 0.0, 1.0);
                const double v = std::pow (q, k);
                t.offset = h / (k + 1.0) + h * std::pow (q, k + 1.0) / (k + 1.0);
                t.speed = v;
                t.gain = slowGain (v);
            }
            t.grain = ! follow;
            break;
        }

        case TimeMode::rewind:
        {
            // speed goes +1 -> 0 -> -3 : decelerate, then spin backwards
            const double v = 1.0 - 4.0 * pr;
            t.offset = L * (pr - 2.0 * pr * pr);
            t.speed = v;
            t.gain = (float) std::min (1.0, 0.35 + std::abs (v) * 3.0);
            t.grain = false;
            break;
        }

        case TimeMode::reverse:
        {
            if (p.alternate)
            {
                const auto c = (int64_t) std::floor (e / R);
                const double rel = e - (double) c * R;
                t.slice = c;
                t.sliceLen = R;
                if ((c & 1) == 0) { t.offset = e; t.speed = 1.0; }
                else              { t.offset = (double) c * R - rel; t.speed = -1.0; }
            }
            else
            {
                const double s = std::clamp ((double) p.speed, 0.125, 2.0);
                t.offset = -e * s;
                t.speed = -s;
                t.dir = -1;
                t.grain = std::abs (s - 1.0) > 1.0e-3 && ! follow;
            }
            break;
        }

        case TimeMode::repeat:
        {
            if (segKey != repeatSegment || e < sliceStartE)
            {
                repeatSegment = segKey;
                sliceStartE = 0.0;
                sliceLenE = R;
                sliceIndex = 0;
            }
            int guard = 0;
            while (e >= sliceStartE + sliceLenE && guard++ < 256)
            {
                sliceStartE += sliceLenE;
                ++sliceIndex;
                const double pp = L > 0.0 ? sliceStartE / L : 0.0;
                const double shrink = std::max (0.125, 1.0 - (double) p.ramp * 0.875 * std::clamp (pp, 0.0, 1.0));
                sliceLenE = std::max (16.0, R * shrink);
            }
            const double rel = e - sliceStartE;
            const bool reversed = p.alternate && (sliceIndex & 1) == 1;
            t.offset = reversed ? sliceLenE - rel : rel;
            t.speed = reversed ? -1.0 : 1.0;
            t.slice = sliceIndex;
            t.sliceLen = sliceLenE;
            break;
        }

        case TimeMode::scatter:
        {
            const auto c = (int64_t) std::floor (e / R);
            const double rel = e - (double) c * R;
            Rng rng (hash32 ((uint64_t) segKey * 1315423911ULL + (uint64_t) c));
            const float choice = c == 0 ? 0.0f : rng.nextFloat();
            t.slice = c;
            t.sliceLen = R;
            if (choice < 0.30f)      { t.offset = e; }                                              // straight
            else if (choice < 0.55f) { t.offset = (double) (c - 1) * R + rel; }                     // repeat previous slice
            else if (choice < 0.75f) { t.offset = (double) (c - 1 - rng.nextInt (4)) * R + rel; }   // jump back
            else if (choice < 0.90f) { t.offset = (double) c * R - rel; t.speed = -1.0; }           // reversed
            else                     { t.offset = (double) c * R + rel * 0.5 - R * 0.5; t.speed = 0.5; } // slowed
            break;
        }
    }
    return t;
}

void TimeEngine::process (float& l, float& r, const TimeParams& p, const Frame& f) noexcept
{
    const float inL = l, inR = r;
    buffer.write (l, r);
    const double now = (double) buffer.newest();
    const double spb = std::max (1.0, f.samplesPerBeat);
    const double e = std::max (0.0, f.elapsedBeats * spb);
    const double L = std::max (1.0, f.segLenBeats * spb);

    const bool on = f.active && p.mode != TimeMode::none && p.mix > 0.0f;

    Key key;
    Target t;
    if (on)
    {
        const auto segKey = (int64_t) std::llround (f.segStartPpq * 15360.0);
        t = computeTarget (p, e, L, spb, segKey);
        key.mode = (int) p.mode + 1;
        key.segment = segKey;
        key.slice = t.slice;
    }

    if (key != currentKey || forceNewHead)
    {
        int xf = xfadeSamples;
        if (t.xfade > 0)
            xf = t.xfade;
        else if (t.sliceLen > 0.0)
            xf = std::clamp ((int) (t.sliceLen * 0.3), 32, xfadeSamples);

        if (started)
        {
            previous = current;
            previousActive = true;
            previous.fadeStep = 1.0f / (float) xf;
        }

        current = Head {};
        current.grain = t.grain;
        current.dir = t.dir;
        current.pos = on ? std::min (now, now - e + t.offset) : now;
        current.fade = started ? 0.0f : 1.0f;
        current.fadeStep = 1.0f / (float) xf;
        current.len = t.grainLen > 0 ? t.grainLen : grainLen;
        current.align = t.align;
        current.alignRange = alignRange;
        if (current.grain)
            current.startGrains();

        currentKey = key;
        forceNewHead = false;
        started = true;
    }

    // --- current head (position recomputed from the segment start every sample -> no drift)
    current.pos = on ? std::min (now, now - e + t.offset) : now;
    current.speed = on ? t.speed : 1.0;
    current.gain = on ? t.gain : 1.0f;

    float cl, cr;
    current.render (buffer, cl, cr);
    const float cg = current.gain * current.fade;
    float outL = cl * cg, outR = cr * cg;
    current.fade = std::min (1.0f, current.fade + current.fadeStep);
    current.advance (buffer);

    // --- previous head keeps running at its last speed while it fades out
    if (previousActive)
    {
        float pl, pr;
        previous.render (buffer, pl, pr);
        const float pg = previous.gain * previous.fade;
        outL += pl * pg;
        outR += pr * pg;
        previous.fade -= previous.fadeStep;
        previous.pos = std::min (now, previous.pos + previous.speed);
        previous.advance (buffer);
        if (previous.fade <= 0.0f)
            previousActive = false;
    }

    // --- end mute (e.g. "Pre-Drop Stop")
    const bool muted = on && p.endMute > 0.0f && e >= L - (double) p.endMute * spb;
    const float m = muteSmoother.process (muted ? 0.0f : 1.0f);

    // FADE: ease the halftime in after it starts
    onSamples = on ? onSamples + 1 : 0;
    float fadeGain = 1.0f;
    if (on && isHalftimeMode (p.mode) && p.fade > 0.0f)
        fadeGain = (float) std::min (1.0, (double) onSamples / std::max (1.0, (double) p.fade * spb));

    const float mix = on || previousActive ? clamp01 (p.mix) * fadeGain : 1.0f;
    l = lerp (inL, outL * m, mix);
    r = lerp (inR, outR * m, mix);

    lastOffsetFromNow = current.pos - now;
}
} // namespace vk
