#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace vk
{
// ---------------------------------------------------------------------------
// Enums shared by engine, JSON and UI
// ---------------------------------------------------------------------------
enum class TimeMode : int { none, half, quarter, dbl, stop, start, stopstart, rewind, reverse, repeat, scatter, count };
enum class FilterType : int { off, lp, hp, bp, notch, band, count };
enum class LfoShape : int { sine, sampleHold };
enum class DriveType : int { off, tape, fuzz, clip, fold, bit, count };
enum class TriggerMode : int { always, hold, every4, every8, lastBeat, steps, count };

constexpr int kMaxGateSteps = 32;
constexpr int kNumSteps     = 16;
constexpr int kNumCategories = 12;
constexpr int kPresetsPerCategory = 8;

// ---------------------------------------------------------------------------
// Module parameters. Everything is plain-old-data so a whole preset can be
// copied through a lock-free FIFO to the audio thread.
// Beat values are in quarter notes (1 = 1/4, 0.5 = 1/8, 0.25 = 1/16 ...).
// ---------------------------------------------------------------------------
struct TimeParams
{
    TimeMode mode = TimeMode::none;
    float rate  = 0.5f;     // repeat / scatter / alternate slice (beats)
    float curve = 0.5f;     // stop/start shape: 0 = linear, 1 = exponential tape
    float ramp  = 0.0f;     // repeat: accelerating roll (0..1)
    float speed = 1.0f;     // reverse playback speed (0.5 = reverse + halftime)
    int   pitchFollow = -1; // -1 = mode default, 0 = time-stretch (pitch kept), 1 = tape (pitch follows speed)
    bool  alternate = false;// reverse: alternate fwd/rev slices, repeat: odd repeats reversed
    float endMute = 0.0f;   // mute the last N beats of the segment
    float mix = 1.0f;       // amount of time-manipulated signal (AMOUNT macro)
};

struct PitchParams
{
    float semis = 0.0f;       // -24..+24
    float fine = 0.0f;        // cents
    float endDrop = 0.0f;     // semitones reached at the end of the last beat of the segment
    float ramp = 0.0f;        // semitones reached at the end of the segment (riser)
    float wobbleCents = 0.0f;
    float wobbleHz = 0.5f;
    float formant = 0.0f;     // -12..+12
    float blend = 1.0f;       // shifted voice level vs. unshifted
    float detuneCents = 0.0f; // two detuned voices (chorus)
};

struct GateParams
{
    bool  enabled = false;
    int   numSteps = 0;
    std::array<float, kMaxGateSteps> pattern {}; // 0..1 level per step
    float rate = 0.25f;       // beats per step
    float depth = 1.0f;
    float smoothMs = 3.0f;
    bool  pump = false;       // smooth sidechain-pump shape instead of steps
};

struct FilterParams
{
    FilterType type = FilterType::off;
    float cutoff = 1000.0f;   // Hz (band: low edge)
    float cutoff2 = 3000.0f;  // Hz (band: high edge)
    float reso = 0.1f;        // 0..1
    float lfoRate = 1.0f;     // beats per cycle
    float lfoDepth = 0.0f;    // 0..1 (±4 octaves at 1)
    LfoShape lfoShape = LfoShape::sine;
    float sweepFrom = 0.0f;   // Hz, 0 = no sweep
    float sweepTo = 0.0f;
    float mix = 1.0f;
};

struct LofiParams
{
    bool  enabled = false;
    float bits = 16.0f;       // 4..16
    float srate = 44100.0f;   // Hz
    float wow = 0.0f;
    float flutter = 0.0f;
    float noise = 0.0f;
    float tone = 20000.0f;    // LP Hz
};

struct DriveParams
{
    DriveType type = DriveType::off;
    float amount = 0.0f;
    float postLP = 20000.0f;
};

struct SpaceParams
{
    float reverbSize = 0.5f;
    float reverbMix = 0.0f;
    float shimmer = 0.0f;
    bool  freeze = false;
    float delayBeats = 0.5f;
    float delayFb = 0.0f;
    bool  pingPong = false;
    float delayMix = 0.0f;
};

struct DuckParams
{
    float amount = 0.0f;
    float releaseMs = 150.0f;
};

struct PresetData
{
    TriggerMode trigger = TriggerMode::always;
    float length = 4.0f;      // segment length in beats
    TimeParams   time;
    PitchParams  pitch;
    GateParams   gate;
    FilterParams filter;
    LofiParams   lofi;
    DriveParams  drive;
    SpaceParams  space;
    float        width = 1.0f; // 0..2
    DuckParams   duck;
};

/** Full preset (message thread only: contains Strings). */
struct PresetInfo
{
    juce::String id, name, desc, category, photo;
    int categoryIndex = 0;
    int indexInCategory = 0;
    bool factory = true;
    PresetData data;
};

struct CategoryInfo
{
    juce::String id, name, desc, colour;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
/** Parses "x-x-", digits 0-9 = level. Returns number of steps. */
int parseGatePattern (const juce::String& text, GateParams& gate);
juce::String gatePatternToString (const GateParams& gate);

/** Parses rate strings: "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/4D", or a plain beat count. */
float parseRate (const juce::String& text, float fallback);
juce::String rateToString (float beats);

const char* timeModeName (TimeMode);
TimeMode timeModeFromName (const juce::String&);
const char* filterTypeName (FilterType);
FilterType filterTypeFromName (const juce::String&);
const char* driveTypeName (DriveType);
DriveType driveTypeFromName (const juce::String&);
const char* triggerName (TriggerMode);        // JSON ids ("always", "every4"...)
const char* triggerLabel (TriggerMode);       // UI labels ("Always", "Every 4"...)
TriggerMode triggerFromName (const juce::String&);

/** Default for pitchFollow when the preset leaves it unset. */
bool timeModePitchFollowDefault (TimeMode) noexcept;

/** JSON (de)serialisation of a single preset. */
juce::var presetDataToVar (const PresetData&);
PresetData presetDataFromVar (const juce::var&);
juce::var presetInfoToVar (const PresetInfo&);
PresetInfo presetInfoFromVar (const juce::var&);

/** Neutral module check helpers (used for bypass and morphing). */
bool timeActive   (const PresetData&) noexcept;
bool pitchActive  (const PresetData&) noexcept;
bool filterActive (const PresetData&) noexcept;
bool lofiActive   (const PresetData&) noexcept;
bool driveActive  (const PresetData&) noexcept;
bool spaceActive  (const PresetData&) noexcept;
} // namespace vk
