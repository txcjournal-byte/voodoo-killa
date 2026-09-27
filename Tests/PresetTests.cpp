#include <juce_core/juce_core.h>
#include "engine/Engine.h"
#include "engine/PresetLibrary.h"
#include <set>
#include <cstdlib>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace vk;

namespace
{
constexpr double kSr = 44100.0;
constexpr int kBlock = 256;
constexpr double kBpm = 120.0;

float testSignal (int64_t n)
{
    const double spb = kSr * 60.0 / kBpm;
    const double s = 0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) n / kSr);
    const auto inBeat = (int64_t) std::fmod ((double) n, spb);
    const double click = inBeat < 64 ? 0.5 * (1.0 - inBeat / 64.0) : 0.0;
    return (float) (s + click);
}

struct OfflineResult
{
    bool finite = true;
    float peak = 0.0f;
    float maxSyncError = 0.0f;   // |out - dry| after the trigger window closed (+ 25 ms)
    int syncChecks = 0;
};

OfflineResult renderOffline (const PresetData& a, const PresetData* b, float morph, int bars, bool checkSync)
{
    Engine engine;
    engine.prepare (kSr, kBlock);
    engine.setSlotA (a);
    engine.setSlotB (b);

    EngineControls c;
    c.morph = morph;
    c.trigger = b != nullptr && morph >= 0.5f ? b->trigger : a.trigger;
    if (c.trigger == TriggerMode::hold)
        engine.setHold (true);

    const int lat = engine.getLatencySamples();
    const int total = (int) (bars * 4 * kSr * 60.0 / kBpm);
    const int settle = (int) (0.025 * kSr);

    TransportInfo t;
    t.playing = true;
    t.hasPpq = true;
    t.bpm = kBpm;

    OfflineResult r;
    std::vector<float> dry ((size_t) total + kBlock), active ((size_t) total + kBlock);
    juce::AudioBuffer<float> buf (2, kBlock);
    int inactiveRun = 0;

    for (int64_t n = 0; n < total; n += kBlock)
    {
        for (int i = 0; i < kBlock; ++i)
        {
            const float x = testSignal (n + i);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
            dry[(size_t) (n + i)] = x;
        }
        t.ppq = (double) n / (kSr * 60.0 / kBpm);
        engine.process (buf, nullptr, t, c);

        TriggerContext tctx;
        tctx.lengthBeats = engine.getEffectivePreset().length;
        tctx.beatsPerBar = 4.0;
        tctx.holdDown = true;
        for (int i = 0; i < kBlock; ++i)
        {
            const float l = buf.getSample (0, i), rr = buf.getSample (1, i);
            r.finite = r.finite && std::isfinite (l) && std::isfinite (rr);
            r.peak = std::max (r.peak, std::max (std::abs (l), std::abs (rr)));

            if (checkSync)
            {
                const double ppq = t.ppq + i / (kSr * 60.0 / kBpm);
                const bool on = Trigger::evaluate (c.trigger, ppq, tctx).active;
                inactiveRun = on ? 0 : inactiveRun + 1;
                const auto idx = n + i;
                if (inactiveRun > settle && idx >= lat && idx > (int64_t) (kSr * 0.5))
                {
                    r.maxSyncError = std::max (r.maxSyncError, std::abs (l - dry[(size_t) (idx - lat)]));
                    ++r.syncChecks;
                }
            }
        }
    }
    return r;
}
}

class PresetTests : public juce::UnitTest
{
public:
    PresetTests() : juce::UnitTest ("Presets / Morph / KILL", "VoodooKilla") {}

