#include "Morph.h"
#include "DspUtil.h"

namespace vk
{
namespace
{
    template <typename T>
    T pick (const T& a, const T& b, float t) noexcept { return t < 0.5f ? a : b; }

    /** Returns a copy of `on` with its intensity field replaced so it acts as the neutral side. */
    TimeParams morphTime (const TimeParams& a, const TimeParams& b, float t) noexcept
    {
        const bool aOn = a.mode != TimeMode::none, bOn = b.mode != TimeMode::none;
        if (! aOn && ! bOn)
            return a;
        if (aOn != bOn)
        {
            TimeParams r = aOn ? a : b;
            r.mix = aOn ? lerp (a.mix, 0.0f, t) : lerp (0.0f, b.mix, t);
            return r;
        }
        if (a.mode != b.mode)
        {
            // different modes: switch at 0.5 (the TIME module crossfades its read heads)
            return pick (a, b, t);
        }
        TimeParams r = pick (a, b, t);   // discrete fields (rate, alternate, pitchFollow)
        r.curve = lerp (a.curve, b.curve, t);
        r.ramp = lerp (a.ramp, b.ramp, t);
        r.speed = lerp (a.speed, b.speed, t);
        r.endMute = lerp (a.endMute, b.endMute, t);
        r.mix = lerp (a.mix, b.mix, t);
        return r;
    }

    PitchParams morphPitch (const PitchParams& a, const PitchParams& b, float t) noexcept
    {
        PitchParams r;
        r.semis = lerp (a.semis, b.semis, t);
        r.fine = lerp (a.fine, b.fine, t);
        r.endDrop = lerp (a.endDrop, b.endDrop, t);
        r.ramp = lerp (a.ramp, b.ramp, t);
        r.wobbleCents = lerp (a.wobbleCents, b.wobbleCents, t);
        const float wa = a.wobbleCents > 0.0f ? a.wobbleHz : b.wobbleHz;
        const float wb = b.wobbleCents > 0.0f ? b.wobbleHz : a.wobbleHz;
        r.wobbleHz = lerp (wa, wb, t);
        r.formant = lerp (a.formant, b.formant, t);
        const bool aShift = a.semis != 0.0f || a.fine != 0.0f;
        const bool bShift = b.semis != 0.0f || b.fine != 0.0f;
        r.blend = lerp (aShift ? a.blend : b.blend, bShift ? b.blend : a.blend, t);
        r.detuneCents = lerp (a.detuneCents, b.detuneCents, t);
        return r;
    }

    GateParams morphGate (const GateParams& a, const GateParams& b, float t) noexcept
    {
        if (! a.enabled && ! b.enabled)
            return a;
        if (a.enabled != b.enabled)
        {
            GateParams r = a.enabled ? a : b;
            r.depth = a.enabled ? lerp (a.depth, 0.0f, t) : lerp (0.0f, b.depth, t);
            return r;
        }
        GateParams r = pick (a, b, t);
        r.depth = lerp (a.depth, b.depth, t);
        r.smoothMs = lerp (a.smoothMs, b.smoothMs, t);
        return r;
    }

    FilterParams morphFilter (const FilterParams& a, const FilterParams& b, float t) noexcept
    {
        const bool aOn = a.type != FilterType::off, bOn = b.type != FilterType::off;
        if (! aOn && ! bOn)
            return a;
        if (aOn != bOn)
        {
            FilterParams r = aOn ? a : b;
            r.mix = aOn ? lerp (a.mix, 0.0f, t) : lerp (0.0f, b.mix, t);
            return r;
        }
        FilterParams r = pick (a, b, t);
        if (a.type != b.type)
        {
            // different filter shapes: dip to dry around the switch point
            r.mix *= std::min (1.0f, std::abs (t - 0.5f) * 4.0f);
            return r;
        }
        r.cutoff = lerpLog (a.cutoff, b.cutoff, t);
        r.cutoff2 = lerpLog (a.cutoff2, b.cutoff2, t);
        r.reso = lerp (a.reso, b.reso, t);
        r.lfoDepth = lerp (a.lfoDepth, b.lfoDepth, t);
        if (a.sweepTo > 0.0f && b.sweepTo > 0.0f)
        {
            r.sweepFrom = lerpLog (a.sweepFrom, b.sweepFrom, t);
            r.sweepTo = lerpLog (a.sweepTo, b.sweepTo, t);
        }
        r.mix = lerp (a.mix, b.mix, t);
        return r;
    }

    LofiParams neutralLofi() noexcept
    {
        LofiParams n;
        n.enabled = true;
        n.srate = 48000.0f;
        return n;
    }

