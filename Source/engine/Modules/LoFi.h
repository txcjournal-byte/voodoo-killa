#pragma once

#include "ModuleCommon.h"

namespace vk
{
/** LOFI: wow & flutter (modulated delay), sample-rate reduction, bit reduction, tape/vinyl noise, tone LP. */
class LoFiModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const LofiParams& p) noexcept { target = p; }
    void process (float& l, float& r) noexcept;

private:
    double sr = 44100.0;
    LofiParams target;
    OnePole bitsSm, srateSm, wowSm, flutterSm, noiseSm, toneSm;
    Fader activeEnv, modEnv;
    StereoDelay delay;
    double wowPhase = 0.0, flutterPhase = 0.0, holdPhase = 1.0;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    float holdL = 0.0f, holdR = 0.0f;
    float hissL = 0.0f, hissR = 0.0f, crackle = 0.0f;
    Svf toneF[2];
    float lastTone = -1.0f;
    int counter = 0;
    Rng rng { 0x1234567u };
};
} // namespace vk
