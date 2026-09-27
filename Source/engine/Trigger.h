#pragma once

#include "Preset.h"
#include <array>

namespace vk
{
/** Result of evaluating the trigger for one moment in musical time. */
struct TriggerResult
{
    bool   active   = false;
    double segStart = 0.0;   // ppq where the current segment began
    double segLen   = 4.0;   // segment length in beats
};

/** Everything the trigger needs to know besides the mode. */
struct TriggerContext
{
    double lengthBeats = 4.0;   // preset length (already scaled by SPEED)
    double beatsPerBar = 4.0;
    double barOrigin   = 0.0;   // ppq of any bar line (usually 0 or host "last bar start")
    const std::array<bool, kNumSteps>* steps = nullptr;
    bool   holdDown = false;
    double holdStart = 0.0;     // ppq where the hold began
};

/** Stateless trigger evaluation. Pure function of musical position -> safe to call per sample. */
struct Trigger
{
    static TriggerResult evaluate (TriggerMode mode, double ppq, const TriggerContext& ctx) noexcept;

    /** Default window length when a preset does not specify one explicitly. */
    static double windowLength (TriggerMode mode, double presetLength, double beatsPerBar) noexcept;
};
} // namespace vk
