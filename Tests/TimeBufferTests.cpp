#include <juce_core/juce_core.h>
#include "engine/TimeBuffer.h"
#include "engine/Trigger.h"

using namespace vk;

namespace
{
constexpr double kSr = 48000.0;
constexpr double kBpm = 120.0;
constexpr double kSpb = kSr * 60.0 / kBpm; // 24000 samples per beat

struct Run
{
    std::vector<float> in, out;
    std::vector<double> offset;
    std::vector<bool> active;
    std::vector<double> segStart;
};

/** Feeds a ramp (x[n] = n) through the TIME engine, evaluating the trigger per sample. */
Run render (const TimeParams& tp, TriggerMode mode, double lengthBeats, int numSamples,
            std::function<float (int)> signal = {})
{
    TimeEngine engine;
    engine.prepare (kSr, 34.0);

    TriggerContext ctx;
    ctx.lengthBeats = lengthBeats;
    ctx.beatsPerBar = 4.0;

    Run run;
    run.in.resize ((size_t) numSamples);
    run.out.resize ((size_t) numSamples);
    run.offset.resize ((size_t) numSamples);
    run.active.resize ((size_t) numSamples);
    run.segStart.resize ((size_t) numSamples);

    for (int n = 0; n < numSamples; ++n)
    {
        const double ppq = n / kSpb;
        const auto tr = Trigger::evaluate (mode, ppq, ctx);

        TimeEngine::Frame f;
        f.active = tr.active;
        f.segStartPpq = tr.segStart;
        f.elapsedBeats = ppq - tr.segStart;
        f.segLenBeats = tr.segLen;
        f.samplesPerBeat = kSpb;

        const float x = signal ? signal (n) : (float) n;
        float l = x, r = x;
        engine.process (l, r, tp, f);
        run.in[(size_t) n] = x;
        run.out[(size_t) n] = l;
        run.offset[(size_t) n] = engine.getCurrentOffset();
        run.active[(size_t) n] = tr.active;
        run.segStart[(size_t) n] = tr.segStart;
    }
    return run;
}

const int kXfade = (int) (kSr * 0.010) + 2;
}

class TimeBufferTests : public juce::UnitTest
{
public:
    TimeBufferTests() : juce::UnitTest ("TimeBuffer / TIME module", "VoodooKilla") {}

