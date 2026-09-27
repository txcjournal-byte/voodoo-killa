#pragma once

#include "ModuleCommon.h"

namespace vk
{
/** FILTER: LP / HP / BP / notch / band (HP+LP) state-variable filter with synced LFO (sine or S&H) and segment sweep. */
class FilterModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const FilterParams& p) noexcept { target = p; fade.request ((int) p.type); }
    void process (float& l, float& r, const ModuleContext& ctx) noexcept;

    float getCurrentCutoff() const noexcept { return currentCutoff; }

private:
    void updateCoefficients (const ModuleContext& ctx) noexcept;

    double sr = 44100.0;
    FilterParams target;
    FilterType type = FilterType::off;
    SwitchFade fade;
    OnePole logCutSm, logCut2Sm, resoSm, mixSm, lfoDepthSm;
    Svf a[2], b[2];
    int counter = 0;
    float currentCutoff = 20000.0f;
};

/** TONE macro: tilt -12 (dark, low-pass) .. +12 (bright, high-pass + air shelf). */
class ToneModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float& l, float& r, float tone) noexcept;

private:
    double sr = 44100.0;
    OnePole toneSm;
    Svf lp[2], hp[2];
    Biquad air[2];
    float lastTone = 0.0f;
    int counter = 0;
};

/** WIDTH: mid/side width 0..200 %. */
class WidthModule
{
public:
    void prepare (double sampleRate) { sm.setTime (0.03f, sampleRate); sm.reset (1.0f); }
    void reset() { sm.reset (1.0f); }
    void process (float& l, float& r, float width) noexcept
    {
        const float w = sm.process (width);
        if (std::abs (w - 1.0f) < 1.0e-4f)
            return;
        const float m = (l + r) * 0.5f;
        const float s = (l - r) * 0.5f * w;
        const float comp = w > 1.0f ? 1.0f / (1.0f + (w - 1.0f) * 0.25f) : 1.0f;
        l = (m + s) * comp;
        r = (m - s) * comp;
    }

private:
    OnePole sm;
};
} // namespace vk
