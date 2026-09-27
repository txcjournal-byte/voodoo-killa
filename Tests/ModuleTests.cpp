#include <juce_core/juce_core.h>
#include "engine/Engine.h"

using namespace vk;

namespace
{
constexpr double kSr = 44100.0;
constexpr int kBlock = 256;

/** Test signal: sine + a click on every beat. */
float testSignal (int64_t n, double bpm)
{
    const double spb = kSr * 60.0 / bpm;
    const double s = 0.35 * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) n / kSr);
    const auto inBeat = (int64_t) std::fmod ((double) n, spb);
    const double click = inBeat < 32 ? 0.5 * (1.0 - inBeat / 32.0) : 0.0;
    return (float) (s + click);
}

struct Render
{
    std::vector<float> inL, outL, outR;
};

Render renderEngine (const PresetData& preset, int numBlocks, EngineControls controls = {}, double bpm = 120.0,
                     std::function<void (TransportInfo&, int)> transportHook = {})
{
    Engine engine;
    engine.prepare (kSr, kBlock);
    engine.setSlotA (preset);
    controls.trigger = preset.trigger;

    Render r;
    juce::AudioBuffer<float> buf (2, kBlock);
    TransportInfo t;
    t.playing = true;
    t.hasPpq = true;
    t.bpm = bpm;
    int64_t n = 0;
    double ppq = 0.0;
    for (int b = 0; b < numBlocks; ++b)
    {
        for (int i = 0; i < kBlock; ++i)
        {
            const float x = testSignal (n + i, bpm);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
            r.inL.push_back (x);
        }
        t.ppq = ppq;
        if (transportHook)
            transportHook (t, b);
        engine.process (buf, nullptr, t, controls);
        for (int i = 0; i < kBlock; ++i)
        {
            r.outL.push_back (buf.getSample (0, i));
            r.outR.push_back (buf.getSample (1, i));
        }
        n += kBlock;
        ppq = t.ppq + kBlock * bpm / 60.0 / kSr;
    }
    return r;
}

int latencySamples()
{
    Engine e;
    e.prepare (kSr, kBlock);
    return e.getLatencySamples();
}
}

class ModuleTests : public juce::UnitTest
{
public:
    ModuleTests() : juce::UnitTest ("Modules / Engine", "VoodooKilla") {}

