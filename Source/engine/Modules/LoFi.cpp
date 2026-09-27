#include "LoFi.h"

namespace vk
{
namespace
{
    constexpr float kNeutralBits = 16.0f;
    constexpr float kNeutralRate = 48000.0f;
    constexpr float kNeutralTone = 20000.0f;
}

void LoFiModule::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* s : { &bitsSm, &srateSm, &wowSm, &flutterSm, &noiseSm, &toneSm })
        s->setTime (0.03f, sr);
    activeEnv.setTime (0.02f, sr);
    modEnv.setTime (0.03f, sr);
    delay.prepare ((int) (0.02 * sr));
    reset();
}

void LoFiModule::reset()
{
    const bool on = target.enabled;
    bitsSm.reset (on ? target.bits : kNeutralBits);
    srateSm.reset (std::log2 (on ? target.srate : kNeutralRate));
    wowSm.reset (on ? target.wow : 0.0f);
    flutterSm.reset (on ? target.flutter : 0.0f);
    noiseSm.reset (on ? target.noise : 0.0f);
    toneSm.reset (std::log2 (on ? target.tone : kNeutralTone));
    activeEnv.value = on ? 1.0f : 0.0f;
    modEnv.value = 0.0f;
    delay.reset();
    wowPhase = flutterPhase = 0.0;
    holdPhase = 1.0;
    holdL = holdR = hissL = hissR = crackle = 0.0f;
    for (auto& f : toneF) f.reset();
    lastTone = -1.0f;
}

void LoFiModule::process (float& l, float& r) noexcept
{
    const bool on = target.enabled;
    const float bits    = bitsSm.process (on ? target.bits : kNeutralBits);
    const float rate    = std::exp2 (srateSm.process (std::log2 (on ? std::max (500.0f, target.srate) : kNeutralRate)));
    const float wow     = wowSm.process (on ? target.wow : 0.0f);
    const float flutter = flutterSm.process (on ? target.flutter : 0.0f);
    const float noise   = noiseSm.process (on ? target.noise : 0.0f);
    const float tone    = std::exp2 (toneSm.process (std::log2 (on ? std::max (200.0f, target.tone) : kNeutralTone)));
    const float env = activeEnv.process (on);

    delay.push (l, r);   // always recorded so wow can fade in without stale audio
    if (env <= 0.0f && ! on)
        return;

    const float inL = l, inR = r;

    // ---- wow & flutter (modulated delay, crossfaded in/out so the base delay never jumps)
    const bool wantMod = on && (target.wow > 0.0f || target.flutter > 0.0f);
    const float me = modEnv.process (wantMod);
    if (me > 0.0f)
    {
        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * 0.25);
            driftTarget = rng.nextBipolar();
        }
        drift += (driftTarget - drift) * 0.00005f;

        wowPhase += 0.55 / sr;         wowPhase -= std::floor (wowPhase);
        flutterPhase += 6.7 / sr;      flutterPhase -= std::floor (flutterPhase);

        const double base = 0.009 * sr;
        const double wowDepth = 0.0055 * sr * wow;
        const double flutDepth = 0.00035 * sr * flutter;
        const double d = base + wowDepth * (0.75 * std::sin (kTwoPi * wowPhase) + 0.25 * drift)
                              + flutDepth * std::sin (kTwoPi * flutterPhase);
        float dl, dr;
        delay.read (d, dl, dr);
        l = l + (dl - l) * me;
        r = r + (dr - r) * me;
    }

    // ---- sample-rate reduction (sample & hold)
    if (rate < (float) sr * 0.95f && rate < 40000.0f)
    {
        holdPhase += rate / sr;
        if (holdPhase >= 1.0)
        {
            holdPhase -= std::floor (holdPhase);
            holdL = l;
            holdR = r;
        }
        l = holdL;
        r = holdR;
    }
    else
    {
        holdPhase = 1.0;
    }

    // ---- bit reduction
    if (bits < 15.95f)
    {
        const float levels = std::exp2 (std::max (1.0f, bits - 1.0f));
        l = std::round (l * levels) / levels;
        r = std::round (r * levels) / levels;
    }

    // ---- noise: hiss + crackle
    if (noise > 1.0e-4f)
    {
        const float hissAmt = noise * 0.02f;
        hissL += (rng.nextBipolar() - hissL) * 0.35f;
        hissR += (rng.nextBipolar() - hissR) * 0.35f;
        if (rng.nextFloat() < noise * 12.0f / (float) sr)
            crackle = (rng.nextFloat() * 0.6f + 0.2f) * noise * (rng.nextFloat() < 0.5f ? -1.0f : 1.0f);
        crackle *= 0.85f;
        l += hissL * hissAmt + crackle;
        r += hissR * hissAmt + crackle * 0.8f;
    }

    // ---- tone
    if (tone < 19000.0f)
    {
        if (--counter <= 0 || lastTone < 0.0f)
        {
            counter = 16;
            if (std::abs (tone - lastTone) > 1.0f)
            {
                lastTone = tone;
                toneF[0].set (tone, 0.707f, sr);
                toneF[1].set (tone, 0.707f, sr);
            }
        }
        l = toneF[0].process (l, Svf::Mode::lp);
        r = toneF[1].process (r, Svf::Mode::lp);
    }
    else if (lastTone >= 0.0f)
    {
        lastTone = -1.0f;
        toneF[0].reset();
        toneF[1].reset();
    }

    l = inL + (l - inL) * env;
    r = inR + (r - inR) * env;
}
} // namespace vk
