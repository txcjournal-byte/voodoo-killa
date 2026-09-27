#include "Trigger.h"
#include <cmath>

namespace vk
{
namespace
{
    inline double floorDiv (double x, double len) noexcept { return std::floor (x / len + 1.0e-9); }

    /** Active on the last `len` beats of every `period` beats. */
    TriggerResult lastWindow (double ppq, double origin, double period, double len) noexcept
    {
        TriggerResult r;
        len = std::min (len, period);
        const double cycle = floorDiv (ppq - origin, period);
        const double cycleStart = origin + cycle * period;
        r.segStart = cycleStart + period - len;
        r.segLen = len;
        r.active = ppq >= r.segStart - 1.0e-9;
        return r;
    }
}

double Trigger::windowLength (TriggerMode mode, double presetLength, double beatsPerBar) noexcept
{
    switch (mode)
    {
        case TriggerMode::lastBeat: return std::min (presetLength, beatsPerBar);
        default:                    return presetLength;
    }
}

TriggerResult Trigger::evaluate (TriggerMode mode, double ppq, const TriggerContext& ctx) noexcept
{
    const double bpb = std::max (1.0, ctx.beatsPerBar);
    const double len = std::max (1.0 / 64.0, ctx.lengthBeats);
    TriggerResult r;

    switch (mode)
    {
        case TriggerMode::always:
        {
            const double seg = floorDiv (ppq - ctx.barOrigin, len);
            r.active = true;
            r.segStart = ctx.barOrigin + seg * len;
            r.segLen = len;
            return r;
        }

        case TriggerMode::hold:
        {
            r.active = ctx.holdDown;
            const double since = std::max (0.0, ppq - ctx.holdStart);
            r.segStart = ctx.holdStart + floorDiv (since, len) * len;
            r.segLen = len;
            return r;
        }

        case TriggerMode::every4:   return lastWindow (ppq, ctx.barOrigin, 4.0 * bpb, len);
        case TriggerMode::every8:   return lastWindow (ppq, ctx.barOrigin, 8.0 * bpb, len);
        case TriggerMode::lastBeat: return lastWindow (ppq, ctx.barOrigin, bpb, std::min (len, bpb));

        case TriggerMode::steps:
        {
            const double stepLen = bpb / (double) kNumSteps;
            const double bar = floorDiv (ppq - ctx.barOrigin, bpb);
            const double barStart = ctx.barOrigin + bar * bpb;
            const int step = std::clamp ((int) std::floor ((ppq - barStart) / stepLen + 1.0e-9), 0, kNumSteps - 1);

            auto isOn = [&] (int i) { return ctx.steps != nullptr && (*ctx.steps)[(size_t) i]; };
            r.active = isOn (step);

            int first = step, last = step;
            if (r.active)
            {
                while (first > 0 && isOn (first - 1)) --first;
                while (last < kNumSteps - 1 && isOn (last + 1)) ++last;
            }
            r.segStart = barStart + first * stepLen;
            r.segLen = (last - first + 1) * stepLen;
            return r;
        }

        case TriggerMode::count: break;
    }
    return r;
}
} // namespace vk