    void runTest() override
    {
        PresetLibrary lib;

        beginTest ("factory.json: 12 categories x 8 presets");
        {
            expectEquals ((int) lib.getCategories().size(), kNumCategories);
            expectEquals ((int) lib.getFactory().size(), kNumCategories * kPresetsPerCategory);
            juce::StringArray ids;
            for (int c = 0; c < kNumCategories; ++c)
            {
                for (int i = 0; i < kPresetsPerCategory; ++i)
                {
                    const auto& p = lib.getFactory()[(size_t) lib.factoryIndex (c, i)];
                    expectEquals (p.categoryIndex, c);
                    expectEquals (p.indexInCategory, i);
                    expect (p.name.isNotEmpty() && p.desc.isNotEmpty());
                    expectEquals (p.photo, lib.getCategories()[(size_t) c].id + "_" + juce::String (i + 1) + ".jpg");
                    expect (! ids.contains (p.id), "duplicate id " + p.id);
                    ids.add (p.id);
                }
            }
            expectEquals (lib.getFactory()[0].name, juce::String ("Halfcut"));
            expectEquals (lib.getFactory()[1].name, juce::String ("Slow Grave"));
            expectEquals (lib.getFactory()[95].name, juce::String ("No Cap Drop"));
        }

        beginTest ("preset contents follow the table");
        {
            auto get = [&] (const char* id) { return lib.get (lib.findById (id))->data; };
            auto slowGrave = get ("halfcut_2");
            expect (slowGrave.time.mode == TimeMode::half);
            expect (slowGrave.filter.type == FilterType::lp);
            expectWithinAbsoluteError (slowGrave.filter.cutoff, 1800.0f, 0.1f);

            auto flat = get ("deadtape_1");
            expect (flat.time.mode == TimeMode::stop && flat.trigger == TriggerMode::every4);
            expectWithinAbsoluteError (flat.time.curve, 0.7f, 1.0e-4f);

            auto triplet = get ("glitchritval_5");
            expectWithinAbsoluteError (triplet.time.rate, 1.0f / 3.0f, 1.0e-4f);

            auto chopped = get ("chopped_1");
            expect (chopped.gate.enabled && chopped.gate.numSteps == 16);

            auto mainChar = get ("aura_5");
            expectWithinAbsoluteError (mainChar.space.delayBeats, 1.5f, 1.0e-4f);
            expect (mainChar.space.pingPong);

            auto dropKill = get ("lockin_5");
            expect (dropKill.gate.enabled && dropKill.gate.pattern[0] == 0.0f && dropKill.trigger == TriggerMode::every4);

            expect (get ("villainera_8").trigger == TriggerMode::hold);
            expect (get ("lockin_6").duck.amount > 0.8f);
        }

        beginTest ("JSON round trip is lossless for every preset");
        {
            for (const auto& p : lib.getFactory())
            {
                const auto again = presetDataFromVar (juce::JSON::parse (juce::JSON::toString (presetDataToVar (p.data))));
                const auto s1 = juce::JSON::toString (presetDataToVar (p.data));
                const auto s2 = juce::JSON::toString (presetDataToVar (again));
                expectEquals (s2, s1, p.id);
            }
        }

        beginTest ("morph: 0 = A, 1 = B, discrete switch at 0.5");
        {
            const auto& a = lib.get (lib.findById ("villainera_1"))->data;  // LP 1500 + reverb
            const auto& b = lib.get (lib.findById ("aura_3"))->data;        // HP 400 + shimmer
            const auto m0 = morphPresets (a, b, 0.0f);
            const auto m1 = morphPresets (a, b, 1.0f);
            expect (m0.filter.type == a.filter.type);
            expectWithinAbsoluteError (m0.filter.cutoff, a.filter.cutoff, 0.01f);
            expectWithinAbsoluteError (m0.space.reverbMix, a.space.reverbMix, 1.0e-6f);
            expect (m1.filter.type == b.filter.type);
            expectWithinAbsoluteError (m1.filter.cutoff, b.filter.cutoff, 0.01f);
            expectWithinAbsoluteError (m1.space.shimmer, b.space.shimmer, 1.0e-6f);
            expect (morphPresets (a, b, 0.49f).filter.type == a.filter.type);
            expect (morphPresets (a, b, 0.51f).filter.type == b.filter.type);

            // continuous interpolation in between (log for frequencies)
            const auto& c = lib.get (lib.findById ("villainera_3"))->data;  // pitch -5, LP 2000
            const auto mid = morphPresets (a, c, 0.5f);
            expectWithinAbsoluteError (mid.filter.cutoff, std::sqrt (1500.0f * 2000.0f), 0.5f);
            expectWithinAbsoluteError (mid.pitch.semis, -2.5f, 1.0e-5f);

            // trigger / time mode are discrete
            const auto& h = lib.get (lib.findById ("halfcut_3"))->data;     // quarter, every4
            const auto& r = lib.get (lib.findById ("backmask_1"))->data;    // reverse, last beat
            expect (morphPresets (h, r, 0.4f).trigger == TriggerMode::every4);
            expect (morphPresets (h, r, 0.6f).trigger == TriggerMode::lastBeat);
            expect (morphPresets (h, r, 0.4f).time.mode == TimeMode::quarter);
            expect (morphPresets (h, r, 0.6f).time.mode == TimeMode::reverse);

            // module that is off on one side fades from neutral
            const auto& gate = lib.get (lib.findById ("chopped_1"))->data;
            const auto& pitchOnly = lib.get (lib.findById ("diabolvs_1"))->data;
            const auto g = morphPresets (pitchOnly, gate, 0.25f);
            expect (g.gate.enabled);
            expectWithinAbsoluteError (g.gate.depth, 0.25f, 1.0e-5f);
        }

        beginTest ("KILL is deterministic with a seed and respects the category lock");
        {
            for (uint32_t seed : { 1u, 42u, 1234567u, 0xdeadbeefu })
            {
                expectEquals (lib.randomIndex (seed, -1, 0), lib.randomIndex (seed, -1, 0));
                const int locked = lib.randomIndex (seed, 5, -1);
                expect (locked >= 5 * 8 && locked < 6 * 8);
                expect (lib.randomIndex (seed, 5, 40) != 40);
            }
            std::set<int> seen;
            for (uint32_t s = 1; s < 400; ++s)
                seen.insert (lib.randomIndex (s, -1, -1));
            expect (seen.size() > 80, "covers the library: " + juce::String ((int) seen.size()));

            const auto& base = lib.getFactory()[5].data;
            const auto m1 = mutatePreset (base, 99u);
            const auto m2 = mutatePreset (base, 99u);
            expectEquals (juce::JSON::toString (presetDataToVar (m1)), juce::JSON::toString (presetDataToVar (m2)));
            expect (std::abs (m1.space.reverbMix - base.space.reverbMix) <= base.space.reverbMix * 0.1501f);
        }

        beginTest ("offline render: all 96 presets (no NaN/Inf, < +6 dBFS, back in sync after the window)");
        {
            for (const auto& p : lib.getFactory())
            {
                const bool hasTail = spaceActive (p.data) || p.data.duck.amount > 0.0f;
                const double period = p.data.trigger == TriggerMode::every4 ? 16.0 : p.data.trigger == TriggerMode::every8 ? 32.0 : 4.0;
                const bool checkSync = ! hasTail && p.data.trigger != TriggerMode::always && p.data.trigger != TriggerMode::hold
                                       && p.data.length < period;
                const auto r = renderOffline (p.data, nullptr, 0.0f, 9, checkSync);
                if (std::getenv ("VK_VERBOSE") != nullptr)
                    logMessage ("  " + p.id.paddedRight (' ', 16) + " peak " + juce::String (juce::Decibels::gainToDecibels (r.peak), 1) + " dBFS");
                expect (r.finite, p.id + " produced NaN/Inf");
                expectLessThan (r.peak, 2.0f, p.id + " peak");
                if (checkSync)
                {
                    expect (r.syncChecks > 1000, p.id + " had no inactive region");
                    expectLessThan (r.maxSyncError, 1.0e-4f, p.id + " not back in sync");
                }
            }
        }

        beginTest ("offline render: 10 random A/B pairs at morph 0.5");
        {
            Rng rng (2024u);
            for (int i = 0; i < 10; ++i)
            {
                const auto& a = lib.getFactory()[(size_t) rng.nextInt (96)];
                const auto& b = lib.getFactory()[(size_t) rng.nextInt (96)];
                const auto r = renderOffline (a.data, &b.data, 0.5f, 5, false);
                expect (r.finite, a.id + " x " + b.id + " NaN/Inf");
                expectLessThan (r.peak, 2.0f, a.id + " x " + b.id + " peak");
            }
        }
    }
};

static PresetTests presetTests;