    void runTest() override
    {
        const int lat = latencySamples();

        beginTest ("latency is constant and reported");
        expect (lat > 0 && lat < (int) (kSr * 0.03), "latency " + juce::String (lat));

        beginTest ("neutral preset: output == input delayed by the latency (bit exact)");
        {
            PresetData p;
            auto r = renderEngine (p, 200);
            float err = 0.0f;
            for (size_t i = (size_t) lat; i < r.outL.size(); ++i)
                err = std::max (err, std::abs (r.outL[i] - r.inL[i - (size_t) lat]));
            expectLessThan (err, 1.0e-6f);
        }

        beginTest ("pitch -12: output is an octave down (zero-crossing rate halves)");
        {
            PresetData p;
            p.pitch.semis = -12.0f;
            Engine engine;
            engine.prepare (kSr, kBlock);
            engine.setSlotA (p);
            juce::AudioBuffer<float> buf (2, kBlock);
            TransportInfo t;
            EngineControls c;
            int crossingsIn = 0, crossingsOut = 0;
            float prevIn = 0.0f, prevOut = 0.0f;
            int64_t n = 0;
            for (int b = 0; b < 400; ++b)
            {
                for (int i = 0; i < kBlock; ++i)
                {
                    const float x = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) (n + i) / kSr);
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                    if (b > 50) { if ((x >= 0.0f) != (prevIn >= 0.0f)) ++crossingsIn; }
                    prevIn = x;
                }
                engine.process (buf, nullptr, t, c);
                for (int i = 0; i < kBlock; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (b > 50) { if ((y >= 0.0f) != (prevOut >= 0.0f)) ++crossingsOut; }
                    prevOut = y;
                }
                n += kBlock;
            }
            const double ratio = (double) crossingsOut / (double) crossingsIn;
            expectWithinAbsoluteError (ratio, 0.5, 0.05);
        }

        beginTest ("gate patterns");
        {
            GateParams g;
            parseGatePattern ("x-x-x-x-x-x-x-x-", g);
            g.rate = 0.25f;
            expectEquals (g.numSteps, 16);
            expectEquals (GateModule::patternLevel (g, 0.1), 1.0f);
            expectEquals (GateModule::patternLevel (g, 0.3), 0.0f);
            expectEquals (GateModule::patternLevel (g, 4.1), 1.0f);   // repeats every bar

            parseGatePattern ("x-x", g);       // triplet pattern cycles every 3 steps
            g.rate = 1.0f / 6.0f;
            expectEquals (GateModule::patternLevel (g, 0.5 + 0.01), 1.0f);
            expectEquals (GateModule::patternLevel (g, 1.0 / 6.0 + 0.01), 0.0f);

            parseGatePattern ("x-9-0", g);
            expectEquals (g.numSteps, 5);
            expectEquals (g.pattern[2], 1.0f);
            expectEquals (g.pattern[4], 0.0f);
            expectEquals (gatePatternToString (g), juce::String ("x-x--"));

            GateParams pump;
            pump.pump = true;
            pump.rate = 1.0f;
            expect (GateModule::patternLevel (pump, 0.01) < 0.1f);
            expect (GateModule::patternLevel (pump, 0.9) > 0.99f);
        }

        beginTest ("gated preset actually chops, and is silent on '-' steps");
        {
            PresetData p;
            parseGatePattern ("x-x-x-x-x-x-x-x-", p.gate);
            p.gate.enabled = true;
            auto r = renderEngine (p, 800);
            // step 2 of bar 3 (1/16 = 5512.5 samples @120 BPM), well inside the step, after latency
            const size_t idx = (size_t) (8.0 * 22050.0 + 5512.5 * 1.5) + (size_t) lat;
            float peak = 0.0f;
            for (size_t i = idx - 800; i < idx + 800; ++i) peak = std::max (peak, std::abs (r.outL[i]));
            expectLessThan (peak, 0.01f);
        }

        beginTest ("every module stays finite and bounded at extreme settings");
        {
            PresetData p;
            p.time.mode = TimeMode::scatter;
            p.time.rate = 0.125f;
            p.pitch.semis = 12.0f; p.pitch.wobbleCents = 50.0f; p.pitch.formant = 12.0f; p.pitch.detuneCents = 20.0f;
            parseGatePattern ("x-x-xx--", p.gate); p.gate.enabled = true;
            p.filter.type = FilterType::lp; p.filter.cutoff = 800.0f; p.filter.reso = 1.0f; p.filter.lfoDepth = 1.0f;
            p.lofi.enabled = true; p.lofi.bits = 4.0f; p.lofi.srate = 2000.0f; p.lofi.wow = 1.0f; p.lofi.flutter = 1.0f; p.lofi.noise = 1.0f;
            p.drive.type = DriveType::fuzz; p.drive.amount = 1.0f;
            p.space.reverbMix = 1.0f; p.space.shimmer = 1.0f; p.space.delayMix = 1.0f; p.space.delayFb = 0.95f; p.space.pingPong = true;
            p.width = 2.0f;
            auto r = renderEngine (p, 1500);
            bool finite = true;
            float peak = 0.0f;
            for (size_t i = 0; i < r.outL.size(); ++i)
            {
                finite = finite && std::isfinite (r.outL[i]) && std::isfinite (r.outR[i]);
                peak = std::max (peak, std::max (std::abs (r.outL[i]), std::abs (r.outR[i])));
            }
            expect (finite);
            expectLessThan (peak, 2.0f);
        }

        beginTest ("host sync: looping transport resyncs the read head");
        {
            PresetData p;
            p.time.mode = TimeMode::half;
            p.time.pitchFollow = 1;
            p.length = 4.0f;
            const double bpm = 120.0;
            const int loopBlocks = (int) std::ceil (3.0 * 22050.0 / kBlock);   // jump back every ~3 beats
            auto r = renderEngine (p, loopBlocks * 3, {}, bpm, [&] (TransportInfo& t, int block)
            {
                t.ppq = (double) (block % loopBlocks) * kBlock * bpm / 60.0 / kSr;
            });
            // right after each loop point (+ crossfade), the half-time head starts again at the segment start
            // -> output equals the latency-aligned input
            const int xf = (int) (kSr * 0.012);
            for (int loop = 1; loop < 3; ++loop)
            {
                const size_t s = (size_t) (loop * loopBlocks * kBlock);
                const size_t chk = s + (size_t) xf + (size_t) lat;
                expectWithinAbsoluteError (r.outL[chk], r.inL[s + (size_t) xf / 2], 0.2f);
            }
            bool finite = true;
            for (auto v : r.outL) finite = finite && std::isfinite (v);
            expect (finite);
        }

        beginTest ("stopped transport: free-running clock keeps effects moving");
        {
            PresetData p;
            p.time.mode = TimeMode::repeat;
            p.time.rate = 0.25f;
            p.length = 1.0f;
            auto r = renderEngine (p, 200, {}, 120.0, [] (TransportInfo& t, int) { t.playing = false; t.ppq = 0.0; });
            // repeats -> output differs from dry somewhere after the first slice
            float diff = 0.0f;
            for (size_t i = 20000; i < 40000; ++i)
                diff = std::max (diff, std::abs (r.outL[i] - r.inL[i - (size_t) lat]));
            expectGreaterThan (diff, 0.05f);
        }

        beginTest ("MIX = 0 gives the dry signal");
        {
            PresetData p;
            p.drive.type = DriveType::clip;
            p.drive.amount = 1.0f;
            EngineControls c;
            c.mix = 0.0f;
            auto r = renderEngine (p, 100, c);
            float err = 0.0f;
            for (size_t i = 5000; i < r.outL.size(); ++i)
                err = std::max (err, std::abs (r.outL[i] - r.inL[i - (size_t) lat]));
            expectLessThan (err, 1.0e-5f);
        }
    }
};

static ModuleTests moduleTests;