    LofiParams morphLofi (const LofiParams& a0, const LofiParams& b0, float t) noexcept
    {
        if (! a0.enabled && ! b0.enabled)
            return a0;
        const LofiParams a = a0.enabled ? a0 : neutralLofi();
        const LofiParams b = b0.enabled ? b0 : neutralLofi();
        LofiParams r;
        r.enabled = true;
        r.bits = lerp (a.bits, b.bits, t);
        r.srate = lerpLog (a.srate, b.srate, t);
        r.wow = lerp (a.wow, b.wow, t);
        r.flutter = lerp (a.flutter, b.flutter, t);
        r.noise = lerp (a.noise, b.noise, t);
        r.tone = lerpLog (a.tone, b.tone, t);
        return r;
    }

    DriveParams morphDrive (const DriveParams& a, const DriveParams& b, float t) noexcept
    {
        const bool aOn = a.type != DriveType::off, bOn = b.type != DriveType::off;
        if (! aOn && ! bOn)
            return a;
        if (aOn != bOn)
        {
            DriveParams r = aOn ? a : b;
            r.amount = aOn ? lerp (a.amount, 0.0f, t) : lerp (0.0f, b.amount, t);
            return r;
        }
        DriveParams r = pick (a, b, t);
        if (a.type != b.type)
        {
            r.amount *= std::min (1.0f, std::abs (t - 0.5f) * 4.0f);
            return r;
        }
        r.amount = lerp (a.amount, b.amount, t);
        r.postLP = lerpLog (a.postLP, b.postLP, t);
        return r;
    }

