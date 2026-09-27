#include "Engine.h"

namespace vk
{
void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;

    time.prepare (sr, 34.0);
    pitch.prepare (sr);
    gate.prepare (sr);
    filter.prepare (sr);
    lofi.prepare (sr);
    drive.prepare (sr);
    tone.prepare (sr);
    space.prepare (sr);
    width.prepare (sr);
    duck.prepare (sr);
    dryDelay.prepare (pitch.getLatencySamples() + 8);

    activeEnv.setTime (0.010f, sr);
    mixSm.reset (sr, 0.03);
    outSm.reset (sr, 0.03);
    reset();
}

void Engine::reset()
{
    time.reset();
    pitch.reset();
    gate.reset();
    filter.reset();
    lofi.reset();
    drive.reset();
    tone.reset();
    space.reset();
    width.reset();
    duck.reset();
    dryDelay.reset();
    activeEnv.value = 0.0f;
    mixSm.setCurrentAndTargetValue (1.0f);
    outSm.setCurrentAndTargetValue (1.0f);
    expectedPpq = -1.0;
    wasPlaying = false;
    lastColumn = -1;
    colDry = colWet = 0.0f;
}

void Engine::beginBlock (const TransportInfo& t, int numSamples) noexcept
{
    const double bpm = t.bpm > 1.0 ? t.bpm : 120.0;
    samplesPerBeat = sr * 60.0 / bpm;
    blockBeatsPerSample = 1.0 / samplesPerBeat;
    beatsPerBar = std::max (1.0, (double) t.timeSigNum * 4.0 / (double) std::max (1, t.timeSigDen));

    if (t.playing && t.hasPpq)
    {
        blockPpq = t.ppq;
        if (! wasPlaying || std::abs (blockPpq - expectedPpq) > 2.0e-3)
            time.resync();    // transport start, loop or jump -> fresh, in-sync read head

        barOrigin = t.hasBarStart ? std::fmod (t.barStartPpq, beatsPerBar) : 0.0;
        if (barOrigin < 0.0) barOrigin += beatsPerBar;
    }
    else
    {
        blockPpq = internalPpq;   // free-running clock while the host is stopped
    }

    wasPlaying = t.playing && t.hasPpq;
    expectedPpq = blockPpq + numSamples * blockBeatsPerSample;
    internalPpq = expectedPpq;
    meters.bpm.store ((float) bpm, std::memory_order_relaxed);
}

void Engine::updateEffective (const EngineControls& c) noexcept
{
    const PresetData base = hasB ? morphPresets (slotA, slotB, c.morph) : slotA;
    MacroValues mv;
    mv.amount = clamp01 (c.amount * velocity);
    mv.speed = c.speed;
    mv.space = c.space;
    effective = applyMacros (base, mv);

    pitch.setParams (effective.pitch);
    gate.setParams (effective.gate);
    filter.setParams (effective.filter);
    lofi.setParams (effective.lofi);
    drive.setParams (effective.drive);
    space.setParams (effective.space);
}

void Engine::process (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>* sidechain,
                      const TransportInfo& transport, const EngineControls& controls) noexcept
{
    beginBlock (transport, io.getNumSamples());
    processRange (io, 0, io.getNumSamples(), sidechain, controls);
}

