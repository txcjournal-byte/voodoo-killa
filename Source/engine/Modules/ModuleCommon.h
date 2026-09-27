#pragma once

#include "../DspUtil.h"
#include "../Preset.h"
#include <vector>

namespace vk
{
/** Per-sample musical context shared by all modules. */
struct ModuleContext
{
    double ppq = 0.0;            // song position (quarter notes)
    double ppqFromOrigin = 0.0;  // ppq relative to a bar line
    double samplesPerBeat = 22050.0;
    bool   active = false;       // trigger window
    float  activeEnv = 0.0f;     // smoothed 0..1 version of active
    double segElapsed = 0.0;     // beats since segment start
    double segLen = 4.0;         // beats
    float  progress = 0.0f;      // 0..1 inside the segment (0 when inactive)
};

/** Stereo power-of-two delay line with fractional Hermite reads (delay measured from the newest sample). */
class StereoDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        const int size = nextPow2 (maxDelaySamples + 8);
        l.assign ((size_t) size, 0.0f);
        r.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset()
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
    }
    void push (float xl, float xr) noexcept
    {
        ++w;
        l[(size_t) (w & mask)] = xl;
        r[(size_t) (w & mask)] = xr;
    }
    void read (double delay, float& ol, float& orr) const noexcept
    {
        delay = std::min ((double) (mask - 4), std::max (0.0, delay));
        const double pos = (double) w - delay;
        ol = readHermite (l, mask, pos);
        orr = readHermite (r, mask, pos);
    }
    float readL (double delay) const noexcept { return readHermite (l, mask, (double) w - std::min ((double) (mask - 4), std::max (0.0, delay))); }
    float readR (double delay) const noexcept { return readHermite (r, mask, (double) w - std::min ((double) (mask - 4), std::max (0.0, delay))); }
    int maxDelay() const noexcept { return (int) mask - 4; }

private:
    std::vector<float> l, r;
    int64_t mask = 0, w = 0;
};

/**
    Two-tap rotating-delay pitch shifter voice ("granular" with Hann-shaped crossfades).
    At phase 0.5 tap A sits exactly at `centreDelay` with full weight -> identical to a plain delay.
*/
struct ShifterVoice
{
    double phase = 0.5;

    void resetPhase (double p = 0.5) noexcept { phase = p; }

    /** Returns the shifted stereo sample. delayOffset lets callers add vibrato. */
    void process (const StereoDelay& d, double ratio, double grain, double minDelay, double delayOffset,
                  float& ol, float& orr) noexcept
    {
        phase += (1.0 - ratio) / grain;
        phase -= std::floor (phase);
        const double p2 = phase + 0.5 - std::floor (phase + 0.5);
        const float s1 = std::sin (kPi * (float) phase);
        const float w1 = s1 * s1;
        const float w2 = 1.0f - w1;
        float al, ar, bl, br;
        d.read (minDelay + phase * grain + delayOffset, al, ar);
        d.read (minDelay + p2 * grain + delayOffset, bl, br);
        ol = al * w1 + bl * w2;
        orr = ar * w1 + br * w2;
    }
};

/** Crossfade helper for discrete switches (filter / drive type): dips to the unprocessed signal, never to silence. */
struct SwitchFade
{
    int current = -1, pending = -1;
    float gain = 1.0f, step = 0.01f;

    void prepare (double sr) noexcept { step = (float) (1.0 / (0.006 * sr)); }
    void request (int type) noexcept
    {
        if (current < 0) { current = type; gain = 1.0f; }
        pending = type;
    }
    /** Returns true when the module must switch (and reset its state) now. */
    bool tick() noexcept
    {
        if (pending != current)
        {
            gain -= step;
            if (gain <= 0.0f) { gain = 0.0f; current = pending; return true; }
        }
        else if (gain < 1.0f)
            gain = std::min (1.0f, gain + step);
        return false;
    }
};
} // namespace vk
