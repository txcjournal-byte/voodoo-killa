#pragma once

#include "ModuleCommon.h"

namespace vk
{
/** GATE: host-synced step pattern (volume chop) or smooth sidechain-style pump. */
class GateModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const GateParams& p) noexcept { target = p; }
    void process (float& l, float& r, const ModuleContext& ctx) noexcept;

    /** Pattern level (0..1) for a musical position. Pure function, used by tests and UI. */
    static float patternLevel (const GateParams& p, double ppqFromOrigin) noexcept;

private:
    double sr = 44100.0;
    GateParams target;
    OnePole depthSm, gainSm;
    float smoothMsCached = -1.0f;
};
} // namespace vk
