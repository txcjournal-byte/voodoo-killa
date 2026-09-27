#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace vk
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float clamp01 (float x) noexcept  { return std::min (1.0f, std::max (0.0f, x)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Interpolates frequencies (or any positive value) in log space. */
inline float lerpLog (float a, float b, float t) noexcept
{
    a = std::max (a, 1.0e-3f);
    b = std::max (b, 1.0e-3f);
    return a * std::pow (b / a, t);
}

/** 4-point, 3rd-order Hermite interpolation. */
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

/** One-pole parameter smoother (per-sample). Real-time safe, no allocation. */
struct OnePole
{
    float value = 0.0f, coeff = 0.0f;

    void setTime (float seconds, double sampleRate) noexcept
    {
        coeff = seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (seconds * sampleRate));
    }
    void reset (float v) noexcept { value = v; }
    float process (float target) noexcept
    {
        value = target + coeff * (value - target);
        return value;
    }
};

/** Linear ramp 0..1 used for click-free crossfades. */
struct Fader
{
    float value = 0.0f, step = 0.0f;

    void setTime (float seconds, double sampleRate) noexcept { step = (float) (1.0 / std::max (1.0, seconds * sampleRate)); }
    float process (bool on) noexcept
    {
        value = on ? std::min (1.0f, value + step) : std::max (0.0f, value - step);
        return value;
    }
};

/** Small deterministic PRNG (xorshift32). Used where results must be reproducible (scatter, KILL seed). */
struct Rng
{
    uint32_t state = 0x9E3779B9u;

    explicit Rng (uint32_t seed = 0x9E3779B9u) noexcept : state (seed == 0 ? 0x9E3779B9u : seed) {}
    uint32_t next() noexcept
    {
        uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return state = x;
    }
    float nextFloat() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }        // [0,1)
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }
    int nextInt (int maxExclusive) noexcept { return maxExclusive <= 0 ? 0 : (int) (next() % (uint32_t) maxExclusive); }
};

inline uint32_t hash32 (uint64_t v) noexcept
{
    v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
    v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
    v ^= v >> 31;
    return (uint32_t) v ^ (uint32_t) (v >> 32);
}

/** Topology-preserving-transform state variable filter (Zavalishin / Cytomic). */
struct Svf
{
    enum class Mode { lp, hp, bp, notch };

    float g = 0.0f, k = 1.4142f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;

    void set (float cutoffHz, float q, double sampleRate) noexcept
    {
        cutoffHz = std::min (std::max (cutoffHz, 10.0f), (float) (sampleRate * 0.49));
        g  = std::tan (kPi * cutoffHz / (float) sampleRate);
        k  = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float process (float v0, Mode mode) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (mode)
        {
            case Mode::lp:    return v2;
            case Mode::hp:    return v0 - k * v1 - v2;
            case Mode::bp:    return v1;
            case Mode::notch: return v0 - k * v1;
        }
        return v2;
    }
};

/** RBJ biquad (used for shelves / peaks). Transposed direct form II. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    void setLowShelf (float f, float gainDb, double sr) noexcept  { setShelf (f, gainDb, sr, false); }
    void setHighShelf (float f, float gainDb, double sr) noexcept { setShelf (f, gainDb, sr, true); }

    void setShelf (float f, float gainDb, double sr, bool high) noexcept
    {
        const float A  = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * std::min (f, (float) sr * 0.45f) / (float) sr;
        const float cw = std::cos (w0), sw = std::sin (w0);
        const float alpha = sw / 2.0f * std::sqrt (2.0f);   // S = 1
        const float sA = 2.0f * std::sqrt (A) * alpha;
        float nb0, nb1, nb2, na0, na1, na2;
        if (! high)
        {
            nb0 =        A * ((A + 1) - (A - 1) * cw + sA);
            nb1 =  2.0f * A * ((A - 1) - (A + 1) * cw);
            nb2 =        A * ((A + 1) - (A - 1) * cw - sA);
            na0 =             (A + 1) + (A - 1) * cw + sA;
            na1 = -2.0f *    ((A - 1) + (A + 1) * cw);
            na2 =             (A + 1) + (A - 1) * cw - sA;
        }
        else
        {
            nb0 =        A * ((A + 1) + (A - 1) * cw + sA);
            nb1 = -2.0f * A * ((A - 1) + (A + 1) * cw);
            nb2 =        A * ((A + 1) + (A - 1) * cw - sA);
            na0 =             (A + 1) - (A - 1) * cw + sA;
            na1 =  2.0f *    ((A - 1) - (A + 1) * cw);
            na2 =             (A + 1) - (A - 1) * cw - sA;
        }
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/** DC blocker (1st order HP ~10 Hz). */
struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double sr) noexcept { r = 1.0f - (kTwoPi * 10.0f / (float) sr); }
    void reset() noexcept { x1 = y1 = 0; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = y;
        return y;
    }
};

/** Power-of-two stereo-agnostic mono delay line with fractional (Hermite) read. Memory is allocated in prepare only. */
template <typename Container>
inline float readHermite (const Container& buf, int64_t mask, double pos) noexcept
{
    const double fl = std::floor (pos);
    const int64_t i = (int64_t) fl;
    const float t = (float) (pos - fl);
    return hermite (buf[(size_t) ((i - 1) & mask)], buf[(size_t) (i & mask)],
                    buf[(size_t) ((i + 1) & mask)], buf[(size_t) ((i + 2) & mask)], t);
}

inline int nextPow2 (int v) noexcept
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
} // namespace vk
