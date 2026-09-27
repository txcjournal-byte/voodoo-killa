#include "Pitch.h"

namespace vk
{
void PitchModule::prepare (double sampleRate)
{
    sr = sampleRate;
    grain = std::round (0.036 * sr);
    minDelay = std::round (0.006 * sr);        // headroom for vibrato
    latency = (int) (minDelay + grain * 0.5);
    delay.prepare ((int) (minDelay * 2 + grain * 2 + 16));

    semisSm.setTime (0.03f, sr);
    blendSm.setTime (0.03f, sr);
    wobbleSm.setTime (0.05f, sr);
    detuneSm.setTime (0.05f, sr);
    formantSm.setTime (0.05f, sr);
    shiftEnv.setTime (0.03f, sr);
    detuneEnv.setTime (0.05f, sr);
    wobbleEnv.setTime (0.05f, sr);
    reset();
}

void PitchModule::reset()
{
    delay.reset();
    semisSm.reset (target.semis + target.fine * 0.01f);
    blendSm.reset (target.blend);
    wobbleSm.reset (target.wobbleCents);
    detuneSm.reset (target.detuneCents);
    formantSm.reset (target.formant);
    shiftEnv.value = 0.0f;
    detuneEnv.value = 0.0f;
    wobbleEnv.value = 0.0f;
    main.resetPhase();
    up.resetPhase (0.25);
    down.resetPhase (0.75);
    wobblePhase = 0.0;
    lastFormant = 1000.0f;
    for (auto& b : lowShelf) b.reset();
    for (auto& b : highShelf) b.reset();
}

void PitchModule::process (float& l, float& r, const ModuleContext& ctx) noexcept
{
    delay.push (l, r);

    // ---- semitone target: static + riser ramp + end-of-segment drop
    float st = semisSm.process (target.semis + target.fine * 0.01f);
    if (ctx.active)
    {
        st += target.ramp * ctx.progress;
        if (target.endDrop != 0.0f)
        {
            const double remain = ctx.segLen - ctx.segElapsed;
            const double window = std::min (1.0, ctx.segLen);
            const float q = (float) std::clamp (1.0 - remain / window, 0.0, 1.0);
            st += target.endDrop * q * q;
        }
    }

    const bool wantShift = std::abs (st) > 1.0e-3f;
    const float se = shiftEnv.process (wantShift);
    if (se <= 0.0f)
        main.resetPhase();

    // ---- vibrato (delay modulation, depth converted from cents)
    const float wobC = wobbleSm.process (target.wobbleCents);
    const float we = wobbleEnv.process (target.wobbleCents > 0.0f);
    double wob = 0.0;
    if (we > 0.0f)
    {
        const double hz = std::max (0.05f, target.wobbleHz);
        wobblePhase += hz / sr;
        wobblePhase -= std::floor (wobblePhase);
        const double amp = (std::pow (2.0, wobC / 1200.0) - 1.0) * sr / (kTwoPi * hz);
        wob = std::min (minDelay - 4.0, amp) * std::sin (kTwoPi * wobblePhase) * we;
    }

    const double centre = minDelay + grain * 0.5;
    float pl, pr;
    delay.read (centre + wob, pl, pr);

    float ol = pl, orr = pr;
    if (se > 0.0f)
    {
        float sl, sr2;
        main.process (delay, std::pow (2.0, st / 12.0), grain, minDelay, wob, sl, sr2);
        const float b = blendSm.process (target.blend);
        const float wl = pl * (1.0f - b) + sl * b;
        const float wr = pr * (1.0f - b) + sr2 * b;
        ol = pl + (wl - pl) * se;
        orr = pr + (wr - pr) * se;
    }

    // ---- detune chorus: +c on the left, -c on the right
    const float dc = detuneSm.process (target.detuneCents);
    const float de = detuneEnv.process (target.detuneCents > 0.0f);
    if (de > 0.0f)
    {
        float ul, ur, dl, dr;
        up.process (delay, std::pow (2.0, dc / 1200.0), grain, minDelay, wob, ul, ur);
        down.process (delay, std::pow (2.0, -dc / 1200.0), grain, minDelay, wob, dl, dr);
        ol = ol * (1.0f - 0.35f * de) + (ul * 0.8f + dl * 0.2f) * 0.55f * de;
        orr = orr * (1.0f - 0.35f * de) + (dr * 0.8f + ur * 0.2f) * 0.55f * de;
    }
    else
    {
        up.resetPhase (0.25);
        down.resetPhase (0.75);
    }

    // ---- formant approximation (complementary shelves)
    const float fm = formantSm.process (target.formant);
    if (std::abs (fm) > 0.01f || std::abs (lastFormant) > 0.01f)
    {
        if (--formantCounter <= 0 || std::abs (fm - lastFormant) > 0.25f)
        {
            formantCounter = 32;
            if (std::abs (fm - lastFormant) > 1.0e-4f)
            {
                lastFormant = fm;
                for (int c = 0; c < 2; ++c)
                {
                    lowShelf[c].setLowShelf (350.0f, -fm * 0.55f, sr);
                    highShelf[c].setHighShelf (2600.0f, fm * 0.9f, sr);
                }
            }
        }
        if (std::abs (lastFormant) > 0.01f)
        {
            ol = highShelf[0].process (lowShelf[0].process (ol));
            orr = highShelf[1].process (lowShelf[1].process (orr));
        }
        else
        {
            lastFormant = 0.0f;
            for (auto& b : lowShelf) b.reset();
            for (auto& b : highShelf) b.reset();
        }
    }

    l = ol;
    r = orr;
}
} // namespace vk
