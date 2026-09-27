#pragma once

#include "Preset.h"

namespace vk
{
/**
    Morph A <-> B.
    Continuous parameters are interpolated (frequencies in log space); discrete ones (mode, type,
    trigger, pattern, length) switch at 0.5. A module that is off on one side morphs from its neutral
    setting, so e.g. "no filter" -> "LP 1800" is a smooth sweep rather than a jump.
*/
PresetData morphPresets (const PresetData& a, const PresetData& b, float t) noexcept;

/** Macro scaling (AMOUNT incl. velocity, SPEED multiplier, SPACE 0..1 with 0.5 = as designed). */
struct MacroValues
{
    float amount = 1.0f;
    float speed = 1.0f;   // 0.25 .. 4 (2 = twice as fast)
    float space = 0.5f;
};
PresetData applyMacros (const PresetData& p, const MacroValues& m) noexcept;

/** KILL / Mutate helpers (deterministic for a given seed). */
PresetData mutatePreset (const PresetData& p, uint32_t seed, float amount = 0.15f) noexcept;
} // namespace vk
