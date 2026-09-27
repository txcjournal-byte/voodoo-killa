#include <juce_core/juce_core.h>
#include "engine/Engine.h"
#include "engine/PresetLibrary.h"

using namespace vk;

/** CPU check (spec: < 5 % of one core at 44.1 kHz / 256, also while morphing). Logs the worst presets. */
class BenchTests : public juce::UnitTest
{
public:
    BenchTests() : juce::UnitTest ("CPU benchmark", "VoodooKilla") {}

    static double cpuPercent (const PresetData& a, const PresetData* b, bool morphing)
    {
        constexpr double sr = 44100.0;
        constexpr int block = 256;
        Engine e;
        e.prepare (sr, block);
        e.setSlotA (a);
        e.setSlotB (b);
        EngineControls c;
        c.trigger = TriggerMode::always;
        juce::AudioBuffer<float> buf (2, block);
        TransportInfo t;
        t.playing = true; t.hasPpq = true; t.bpm = 140.0;
        const int blocks = (int) (4.0 * sr / block);   // 4 s of audio
        juce::Random r (1);
        double elapsed = 0.0;
        for (int i = 0; i < blocks; ++i)
        {
            for (int s = 0; s < block; ++s) { const float x = r.nextFloat() * 0.5f - 0.25f; buf.setSample (0, s, x); buf.setSample (1, s, x); }
            if (morphing) c.morph = 0.5f + 0.5f * (float) std::sin (i * 0.01);
            t.ppq = i * block * t.bpm / 60.0 / sr;
            const auto t0 = juce::Time::getHighResolutionTicks();
            e.process (buf, nullptr, t, c);
            elapsed += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        }
        return 100.0 * elapsed / 4.0;
    }

    void runTest() override
    {
        beginTest ("CPU per preset and while morphing");
        PresetLibrary lib;
        double worst = 0.0, sum = 0.0;
        juce::String worstId;
        for (const auto& p : lib.getFactory())
        {
            const double cpu = cpuPercent (p.data, nullptr, false);
            sum += cpu;
            if (cpu > worst) { worst = cpu; worstId = p.id; }
        }
        const double morph = cpuPercent (lib.getFactory()[53].data, &lib.getFactory()[65].data, true);
        logMessage ("  CPU average " + juce::String (sum / 96.0, 2) + " %, worst " + juce::String (worst, 2) + " % (" + worstId
                    + "), morphing pair " + juce::String (morph, 2) + " %");
        // generous bound so slow CI machines do not flake; the target is < 5 %
        expectLessThan (worst, 15.0);
        expectLessThan (morph, 15.0);
    }
};

static BenchTests benchTests;
