#include <juce_core/juce_core.h>
#include "engine/Engine.h"
#include "engine/PresetLibrary.h"

using namespace vk;

/**
    Stress / fuzz test: random presets, morphs, macros, triggers, block sizes, sample rates, tempo changes,
    loops, stops and MIDI-style hold changes. Fails on NaN/Inf, runaway levels or a block that takes longer
    than its own real-time duration (which would stall the host).
*/
class StressTests : public juce::UnitTest
{
public:
    StressTests() : juce::UnitTest ("Stress / fuzz", "VoodooKilla") {}

    void runTest() override
    {
        PresetLibrary lib;
        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        const int blocks[] = { 1, 7, 64, 256, 1024, 4096 };

        for (int run = 0; run < 8; ++run)
        {
            Rng rng (1234u + (uint32_t) run);
            const double sr = rates[run % 4];
            const int maxBlock = 4096;
            beginTest ("fuzz run " + juce::String (run) + " @ " + juce::String (sr) + " Hz");

            Engine e;
            e.prepare (sr, maxBlock);
            juce::AudioBuffer<float> buf (2, maxBlock), sc (2, maxBlock);

            TransportInfo t;
            t.hostTransport = true;
            t.playing = true;
            t.hasPpq = true;
            t.bpm = 140.0;
            double ppq = 0.0;
            EngineControls c;
            bool finite = true;
            float peak = 0.0f;
            double worstRatio = 0.0;
            double processedSeconds = 0.0;

            while (processedSeconds < 6.0)
            {
                // random events
                if (rng.nextFloat() < 0.05f) e.setSlotA (lib.getFactory()[(size_t) rng.nextInt (96)].data);
                if (rng.nextFloat() < 0.03f)
                {
                    const auto& b = lib.getFactory()[(size_t) rng.nextInt (96)].data;
                    e.setSlotB (rng.nextFloat() < 0.8f ? &b : nullptr);
                }
                if (rng.nextFloat() < 0.1f) c.morph = rng.nextFloat();
                if (rng.nextFloat() < 0.05f) c.amount = rng.nextFloat();
                if (rng.nextFloat() < 0.05f) c.speed = std::pow (2.0f, (float) (rng.nextInt (5) - 2));
                if (rng.nextFloat() < 0.05f) c.tone = rng.nextBipolar() * 12.0f;
                if (rng.nextFloat() < 0.05f) c.space = rng.nextFloat();
                if (rng.nextFloat() < 0.05f) c.duck = rng.nextFloat() < 0.5f;
                if (rng.nextFloat() < 0.05f) c.trigger = (TriggerMode) rng.nextInt ((int) TriggerMode::count);
                if (rng.nextFloat() < 0.05f) for (auto& s : c.steps) s = rng.nextFloat() < 0.5f;
                if (rng.nextFloat() < 0.05f) e.setHold (rng.nextFloat() < 0.5f);
                if (rng.nextFloat() < 0.02f) t.bpm = 40.0 + rng.nextFloat() * 260.0;
                if (rng.nextFloat() < 0.02f) ppq = rng.nextFloat() * 64.0;          // jump / loop
                if (rng.nextFloat() < 0.02f) t.playing = ! t.playing;               // stop / start
                if (rng.nextFloat() < 0.02f) e.trigger808();

                const int n = blocks[rng.nextInt (6)];
                for (int i = 0; i < n; ++i)
                {
                    const float x = rng.nextFloat() < 0.001f ? 1.0f : 0.3f * std::sin ((float) i * 0.05f) + 0.05f * rng.nextBipolar();
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x * 0.9f);
                    sc.setSample (0, i, rng.nextFloat() < 0.01f ? 0.8f : 0.0f);
                    sc.setSample (1, i, 0.0f);
                }
                juce::AudioBuffer<float> io (buf.getArrayOfWritePointers(), 2, n);
                juce::AudioBuffer<float> side (sc.getArrayOfWritePointers(), 2, n);
                t.ppq = ppq;

                const auto t0 = juce::Time::getHighResolutionTicks();
                e.process (io, &side, t, c);
                const double took = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                const double budget = n / sr;
                if (n >= 256)
                    worstRatio = std::max (worstRatio, took / budget);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = io.getSample (ch, i);
                        finite = finite && std::isfinite (v);
                        peak = std::max (peak, std::abs (v));
                    }

                if (t.playing)
                    ppq += n * t.bpm / 60.0 / sr;
                processedSeconds += budget;
            }

            expect (finite, "NaN/Inf");
            expectLessThan (peak, 4.0f, "runaway level");
            // a block must never take longer than 60 % of its real-time budget (host would stall)
            expectLessThan (worstRatio, 0.6, "slowest block used " + juce::String (worstRatio * 100.0, 1) + " % of real time");
            logMessage ("  peak " + juce::String (peak, 2) + ", slowest block " + juce::String (worstRatio * 100.0, 1) + " % of real time");
        }

        beginTest ("stopped host transport: silence in = silence out, no replayed audio");
        {
            Engine e;
            e.prepare (44100.0, 512);
            e.setSlotA (lib.get (lib.findById ("glitchritval_2"))->data);   // scatter
            EngineControls c;
            c.trigger = TriggerMode::always;
            TransportInfo t;
            t.hostTransport = true;
            t.playing = true;
            t.hasPpq = true;
            juce::AudioBuffer<float> buf (2, 512);
            double ppq = 0.0;
            for (int b = 0; b < 200; ++b)   // play some music
            {
                for (int i = 0; i < 512; ++i) { buf.setSample (0, i, 0.5f * std::sin ((float) (b * 512 + i) * 0.03f)); buf.setSample (1, i, buf.getSample (0, i)); }
                t.ppq = ppq;
                e.process (buf, nullptr, t, c);
                ppq += 512.0 * 2.0 / 44100.0;
            }
            t.playing = false;               // stop: host sends silence
            float peak = 0.0f;
            for (int b = 0; b < 400; ++b)
            {
                buf.clear();
                e.process (buf, nullptr, t, c);
                if (b > 20)                  // after the 10 ms crossfade and latency
                    peak = std::max (peak, buf.getMagnitude (0, 512));
            }
            expectLessThan (peak, 1.0e-4f, "plugin keeps sounding while stopped");
        }

        beginTest ("lo-fi noise stays silent on silent input");
        {
            Engine e;
            e.prepare (44100.0, 512);
            e.setSlotA (lib.get (lib.findById ("cassettecvlt_7"))->data);   // pirate radio: noise 0.4
            EngineControls c;
            TransportInfo t;
            t.hostTransport = true;
            t.playing = true;
            t.hasPpq = true;
            juce::AudioBuffer<float> buf (2, 512);
            float peak = 0.0f;
            for (int b = 0; b < 300; ++b)
            {
                buf.clear();
                t.ppq = b * 512.0 * 2.0 / 44100.0;
                e.process (buf, nullptr, t, c);
                if (b > 100)
                    peak = std::max (peak, buf.getMagnitude (0, 512));
            }
            expectLessThan (peak, 1.0e-4f);
        }
    }
};

static StressTests stressTests;