    void runTest() override
    {
        beginTest ("mode none is bit-transparent");
        {
            TimeParams tp;
            auto run = render (tp, TriggerMode::always, 4.0, 50000);
            float maxErr = 0.0f;
            for (size_t i = 0; i < run.in.size(); ++i)
                maxErr = std::max (maxErr, std::abs (run.in[i] - run.out[i]));
            expectEquals (maxErr, 0.0f);
        }

        beginTest ("halftime (tape): reads S + e/2 and resyncs every segment");
        {
            TimeParams tp;
            tp.mode = TimeMode::half;
            tp.pitchFollow = 1;
            const double lenBeats = 1.0;
            const int L = (int) (lenBeats * kSpb);
            auto run = render (tp, TriggerMode::always, lenBeats, L * 3 + 100);

            for (int seg = 0; seg < 3; ++seg)
            {
                const int S = seg * L;
                expectWithinAbsoluteError (run.offset[(size_t) S], 0.0, 1.0e-6, "segment starts in sync");
                for (int e : { kXfade, L / 4, L / 2, L - 10 })
                    expectWithinAbsoluteError ((double) run.out[(size_t) (S + e)], S + e * 0.5, 0.02);
            }
            // segment length: exactly L samples of material consumed at half speed = L/2 source samples
            expectWithinAbsoluteError (run.offset[(size_t) (L - 1)], -(L - 1) * 0.5, 1.0);
        }

        beginTest ("quarter time: reads S + e/4");
        {
            TimeParams tp;
            tp.mode = TimeMode::quarter;
            tp.pitchFollow = 1;
            const int L = (int) (4.0 * kSpb);
            auto run = render (tp, TriggerMode::always, 4.0, L + 100);
            for (int e : { kXfade, L / 3, L - 5 })
                expectWithinAbsoluteError ((double) run.out[(size_t) e], e * 0.25, 0.02);
        }

        beginTest ("halftime (stretch, pitch kept) starts sample-identical to dry and stays finite");
        {
            TimeParams tp;
            tp.mode = TimeMode::half;
            const int L = (int) (2.0 * kSpb);
            auto sine = [] (int n) { return (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * n / kSr); };
            auto run = render (tp, TriggerMode::always, 2.0, L * 2 + 10, sine);
            expectWithinAbsoluteError (run.out[(size_t) (L + kXfade)], run.in[(size_t) (L + kXfade)], 1.0e-4f);
            float peak = 0.0f;
            bool finite = true;
            for (auto v : run.out) { peak = std::max (peak, std::abs (v)); finite = finite && std::isfinite (v); }
            expect (finite);
            expect (peak < 1.3f, "grain overlap stays near unity gain: " + juce::String (peak));
            expect (peak > 0.7f);
        }

        beginTest ("tape stop slows to zero speed and fades out at the end");
        {
            TimeParams tp;
            tp.mode = TimeMode::stop;
            tp.curve = 0.0f; // linear speed ramp
            const int L = (int) (1.0 * kSpb);
            auto run = render (tp, TriggerMode::always, 1.0, L + 10);
            // position integral of (1 - p): offset(e) = L*(1-(1-p)^2)/2 relative to S
            for (int e : { L / 4, L / 2 })
            {
                const double pr = (double) e / L;
                const double expected = L * (1.0 - (1.0 - pr) * (1.0 - pr)) / 2.0;
                const double gain = std::min (1.0, (1.0 - pr) * 4.0);
                expectWithinAbsoluteError ((double) run.out[(size_t) e], expected * gain, 0.05 * expected + 0.1);
            }
            expect (std::abs (run.out[(size_t) (L - 2)]) < 0.01f * (float) L, "silent at the end of the stop");
        }

        beginTest ("reverse plays the past backwards from the segment start");
        {
            TimeParams tp;
            tp.mode = TimeMode::reverse;
            const int L = (int) (1.0 * kSpb);
            auto run = render (tp, TriggerMode::always, 1.0, L * 3);
            const int S = L * 2;
            expectWithinAbsoluteError (run.offset[(size_t) S], 0.0, 1.0e-6);
            for (int e : { kXfade, L / 2, L - 2 })
                expectWithinAbsoluteError ((double) run.out[(size_t) (S + e)], (double) (S - e), 0.02);
        }

        beginTest ("repeat loops the first slice of the segment");
        {
            TimeParams tp;
            tp.mode = TimeMode::repeat;
            tp.rate = 0.5f; // 1/8
            const int L = (int) (1.0 * kSpb);
            const int R = (int) (0.5 * kSpb);
            auto run = render (tp, TriggerMode::always, 1.0, L * 2);
            const int S = L;
            for (int rel : { 1000, 5000, R - 100 })
            {
                expectWithinAbsoluteError ((double) run.out[(size_t) (S + rel)], (double) (S + rel), 0.02);
                expectWithinAbsoluteError ((double) run.out[(size_t) (S + R + rel)], (double) (S + rel), 0.02, "second slice repeats the first");
            }
        }

        beginTest ("ramp roll: slices get shorter over the segment");
        {
            TimeParams tp;
            tp.mode = TimeMode::repeat;
            tp.rate = 0.5f;
            tp.ramp = 1.0f;
            const int L = (int) (2.0 * kSpb);
            auto run = render (tp, TriggerMode::always, 2.0, L);
            // count slice restarts (offset jumps back towards S)
            int restartsFirstHalf = 0, restartsSecondHalf = 0;
            for (int n = 1; n < L; ++n)
                if (run.offset[(size_t) n] < run.offset[(size_t) n - 1] - 100.0)
                    (n < L / 2 ? restartsFirstHalf : restartsSecondHalf)++;
            expect (restartsSecondHalf > restartsFirstHalf, "accelerating: " + juce::String (restartsFirstHalf) + " vs " + juce::String (restartsSecondHalf));
        }

        beginTest ("double time ends in sync with the input");
        {
            TimeParams tp;
            tp.mode = TimeMode::dbl;
            tp.pitchFollow = 1;
            const int L = (int) (1.0 * kSpb);
            auto run = render (tp, TriggerMode::always, 1.0, L * 3);
            const int S = L;
            expectWithinAbsoluteError ((double) run.out[(size_t) (S + L - 2)], (double) (S + L - 4), 0.05);
        }

        beginTest ("Last Beat trigger: effect only on beat 4, dry again right after");
        {
            TimeParams tp;
            tp.mode = TimeMode::reverse;
            const int bar = (int) (4.0 * kSpb);
            auto run = render (tp, TriggerMode::lastBeat, 1.0, bar * 2 + 2000);
            for (int n = kXfade; n < 3 * (int) kSpb; n += 997)
                expectEquals (run.out[(size_t) n], run.in[(size_t) n]);
            expect (run.active[(size_t) (3 * (int) kSpb + 10)]);
            expect (! run.active[(size_t) (bar + 10)]);
            // back in sync after the crossfade following the window
            for (int n = bar + kXfade; n < bar + 20000; n += 101)
                expectWithinAbsoluteError (run.out[(size_t) n], run.in[(size_t) n], 1.0e-3f);
        }

        beginTest ("scatter is deterministic");
        {
            TimeParams tp;
            tp.mode = TimeMode::scatter;
            tp.rate = 0.25f;
            auto a = render (tp, TriggerMode::always, 2.0, 60000);
            auto b = render (tp, TriggerMode::always, 2.0, 60000);
            expect (a.out == b.out);
        }

        beginTest ("end mute silences the tail of the segment");
        {
            TimeParams tp;
            tp.mode = TimeMode::stop;
            tp.endMute = 0.5f;
            const int L = (int) (2.0 * kSpb);
            auto sine = [] (int n) { return (float) std::sin (0.05 * n); };
            auto run = render (tp, TriggerMode::always, 2.0, L, sine);
            expect (std::abs (run.out[(size_t) (L - 200)]) < 1.0e-3f);
        }
    }
};

