#pragma once

#include "TimeBuffer.h"
#include "Trigger.h"
#include "Morph.h"
#include "Modules/Pitch.h"
#include "Modules/Gate.h"
#include "Modules/Filter.h"
#include "Modules/LoFi.h"
#include "Modules/Drive.h"
#include "Modules/Space.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace vk
{
/** Host transport snapshot for one block. */
struct TransportInfo
{
    bool   playing = false;
    bool   hasPpq = false;
    double ppq = 0.0;
    double bpm = 120.0;
    int    timeSigNum = 4, timeSigDen = 4;
    bool   hasBarStart = false;
    double barStartPpq = 0.0;
};

/** Per-block macro / global state (APVTS parameters, already read on the audio thread). */
struct EngineControls
{
    float amount = 1.0f;
    float speed = 1.0f;       // multiplier
    float tone = 0.0f;        // -12..12
    float space = 0.5f;
    float mix = 1.0f;
    float outDb = 0.0f;
    bool  duck = false;
    float morph = 0.0f;
    TriggerMode trigger = TriggerMode::always;
    std::array<bool, kNumSteps> steps {};
};

/** Lightweight real-time state for the UI (written by the audio thread, read by the message thread). */
struct EngineMeters
{
    static constexpr int kColumns = 256;
    std::atomic<float> dry[kColumns] {};
    std::atomic<float> wet[kColumns] {};
    std::atomic<float> playhead { 0.0f };   // 0..1 inside the displayed bar
    std::atomic<bool>  effectActive { false };
    std::atomic<int>   currentStep { 0 };
    std::atomic<float> bpm { 120.0f };
};

/**
    The whole DSP engine (no JUCE plugin dependencies, so it can be unit/offline tested).

    IN -> TIME -> PITCH -> GATE -> FILTER -> LOFI -> DRIVE -> TONE -> SPACE(send) -> WIDTH -> DUCK -> MIX -> OUT
    Outside the trigger window the (latency-aligned) dry signal passes; transitions are crossfaded.
*/
class Engine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Audio thread only. */
    void setSlotA (const PresetData& p) noexcept { slotA = p; }
    void setSlotB (const PresetData* p) noexcept { hasB = p != nullptr; if (p) slotB = *p; }
    void setHold (bool down) noexcept { holdRequested = down; }
    void setVelocity (float v) noexcept { velocity = v; }
    void trigger808() noexcept { duck.trigger(); }

    /** io: stereo in/out. sidechain may be null. */
    void process (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>* sidechain,
                  const TransportInfo& transport, const EngineControls& controls) noexcept;

    /** Process a sub-range (used by the processor to split blocks at MIDI events). */
    void processRange (juce::AudioBuffer<float>& io, int start, int num, const juce::AudioBuffer<float>* sidechain,
                       const EngineControls& controls) noexcept;
    void beginBlock (const TransportInfo& transport, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return pitch.getLatencySamples(); }
    const PresetData& getEffectivePreset() const noexcept { return effective; }
    EngineMeters& getMeters() noexcept { return meters; }
    double getCurrentPpq() const noexcept { return blockPpq; }

private:
    void updateEffective (const EngineControls& c) noexcept;

    double sr = 44100.0;
    int maxBlock = 512;

    PresetData slotA, slotB, effective;
    bool hasB = false;
    bool holdRequested = false, holdState = false;
    double holdStartPpq = 0.0;
    float velocity = 1.0f;

    // transport
    double blockPpq = 0.0, blockBeatsPerSample = 0.0, samplesPerBeat = 22050.0;
    double beatsPerBar = 4.0, barOrigin = 0.0;
    double internalPpq = 0.0, expectedPpq = -1.0;
    bool wasPlaying = false;
    int blockOffset = 0;

    // modules
    TimeEngine time;
    PitchModule pitch;
    GateModule gate;
    FilterModule filter;
    LoFiModule lofi;
    DriveModule drive;
    ToneModule tone;
    SpaceModule space;
    WidthModule width;
    DuckModule duck;
    StereoDelay dryDelay;

    Fader activeEnv;
    juce::SmoothedValue<float> mixSm, outSm;
    bool lastActive = false;

    EngineMeters meters;
    int lastColumn = -1;
    float colDry = 0.0f, colWet = 0.0f;
};
} // namespace vk
