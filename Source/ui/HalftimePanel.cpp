#include "HalftimePanel.h"
#include "../PluginProcessor.h"

namespace vk::ui
{
namespace
{
    const float kSpeeds[] = { 1.5f, 2.0f, 3.0f, 4.0f };
    const float kLoops[]  = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };   // beats (1/4 bar .. 4 bars)
    const float kBrakes[] = { 0.0f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
    const float kFades[]  = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
    constexpr float kSmoothMin = 2.0f, kSmoothMax = 80.0f, kSmoothDefault = 10.0f;

    template <size_t N>
    int nearestIndex (const float (&values)[N], float v)
    {
        int best = 0;
        for (int i = 1; i < (int) N; ++i)
            if (std::abs (values[i] - v) < std::abs (values[best] - v))
                best = i;
        return best;
    }

    juce::String beatsText (float beats)
    {
        if (beats <= 0.0f)  return "OFF";
        if (beats >= 8.0f)  return juce::String (juce::roundToInt (beats / 4.0f)) + " BARS";
        if (beats >= 4.0f)  return "1 BAR";
        return "1/" + juce::String (juce::roundToInt (4.0f / beats));
    }

    const juce::String& dot() { static const juce::String d (juce::CharPointer_UTF8 (" \xc2\xb7 ")); return d; }
}

HalftimePanel::HalftimePanel (VoodooKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    auto time = [this]() -> const vk::TimeParams& { return proc.getSlotA().info.data.time; };

    smooth.getValue = [time]
    {
        const float ms = time().smoothMs > 0.0f ? time().smoothMs : kSmoothDefault;
        return (ms - kSmoothMin) / (kSmoothMax - kSmoothMin);
    };
    smooth.setValue = [this] (float v, bool done)
    {
        proc.editHalftime ([v] (vk::TimeParams& t, float&) { t.smoothMs = kSmoothMin + v * (kSmoothMax - kSmoothMin); }, done);
    };
    smooth.getText = [time]
    {
        return juce::String (juce::roundToInt (time().smoothMs > 0.0f ? time().smoothMs : kSmoothDefault)) + " ms";
    };
    smooth.defaultValue = (kSmoothDefault - kSmoothMin) / (kSmoothMax - kSmoothMin);
    smooth.setTooltip ("Smooth: crossfade at every loop seam (short = punchy, long = soft).");

    brake.getValue = [time] { return (float) nearestIndex (kBrakes, time().brake) / 6.0f; };
    brake.setValue = [this] (float v, bool done)
    {
        proc.editHalftime ([v] (vk::TimeParams& t, float&) { t.brake = kBrakes[juce::roundToInt (v * 6.0f)]; }, done);
    };
    brake.getText = [time] { return beatsText (time().brake); };
    brake.setTooltip ("Brake: glide from normal speed into halftime at every loop start (tape brake).");

    fade.getValue = [time] { return (float) nearestIndex (kFades, time().fade) / 7.0f; };
    fade.setValue = [this] (float v, bool done)
    {
        proc.editHalftime ([v] (vk::TimeParams& t, float&) { t.fade = kFades[juce::roundToInt (v * 7.0f)]; }, done);
    };
    fade.getText = [time] { return beatsText (time().fade); };
    fade.setTooltip ("Fade: blend the halftime in gradually when it starts.");

    for (auto* k : { &smooth, &brake, &fade })
        addAndMakeVisible (k);
    refresh();
}

bool HalftimePanel::isOn() const
{
    const auto& a = proc.getSlotA();
    return a.valid && vk::isHalftimeMode (a.info.data.time.mode);
}

void HalftimePanel::refresh()
{
    for (auto* k : { &smooth, &brake, &fade })
    {
        k->setEnabledLook (isOn());
        k->repaint();
    }
    repaint();
}

int HalftimePanel::numCells (int row)
{
    switch (row)
    {
        case 0: return 4;
        case 1: return 6;
        case 2: return 3;
        default: return 1;
    }
}

juce::Rectangle<float> HalftimePanel::rowArea (int row) const
{
    const float x0 = 64.0f, right = (float) getWidth() - 12.0f;
    const float y = 38.0f + (float) juce::jmin (row, 2) * 38.0f;
    if (row == 2) return { x0, y, right - x0 - 52.0f, 24.0f };
    if (row == 3) return { right - 44.0f, y, 44.0f, 24.0f };
    return { x0, y, right - x0, 24.0f };
}

juce::Rectangle<float> HalftimePanel::cell (int row, int index) const
{
    const auto r = rowArea (row).reduced (2.0f);
    const float w = r.getWidth() / (float) numCells (row);
    return { r.getX() + w * (float) index, r.getY(), w, r.getHeight() };
}

HalftimePanel::Hit HalftimePanel::hitAt (juce::Point<float> p) const
{
    for (int row = 0; row < 4; ++row)
        if (rowArea (row).contains (p))
            for (int i = 0; i < numCells (row); ++i)
                if (cell (row, i).expanded (0.0f, 2.0f).contains (p))
                    return { row, i };
    return {};
}

int HalftimePanel::selectedIndex (int row) const
{
    if (! isOn())
        return -1;
    const auto& d = proc.getSlotA().info.data;
    switch (row)
    {
        case 0: return nearestIndex (kSpeeds, vk::halftimeFactor (d.time));
        case 1: return nearestIndex (kLoops, d.length);
        case 2: return vk::halftimeMode (d.time);
        default: return -1;
    }
}

juce::String HalftimePanel::pitchInfo() const
{
    if (! isOn())
        return "OFF";
    const auto& t = proc.getSlotA().info.data.time;
    const int mode = vk::halftimeMode (t);
    if (mode == vk::htHold)
        return "PITCH KEPT";
    float semis = -12.0f * std::log2 (vk::halftimeFactor (t));
    if (mode == vk::htKey)
        semis += vk::halftimeKeyCorrection (vk::halftimeFactor (t));
    const int st = juce::roundToInt (semis);
    juce::String name = st == 0 ? "UNISON" : (std::abs (st) % 12 == 0 ? (std::abs (st) == 12 ? "OCTAVE" : "2 OCTAVES")
                                           : (std::abs (st) == 7 ? "FIFTH" : (std::abs (st) == 19 ? "OCT + FIFTH" : "")));
    juce::String s = (st == 0 ? juce::String ("0") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (std::abs (st))) + " ST";
    return name.isEmpty() ? s : s + dot() + name;
}

void HalftimePanel::paint (juce::Graphics& g)
{
    const bool on = isOn();
    const auto b = getLocalBounds().toFloat();

    Theme::drawGlowDot (g, { 16.0f, 16.0f }, 2.6f, on ? 9.0f : 0.1f);
    if (! on)
    {
        g.setColour (Colours::well);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ 16.0f, 16.0f }));
        g.setColour (Colours::muted);
        g.drawEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ 16.0f, 16.0f }), 1.0f);
    }
    g.setColour (Colours::accent);
    g.setFont (Theme::mono (8.0f, 0.2f));
    g.drawText ("HALFTIME ENGINE", juce::Rectangle<float> (26.0f, 8.0f, 150.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (Colours::muted);
    g.setFont (Theme::monoLight (7.0f, 0.16f));
    g.drawText (pitchInfo(), juce::Rectangle<float> (b.getWidth() - 132.0f, 8.0f, 120.0f, 16.0f), juce::Justification::centredRight, false);

    static const char* rowNames[] = { "SPEED", "LOOP", "MODE" };
    for (int row = 0; row < 3; ++row)
    {
        g.setColour (Colours::text2);
        g.setFont (Theme::mono (7.0f, 0.18f));
        g.drawText (rowNames[row], juce::Rectangle<float> (12.0f, rowArea (row).getY(), 50.0f, 24.0f), juce::Justification::centredLeft, false);
    }

    const auto& d = proc.getSlotA().info.data;
    static const juce::StringArray labels[] = {
        { juce::String (juce::CharPointer_UTF8 ("1.5\xc3\x97")), juce::String (juce::CharPointer_UTF8 ("2\xc3\x97")),
          juce::String (juce::CharPointer_UTF8 ("3\xc3\x97")), juce::String (juce::CharPointer_UTF8 ("4\xc3\x97")) },
        { juce::String (juce::CharPointer_UTF8 ("\xc2\xbc")), juce::String (juce::CharPointer_UTF8 ("\xc2\xbd")), "1 BAR", "2", "4", "T" },
        { "TAPE", "KEY", "HOLD" },
        { juce::String (juce::CharPointer_UTF8 ("REV \xe2\x87\x84")) } };

    for (int row = 0; row < 4; ++row)
    {
        const auto r = rowArea (row);
        g.setColour (Colours::well);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

        const int sel = selectedIndex (row);
        for (int i = 0; i < numCells (row); ++i)
        {
            const auto c = cell (row, i);
            bool active = i == sel;
            if (row == 1 && i == 5) active = on && d.time.triplet;
            if (row == 3)           active = on && d.time.reverse;
            if (active)
                Theme::drawChromePill (g, c, 0.25f);
            else if (hover.row == row && hover.index == i)
            {
                g.setColour (Colours::accent.withAlpha (0.12f));
                g.fillRoundedRectangle (c, c.getHeight() * 0.5f);
            }
            g.setColour (active ? Colours::well : (on ? Colours::text : Colours::text2));
            g.setFont (Theme::bold (row == 3 ? 10.0f : 10.5f));
            g.drawFittedText (labels[row][i], c.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
}

void HalftimePanel::resized()
{
    const float y = 152.0f;
    const float w = ((float) getWidth() - 24.0f) / 3.0f;
    int i = 0;
    for (auto* k : { &smooth, &brake, &fade })
        k->setBounds (juce::Rectangle<float> (12.0f + w * (float) i++, y, w, 32.0f).toNearestInt());
}

void HalftimePanel::click (Hit h)
{
    switch (h.row)
    {
        case 0:
        {
            const float f = kSpeeds[h.index];
            proc.editHalftime ([f] (vk::TimeParams& t, float&) { t.mode = vk::TimeMode::half; t.slow = f; }, true);
            break;
        }
        case 1:
            if (h.index == 5)
                proc.editHalftime ([] (vk::TimeParams& t, float&) { t.triplet = ! t.triplet; }, true);
            else
            {
                const float len = kLoops[h.index];
                proc.editHalftime ([len] (vk::TimeParams&, float& l) { l = len; }, true);
            }
            break;
        case 2:
        {
            const int m = h.index;
            proc.editHalftime ([m] (vk::TimeParams& t, float&) { t.htMode = m; }, true);
            break;
        }
        case 3:
            proc.editHalftime ([] (vk::TimeParams& t, float&) { t.reverse = ! t.reverse; }, true);
            break;
        default: break;
    }
}

void HalftimePanel::mouseUp (const juce::MouseEvent& e)
{
    const auto h = hitAt (e.position);
    if (h.row >= 0)
        click (h);
    refresh();
}

void HalftimePanel::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitAt (e.position);
    if (h.row != hover.row || h.index != hover.index)
    {
        hover = h;
        repaint();
    }
}

void HalftimePanel::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

juce::String HalftimePanel::getTooltip()
{
    switch (hitAt (getMouseXYRelative().toFloat()).row)
    {
        case 0: return "Speed: how much slower the halftime plays (2x = classic halftime).";
        case 1: return "Loop: length of the halftime section before it jumps back in sync. T = triplet.";
        case 2: return "Tape = pitch drops, Key = stays in key (octave), Hold = pitch kept (stretch).";
        case 3: return "Reverse: play the halftime backwards.";
        default: return isOn() ? juce::String() : juce::String ("Pick a speed to add halftime to this preset.");
    }
}
} // namespace vk::ui