static TimeBufferTests timeBufferTests;

// ---------------------------------------------------------------------------
class TriggerTests : public juce::UnitTest
{
public:
    TriggerTests() : juce::UnitTest ("Trigger", "VoodooKilla") {}

    void runTest() override
    {
        TriggerContext ctx;
        ctx.beatsPerBar = 4.0;

        beginTest ("Every 4 with length 4 = last bar of a 4-bar phrase");
        ctx.lengthBeats = 4.0;
        expect (! Trigger::evaluate (TriggerMode::every4, 11.9, ctx).active);
        auto r = Trigger::evaluate (TriggerMode::every4, 12.5, ctx);
        expect (r.active);
        expectEquals (r.segStart, 12.0);
        expect (! Trigger::evaluate (TriggerMode::every4, 16.1, ctx).active);

        beginTest ("Every 8 with length 2 = last two beats of an 8-bar phrase");
        ctx.lengthBeats = 2.0;
        expect (Trigger::evaluate (TriggerMode::every8, 30.5, ctx).active);
        expect (! Trigger::evaluate (TriggerMode::every8, 29.5, ctx).active);

        beginTest ("Last Beat");
        ctx.lengthBeats = 1.0;
        expect (Trigger::evaluate (TriggerMode::lastBeat, 3.2, ctx).active);
        expect (! Trigger::evaluate (TriggerMode::lastBeat, 4.2, ctx).active);
        ctx.lengthBeats = 0.5;
        expect (! Trigger::evaluate (TriggerMode::lastBeat, 3.2, ctx).active);
        expect (Trigger::evaluate (TriggerMode::lastBeat, 3.7, ctx).active);

        beginTest ("Steps: runs of consecutive steps form one segment");
        std::array<bool, kNumSteps> steps {};
        steps[2] = steps[3] = steps[4] = true;
        ctx.steps = &steps;
        auto s = Trigger::evaluate (TriggerMode::steps, 4.0 + 0.8, ctx);
        expect (s.active);
        expectEquals (s.segStart, 4.5);
        expectEquals (s.segLen, 0.75);
        expect (! Trigger::evaluate (TriggerMode::steps, 4.0 + 1.3, ctx).active);

        beginTest ("Hold: segments restart from the hold start");
        ctx.lengthBeats = 1.0;
        ctx.holdDown = true;
        ctx.holdStart = 2.3;
        auto h = Trigger::evaluate (TriggerMode::hold, 3.5, ctx);
        expect (h.active);
        expectWithinAbsoluteError (h.segStart, 3.3, 1.0e-9);
        ctx.holdDown = false;
        expect (! Trigger::evaluate (TriggerMode::hold, 3.5, ctx).active);
    }
};

static TriggerTests triggerTests;
