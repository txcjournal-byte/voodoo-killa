#pragma once

#include "ModuleCommon.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace vk
{
/**
    SPACE: reverb (with freeze and +12 st shimmer feedback) and a tempo-synced (ping-pong) delay.
    Works as a send: process() returns only the wet signal so tails keep ringing after the trigger window.
*/
class SpaceModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const SpaceParams& p) noexcept { target = p; }

    /** in = send input (already gated by the trigger). Returns wet only. */
    void process (float inL, float inR, bool active, double samplesPerBeat, float& wetL, float& wetR) noexcept;

private:
    double sr = 44100.0;
    SpaceParams target;
    juce::Reverb reverb;
    juce::Reverb::Parameters revParams;
    bool lastFreeze = false;
    float lastSize = -1.0f;

    OnePole revMixSm, shimmerSm, delayMixSm, fbSm, delayTimeSm;
    StereoDelay delay;
    StereoDelay shimmerBuf;
    ShifterVoice shimmerVoice;
    double shimmerGrain = 2000.0, shimmerMin = 64.0;
    float shimL = 0.0f, shimR = 0.0f;
    float fbL = 0.0f, fbR = 0.0f;
    Svf fbLP[2], fbHP[2];
    float tailEnergy = 0.0f;
    int silentSamples = 0;
    bool sleeping = true;
};

/** 808 DUCK: ducks the output from the host sidechain or from MIDI "808" notes. */
class DuckModule
{
public:
    void prepare (double sampleRate);
    void reset();
    /** Call when an 808 MIDI note starts. */
    void trigger() noexcept { env = 1.0f; }
    /** Returns the gain to apply (1 = no ducking). */
    float process (float sidechainPeak, bool enabled, float amount, float releaseMs) noexcept;

private:
    double sr = 44100.0;
    float env = 0.0f;
    OnePole amountSm, onSm;
};
} // namespace vk