    SpaceParams morphSpace (const SpaceParams& a, const SpaceParams& b, float t) noexcept
    {
        SpaceParams r;
        const bool aRev = a.reverbMix > 0.0f || a.shimmer > 0.0f || a.freeze;
        const bool bRev = b.reverbMix > 0.0f || b.shimmer > 0.0f || b.freeze;
        r.reverbSize = lerp (aRev ? a.reverbSize : b.reverbSize, bRev ? b.reverbSize : a.reverbSize, t);
        r.reverbMix = lerp (a.reverbMix, b.reverbMix, t);
        r.shimmer = lerp (a.shimmer, b.shimmer, t);
        r.freeze = pick (a.freeze, b.freeze, t);

        const bool aDel = a.delayMix > 0.0f, bDel = b.delayMix > 0.0f;
        r.delayBeats = aDel && bDel ? pick (a.delayBeats, b.delayBeats, t) : (aDel ? a.delayBeats : b.delayBeats);
        r.pingPong = aDel && bDel ? pick (a.pingPong, b.pingPong, t) : (aDel ? a.pingPong : b.pingPong);
        r.delayFb = lerp (a.delayFb, b.delayFb, t);
        r.delayMix = lerp (a.delayMix, b.delayMix, t);
        return r;
    }
}

PresetData morphPresets (const PresetData& a, const PresetData& b, float t) noexcept
{
    t = clamp01 (t);
    PresetData r;
    r.trigger = pick (a.trigger, b.trigger, t);
    r.length = pick (a.length, b.length, t);
    r.time = morphTime (a.time, b.time, t);
    r.pitch = morphPitch (a.pitch, b.pitch, t);
    r.gate = morphGate (a.gate, b.gate, t);
    r.filter = morphFilter (a.filter, b.filter, t);
    r.lofi = morphLofi (a.lofi, b.lofi, t);
    r.drive = morphDrive (a.drive, b.drive, t);
    r.space = morphSpace (a.space, b.space, t);
    r.width = lerp (a.width, b.width, t);
    r.duck.amount = lerp (a.duck.amount, b.duck.amount, t);
    r.duck.releaseMs = lerp (a.duck.releaseMs, b.duck.releaseMs, t);
    return r;
}

// ---------------------------------------------------------------------------
PresetData applyMacros (const PresetData& in, const MacroValues& m) noexcept
{
    PresetData p = in;
    const float a = clamp01 (m.amount);

    // AMOUNT: the time effect is fully on from 50 %, everything else scales linearly
    p.time.mix *= std::min (1.0f, a * 2.0f);

    p.pitch.wobbleCents *= a;
    p.pitch.endDrop *= a;
    p.pitch.ramp *= a;
    p.pitch.detuneCents *= a;
    p.pitch.formant *= a;

    p.gate.depth *= a;

    switch (p.filter.type)
    {
        case FilterType::lp:
            p.filter.cutoff = lerpLog (20000.0f, p.filter.cutoff, a);
            break;
        case FilterType::hp:
            p.filter.cutoff = lerpLog (20.0f, p.filter.cutoff, a);
            break;
        default:
            p.filter.mix *= a;
            break;
    }
    if (p.filter.sweepTo > 0.0f)
        p.filter.mix = in.filter.mix * std::min (1.0f, a * 2.0f);
    p.filter.lfoDepth *= a;

    if (p.lofi.enabled)
    {
        p.lofi.bits = 16.0f - (16.0f - p.lofi.bits) * a;
        p.lofi.srate = lerpLog (48000.0f, p.lofi.srate, a);
        p.lofi.wow *= a;
        p.lofi.flutter *= a;
        p.lofi.noise *= a;
        p.lofi.tone = lerpLog (20000.0f, p.lofi.tone, a);
    }

    p.drive.amount *= a;
    p.width = 1.0f + (p.width - 1.0f) * a;

    // SPACE: 0.5 = as designed, 0 = dry, 1 = double (and adds a room to dry presets)
    const float s = clamp01 (m.space) * 2.0f;
    p.space.reverbMix = std::min (1.0f, p.space.reverbMix * s * a);
    p.space.shimmer = std::min (1.0f, p.space.shimmer * s * a);
    p.space.delayMix = std::min (1.0f, p.space.delayMix * s * a);
    if (s > 1.0f && in.space.reverbMix <= 0.0f && in.space.shimmer <= 0.0f && ! in.space.freeze)
    {
        p.space.reverbMix = (s - 1.0f) * 0.3f;
        p.space.reverbSize = 0.6f;
    }

    // SPEED: 2 = twice as fast (half the length / rate), 1/2 = twice as slow
    const float sp = std::clamp (m.speed, 0.125f, 8.0f);
    p.length = std::clamp (p.length / sp, 0.125f, 64.0f);
    p.time.rate = std::clamp (p.time.rate / sp, 1.0f / 32.0f, 4.0f);
    p.gate.rate = std::clamp (p.gate.rate / sp, 1.0f / 32.0f, 4.0f);
    return p;
}

// ---------------------------------------------------------------------------
PresetData mutatePreset (const PresetData& in, uint32_t seed, float amount) noexcept
{
    PresetData p = in;
    Rng rng (seed);
    auto jitter = [&] (float& v, float lo, float hi)
    {
        if (v == 0.0f) { rng.next(); return; }
        v = std::clamp (v * (1.0f + rng.nextBipolar() * amount), lo, hi);
    };
    auto jitterLog = [&] (float& v, float lo, float hi)
    {
        v = std::clamp (v * std::exp2 (rng.nextBipolar() * amount * 2.0f), lo, hi);
    };

    jitter (p.time.curve, 0.0f, 1.0f);
    jitter (p.time.ramp, 0.0f, 1.0f);
    jitter (p.pitch.wobbleCents, 0.0f, 100.0f);
    jitter (p.pitch.wobbleHz, 0.05f, 12.0f);
    jitter (p.pitch.formant, -12.0f, 12.0f);
    jitter (p.pitch.detuneCents, 0.0f, 50.0f);
    jitter (p.gate.depth, 0.0f, 1.0f);
    if (p.filter.type != FilterType::off)
    {
        jitterLog (p.filter.cutoff, 20.0f, 20000.0f);
        jitterLog (p.filter.cutoff2, 20.0f, 20000.0f);
        jitter (p.filter.reso, 0.0f, 1.0f);
        jitter (p.filter.lfoDepth, 0.0f, 1.0f);
    }
    if (p.lofi.enabled)
    {
        jitter (p.lofi.bits, 4.0f, 16.0f);
        jitterLog (p.lofi.srate, 1000.0f, 48000.0f);
        jitter (p.lofi.wow, 0.0f, 1.0f);
        jitter (p.lofi.flutter, 0.0f, 1.0f);
        jitter (p.lofi.noise, 0.0f, 1.0f);
        jitterLog (p.lofi.tone, 200.0f, 20000.0f);
    }
    jitter (p.drive.amount, 0.0f, 1.0f);
    jitter (p.space.reverbSize, 0.0f, 1.0f);
    jitter (p.space.reverbMix, 0.0f, 1.0f);
    jitter (p.space.shimmer, 0.0f, 1.0f);
    jitter (p.space.delayFb, 0.0f, 0.9f);
    jitter (p.space.delayMix, 0.0f, 1.0f);
    if (p.width != 1.0f)
        jitter (p.width, 0.0f, 2.0f);
    return p;
}
} // namespace vk