void Engine::processRange (juce::AudioBuffer<float>& io, int start, int num, const juce::AudioBuffer<float>* sc,
                           const EngineControls& c) noexcept
{
    if (num <= 0 || io.getNumChannels() < 1)
        return;

    updateEffective (c);

    float* L = io.getWritePointer (0);
    float* R = io.getNumChannels() > 1 ? io.getWritePointer (1) : nullptr;
    const float* scL = sc != nullptr && sc->getNumChannels() > 0 ? sc->getReadPointer (0) : nullptr;
    const float* scR = sc != nullptr && sc->getNumChannels() > 1 ? sc->getReadPointer (1) : nullptr;

    mixSm.setTargetValue (clamp01 (c.mix));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (c.outDb, -100.0f));

    const double latency = (double) pitch.getLatencySamples();
    const float duckAmount = effective.duck.amount > 0.0f ? effective.duck.amount : 0.7f;
    const float duckRelease = effective.duck.amount > 0.0f ? effective.duck.releaseMs : 150.0f;
    const bool duckOn = c.duck;

    TriggerContext tctx;
    tctx.lengthBeats = effective.length;
    tctx.beatsPerBar = beatsPerBar;
    tctx.barOrigin = barOrigin;
    tctx.steps = &c.steps;

    bool anyActive = false;

    for (int i = start; i < start + num; ++i)
    {
        const double ppq = blockPpq + i * blockBeatsPerSample;

        if (holdRequested != holdState)
        {
            holdState = holdRequested;
            if (holdState)
                holdStartPpq = ppq;
        }
        tctx.holdDown = holdState;
        tctx.holdStart = holdStartPpq;

        const TriggerResult tr = Trigger::evaluate (c.trigger, ppq, tctx);
        const float a = activeEnv.process (tr.active);
        anyActive = anyActive || tr.active;

        ModuleContext ctx;
        ctx.ppq = ppq;
        ctx.ppqFromOrigin = ppq - barOrigin;
        ctx.samplesPerBeat = samplesPerBeat;
        ctx.active = tr.active;
        ctx.activeEnv = a;
        ctx.segElapsed = std::max (0.0, ppq - tr.segStart);
        ctx.segLen = tr.segLen;
        ctx.progress = tr.active ? (float) std::clamp (ctx.segElapsed / std::max (1.0e-6, tr.segLen), 0.0, 1.0) : 0.0f;

        const float xl = L[i];
        const float xr = R != nullptr ? R[i] : xl;

        // latency-aligned dry
        dryDelay.push (xl, xr);
        float dl, dr;
        dryDelay.read (latency, dl, dr);

        // wet chain
        float wl = xl, wr = xr;
        TimeEngine::Frame f;
        f.active = tr.active;
        f.segStartPpq = tr.segStart;
        f.elapsedBeats = ctx.segElapsed;
        f.segLenBeats = tr.segLen;
        f.samplesPerBeat = samplesPerBeat;
        time.process (wl, wr, effective.time, f);
        pitch.process (wl, wr, ctx);
        gate.process (wl, wr, ctx);
        filter.process (wl, wr, ctx);
        lofi.process (wl, wr);
        drive.process (wl, wr);
        tone.process (wl, wr, c.tone);

        float sl, sr2;
        space.process (wl * a, wr * a, tr.active, samplesPerBeat, sl, sr2);

        float el = wl * a + sl, er = wr * a + sr2;
        width.process (el, er, effective.width);

        float pl = dl * (1.0f - a) + el;
        float pr = dr * (1.0f - a) + er;

        float scPeak = 0.0f;
        if (scL != nullptr) scPeak = std::abs (scL[i]);
        if (scR != nullptr) scPeak = std::max (scPeak, std::abs (scR[i]));
        const float dg = duck.process (scPeak, duckOn, duckAmount, duckRelease);
        pl *= dg;
        pr *= dg;

        const float m = mixSm.getNextValue();
        const float o = outSm.getNextValue();
        const float outL = (dl + (pl - dl) * m) * o;
        const float outR = (dr + (pr - dr) * m) * o;

        L[i] = outL;
        if (R != nullptr)
            R[i] = outR;

        // ---- UI meters: one bar of min/max columns
        const double barPos = std::fmod (ppq - barOrigin, beatsPerBar);
        const double norm = (barPos < 0.0 ? barPos + beatsPerBar : barPos) / beatsPerBar;
        const int col = std::clamp ((int) (norm * EngineMeters::kColumns), 0, EngineMeters::kColumns - 1);
        if (col != lastColumn)
        {
            if (lastColumn >= 0)
            {
                meters.dry[lastColumn].store (colDry, std::memory_order_relaxed);
                meters.wet[lastColumn].store (colWet, std::memory_order_relaxed);
            }
            lastColumn = col;
            colDry = colWet = 0.0f;
        }
        colDry = std::max (colDry, std::max (std::abs (dl), std::abs (dr)));
        colWet = std::max (colWet, std::max (std::abs (outL), std::abs (outR)) * a);
        if (i == start + num - 1)
        {
            meters.playhead.store ((float) norm, std::memory_order_relaxed);
            meters.currentStep.store (std::clamp ((int) (norm * kNumSteps), 0, kNumSteps - 1), std::memory_order_relaxed);
        }
    }

    meters.effectActive.store (anyActive, std::memory_order_relaxed);
}
} // namespace vk
