#include "Filter.h"

namespace vk
{
void FilterModule::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* s : { &logCutSm, &logCut2Sm, &resoSm, &mixSm, &lfoDepthSm })
        s->setTime (0.03f, sr);
    fade.prepare (sr);
    reset();
}

void FilterModule::reset()
{
    logCutSm.reset (std::log2 (std::max (20.0f, target.cutoff)));
    logCut2Sm.reset (std::log2 (std::max (20.0f, target.cutoff2)));
    resoSm.reset (target.reso);
    mixSm.reset (target.type == FilterType::off ? 0.0f : target.mix);
    lfoDepthSm.reset (target.lfoDepth);
    type = target.type;
    fade.current = fade.pending = (int) type;
    fade.gain = 1.0f;
    for (auto& s : a) s.reset();
    for (auto& s : b) s.reset();
    counter = 0;
}

void FilterModule::updateCoefficients (const ModuleContext& ctx) noexcept
{
    float cut = std::exp2 (logCutSm.value);
    if (target.sweepTo > 0.0f && target.sweepFrom > 0.0f)
        cut = lerpLog (target.sweepFrom, target.sweepTo, ctx.progress) * (cut / std::max (20.0f, target.cutoff));

    const float depth = lfoDepthSm.value;
    if (depth > 1.0e-4f)
    {
        const double rate = std::max (1.0 / 32.0, (double) target.lfoRate);
        float lfo;
        if (target.lfoShape == LfoShape::sampleHold)
        {
            const auto cycle = (int64_t) std::floor (ctx.ppqFromOrigin / rate);
            lfo = Rng (hash32 ((uint64_t) cycle * 2654435761ULL + 17)).nextBipolar();
        }
        else
        {
            lfo = std::sin (kTwoPi * (float) (ctx.ppqFromOrigin / rate - std::floor (ctx.ppqFromOrigin / rate)));
        }
        cut *= std::exp2 (depth * 3.0f * lfo);
    }

    cut = std::clamp (cut, 20.0f, (float) (sr * 0.45));
    currentCutoff = cut;

    const float reso = clamp01 (resoSm.value);
    const float q = 0.707f + reso * reso * 11.0f;

    for (int c = 0; c < 2; ++c)
    {
        if (type == FilterType::band)
        {
            a[c].set (cut, 0.707f, sr);
            b[c].set (std::clamp (std::exp2 (logCut2Sm.value), cut, (float) (sr * 0.45)), 0.707f + reso * 2.0f, sr);
        }
        else
        {
            a[c].set (cut, q, sr);
        }
    }
}

void FilterModule::process (float& l, float& r, const ModuleContext& ctx) noexcept
{
    const float mixTarget = target.type == FilterType::off ? 0.0f : target.mix;
    const float mix = mixSm.process (mixTarget);
    logCutSm.process (std::log2 (std::max (20.0f, target.cutoff)));
    logCut2Sm.process (std::log2 (std::max (20.0f, target.cutoff2)));
    resoSm.process (target.reso);
    lfoDepthSm.process (target.lfoDepth);

    if (fade.tick())
    {
        type = (FilterType) fade.current;
        for (auto& s : a) s.reset();
        for (auto& s : b) s.reset();
        counter = 0;
    }

    if (type == FilterType::off || (mix < 1.0e-4f && mixTarget <= 0.0f))
        return;

    if (--counter <= 0)
    {
        counter = 8;
        updateCoefficients (ctx);
    }

    float in[2] = { l, r };
    float out[2];
    for (int c = 0; c < 2; ++c)
    {
        switch (type)
        {
            case FilterType::lp:    out[c] = a[c].process (in[c], Svf::Mode::lp); break;
            case FilterType::hp:    out[c] = a[c].process (in[c], Svf::Mode::hp); break;
            case FilterType::bp:    out[c] = a[c].process (in[c], Svf::Mode::bp) * 1.5f; break;
            case FilterType::notch: out[c] = a[c].process (in[c], Svf::Mode::notch); break;
            case FilterType::band:  out[c] = b[c].process (a[c].process (in[c], Svf::Mode::hp), Svf::Mode::lp); break;
            default:                out[c] = in[c]; break;
        }
    }

    const float m = mix * fade.gain;
    l = in[0] + (out[0] - in[0]) * m;
    r = in[1] + (out[1] - in[1]) * m;
}

// ===========================================================================
void ToneModule::prepare (double sampleRate)
{
    sr = sampleRate;
    toneSm.setTime (0.03f, sr);
    reset();
}

void ToneModule::reset()
{
    toneSm.reset (0.0f);
    lastTone = 0.0f;
    for (auto& s : lp) s.reset();
    for (auto& s : hp) s.reset();
    for (auto& b : air) b.reset();
}

void ToneModule::process (float& l, float& r, float tone) noexcept
{
    const float t = toneSm.process (tone);
    if (std::abs (t) < 0.01f)
    {
        if (lastTone != 0.0f)
        {
            lastTone = 0.0f;
            for (auto& s : lp) s.reset();
            for (auto& s : hp) s.reset();
            for (auto& b : air) b.reset();
        }
        return;
    }

    if (--counter <= 0 || (lastTone < 0.0f) != (t < 0.0f))
    {
        counter = 16;
        lastTone = t;
        for (int c = 0; c < 2; ++c)
        {
            if (t < 0.0f)
                lp[c].set (lerpLog (20000.0f, 450.0f, -t / 12.0f), 0.707f, sr);
            else
            {
                hp[c].set (lerpLog (20.0f, 450.0f, t / 12.0f), 0.707f, sr);
                air[c].setHighShelf (3500.0f, t * 0.45f, sr);
            }
        }
    }

    if (t < 0.0f)
    {
        l = lp[0].process (l, Svf::Mode::lp);
        r = lp[1].process (r, Svf::Mode::lp);
    }
    else
    {
        l = air[0].process (hp[0].process (l, Svf::Mode::hp));
        r = air[1].process (hp[1].process (r, Svf::Mode::hp));
    }
}
} // namespace vk
