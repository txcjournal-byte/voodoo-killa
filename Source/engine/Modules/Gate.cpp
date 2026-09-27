#include "Gate.h"

namespace vk
{
void GateModule::prepare (double sampleRate)
{
    sr = sampleRate;
    depthSm.setTime (0.03f, sr);
    reset();
}

void GateModule::reset()
{
    depthSm.reset (target.enabled ? target.depth : 0.0f);
    gainSm.reset (1.0f);
    smoothMsCached = -1.0f;
}

float GateModule::patternLevel (const GateParams& p, double pos) noexcept
{
    const double rate = std::max (1.0 / 64.0, (double) p.rate);
    if (p.pump)
    {
        const double ph = pos / rate - std::floor (pos / rate);
        const double inv = 1.0 - ph;
        return (float) (1.0 - inv * inv * inv);
    }
    if (p.numSteps <= 0)
        return 1.0f;
    const auto step = (int64_t) std::floor (pos / rate + 1.0e-9);
    const auto idx = (int) (((step % p.numSteps) + p.numSteps) % p.numSteps);
    return p.pattern[(size_t) idx];
}

void GateModule::process (float& l, float& r, const ModuleContext& ctx) noexcept
{
    const float depth = depthSm.process (target.enabled ? target.depth : 0.0f);
    if (depth <= 1.0e-4f && ! target.enabled)
    {
        gainSm.reset (1.0f);
        return;
    }

    const float sm = target.pump ? 1.0f : std::max (0.5f, target.smoothMs);
    if (sm != smoothMsCached)
    {
        smoothMsCached = sm;
        gainSm.setTime (sm * 0.001f, sr);
    }

    const float level = patternLevel (target, ctx.ppqFromOrigin);
    const float g = gainSm.process (1.0f - depth * (1.0f - level));
    l *= g;
    r *= g;
}
} // namespace vk
