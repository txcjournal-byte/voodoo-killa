#pragma once

#include "ModuleCommon.h"

namespace vk
{
/**
    PITCH: low-latency rotating-delay (granular) pitch shifter + vibrato wobble + detune chorus + formant tilt.

    The module always delays the signal by a constant latency (getLatencySamples) so that switching
    presets never changes the plugin latency. With no shift the output is a sample-exact delay.
    Formant is approximated with complementary shelving filters (no pitch detection).
*/
class PitchModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const PitchParams& p) noexcept { target = p; }
    void process (float& l, float& r, const ModuleContext& ctx) noexcept;

    int getLatencySamples() const noexcept { return latency; }
    bool isShifting() const noexcept { return shiftEnv.value > 0.0f; }

private:
    double sr = 44100.0;
    StereoDelay delay;
    double grain = 1600.0, minDelay = 256.0;
    int latency = 0;

    PitchParams target;
    OnePole semisSm, blendSm, wobbleSm, detuneSm, formantSm;
    Fader shiftEnv, detuneEnv, wobbleEnv;
    ShifterVoice main, up, down;
    double wobblePhase = 0.0;
    float lastFormant = 1000.0f;
    Biquad lowShelf[2], highShelf[2];
    int formantCounter = 0;
};
} // namespace vk
