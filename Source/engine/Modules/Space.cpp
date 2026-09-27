#include "Space.h"

namespace vk
{
void SpaceModule::prepare (double sampleRate)
{
    sr = sampleRate;
    reverb.setSampleRate (sr);
    for (auto* s : { &revMixSm, &shimmerSm, &delayMixSm, &fbSm })
        s->setTime (0.03f, sr);
    delayTimeSm.setTime (0.12f, sr);
    delay.prepare ((int) (8.5 * sr));
    shimmerGrain = std::round (0.05 * sr);
    shimmerMin = std::round (0.002 * sr);
    shimmerBuf.prepare ((int) (shimmerGrain * 2 + shimmerMin * 2 + 16));
    for (int c = 0; c < 2; ++c)
    {
        fbLP[c].set (5200.0f, 0.707f, sr);
        fbHP[c].set (140.0f, 0.707f, sr);
    }
    reset();
}

void SpaceModule::reset()
{
    reverb.reset();
    lastFreeze = false;
    lastSize = -1.0f;
    revMixSm.reset (target.reverbMix);
    shimmerSm.reset (target.shimmer);
    delayMixSm.reset (target.delayMix);
    fbSm.reset (target.delayFb);
    delayTimeSm.reset (-1.0f);
    delay.reset();
    shimmerBuf.reset();
    shimmerVoice.resetPhase();
    shimL = shimR = fbL = fbR = 0.0f;
    for (auto& f : fbLP) f.reset();
    for (auto& f : fbHP) f.reset();
    tailEnergy = 0.0f;
    silentSamples = 0;
    sleeping = true;
}

void SpaceModule::process (float inL, float inR, bool active, double spb, float& wetL, float& wetR) noexcept
{
    wetL = wetR = 0.0f;

    const float revMix = revMixSm.process (target.freeze ? std::max (target.reverbMix, 0.35f) : target.reverbMix);
    const float shimmer = shimmerSm.process (target.shimmer);
    const float dMix = delayMixSm.process (target.delayMix);
    const float fb = fbSm.process (std::min (0.95f, target.delayFb));

    const bool wanted = revMix > 1.0e-4f || shimmer > 1.0e-4f || dMix > 1.0e-4f || target.freeze;
    const float inAbs = std::abs (inL) + std::abs (inR);

    if (sleeping)
    {
        if (! wanted || inAbs < 1.0e-7f)
            return;
        sleeping = false;
        silentSamples = 0;
    }

    // ---------------- reverb (+ shimmer feedback)
    const bool freeze = target.freeze && active;
    const float size = target.reverbSize;
    if (freeze != lastFreeze || std::abs (size - lastSize) > 1.0e-3f)
    {
        lastFreeze = freeze;
        lastSize = size;
        // keep the shimmer feedback loop stable: long rooms + pitch feedback would run away
        const float maxRoom = target.shimmer > 0.0f ? 0.86f : 0.99f;
        revParams.roomSize = std::min (maxRoom, 0.35f + 0.64f * clamp01 (size));
        revParams.damping = 0.35f;
        revParams.wetLevel = 0.34f;
        revParams.dryLevel = 0.0f;
        revParams.width = 1.0f;
        revParams.freezeMode = freeze ? 1.0f : 0.0f;
        reverb.setParameters (revParams);
    }

    const float shimAmt = std::min (1.0f, shimmer);
    const float revSend = std::max (revMix, shimAmt * 0.8f);
    // shimmer feedback is soft-limited so the loop gain can never exceed 1
    float rl = inL + std::tanh (shimL * shimAmt * 0.3f);
    float rr = inR + std::tanh (shimR * shimAmt * 0.3f);
    if (revSend > 1.0e-4f || freeze)
    {
        reverb.processStereo (&rl, &rr, 1);
    }
    else
    {
        rl = rr = 0.0f;
    }

    if (shimAmt > 1.0e-4f)
    {
        shimmerBuf.push (rl, rr);
        shimmerVoice.process (shimmerBuf, 2.0, shimmerGrain, shimmerMin, 0.0, shimL, shimR);
    }
    else
    {
        shimL = shimR = 0.0f;
    }

    wetL += rl * revSend + shimL * shimAmt * 0.35f;
    wetR += rr * revSend + shimR * shimAmt * 0.35f;

    // ---------------- tempo-synced delay
    const double targetDelay = std::clamp ((double) target.delayBeats * spb, 16.0, (double) delay.maxDelay() - 8.0);
    if (delayTimeSm.value < 0.0f)
        delayTimeSm.reset ((float) targetDelay);
    const double dt = delayTimeSm.process ((float) targetDelay);

    if (dMix > 1.0e-4f || std::abs (fbL) + std::abs (fbR) > 1.0e-6f)
    {
        float dl, dr;
        delay.read (dt, dl, dr);
        float fl = fbHP[0].process (fbLP[0].process (dl, Svf::Mode::lp), Svf::Mode::hp);
        float fr = fbHP[1].process (fbLP[1].process (dr, Svf::Mode::lp), Svf::Mode::hp);

        if (target.pingPong)
            delay.push ((inL + inR) * 0.5f + fr * fb, fl * fb);
        else
            delay.push (inL + fl * fb, inR + fr * fb);

        fbL = dl;
        fbR = dr;
        wetL += dl * dMix;
        wetR += dr * dMix;
    }
    else
    {
        delay.push (0.0f, 0.0f);
    }

    // ---------------- sleep when everything has decayed
    tailEnergy = tailEnergy * 0.999f + (std::abs (wetL) + std::abs (wetR));
    if (! freeze && inAbs < 1.0e-7f && tailEnergy < 1.0e-5f)
    {
        if (++silentSamples > (int) sr / 2)
        {
            sleeping = true;
            reverb.reset();
            delay.reset();
            fbL = fbR = shimL = shimR = 0.0f;
        }
    }
    else
    {
        silentSamples = 0;
    }
}

// ===========================================================================
void DuckModule::prepare (double sampleRate)
{
    sr = sampleRate;
    amountSm.setTime (0.03f, sr);
    onSm.setTime (0.02f, sr);
    reset();
}

void DuckModule::reset()
{
    env = 0.0f;
    amountSm.reset (0.0f);
    onSm.reset (0.0f);
}

float DuckModule::process (float scPeak, bool enabled, float amount, float releaseMs) noexcept
{
    const float on = onSm.process (enabled ? 1.0f : 0.0f);
    const float amt = amountSm.process (amount);

    const float detect = clamp01 (scPeak * 4.0f);
    if (detect > env)
        env += (detect - env) * 0.5f;   // fast attack
    const float rel = (float) std::exp (-1.0 / (std::max (10.0f, releaseMs) * 0.001 * sr));
    env *= rel;

    if (on <= 1.0e-4f)
        return 1.0f;
    return 1.0f - amt * on * env;
}
} // namespace vk
