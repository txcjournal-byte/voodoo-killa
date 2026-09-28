#include "MorphFader.h"
#include "../engine/Preset.h"

namespace vk::ui
{
MorphFader::MorphFader (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : ParamControl (p, std::move (gestureEnded))
{
    setTooltip ("Morph between card A and card B (shift/right-click a second card to set B).");
}

void MorphFader::setMorphEnabled (bool e)
{
    if (e != enabled)
    {
        enabled = e;
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

juce::Rectangle<float> MorphFader::trackArea() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTrimmedLeft (44.0f).withTrimmedRight (44.0f).withSizeKeepingCentre (b.getWidth() - 88.0f, 30.0f)
            .withY (b.getCentreY() - 6.0f);
}

float MorphFader::valueAt (float x) const
{
    const auto t = trackArea();
    return juce::jlimit (0.0f, 1.0f, (x - t.getX()) / t.getWidth());
}

void MorphFader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto t = trackArea();
    const auto ink = enabled ? Colours::ink : Colours::grey;

    // A / B letters
    g.setColour (ink);
    g.setFont (Theme::marker (30.0f));
    g.drawText ("A", juce::Rectangle<float> (0.0f, t.getY() - 8.0f, 44.0f, 44.0f), juce::Justification::centred);
    g.drawText ("B", juce::Rectangle<float> (b.getRight() - 44.0f, t.getY() - 8.0f, 44.0f, 44.0f), juce::Justification::centred);

    // slot with tick marks
    const auto slot = t.withSizeKeepingCentre (t.getWidth(), 10.0f);
    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRect (slot);
    g.setColour (ink.withAlpha (0.75f));
    for (int i = 0; i <= 40; ++i)
    {
        const float x = t.getX() + t.getWidth() * (float) i / 40.0f;
        const float h = (i % 5 == 0) ? 12.0f : 7.0f;
        g.drawLine (x, slot.getY() - h * 0.5f + 5.0f, x, slot.getBottom() + h * 0.5f - 5.0f, 1.0f);
    }

    // handle
    const float x = t.getX() + t.getWidth() * getNormalised();
    auto handle = juce::Rectangle<float> (16.0f, 38.0f).withCentre ({ x, t.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (handle.translated (2.0f, 3.0f), 2.5f);
    juce::ColourGradient grad (enabled ? Colours::red.brighter (0.2f) : Colours::grey, handle.getX(), handle.getY(),
                               enabled ? Colours::redDark : Colours::greyDark, handle.getRight(), handle.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (handle, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (handle.getCentreX(), handle.getY() + 4.0f, handle.getCentreX(), handle.getBottom() - 4.0f, 1.5f);
}

void MorphFader::mouseDown (const juce::MouseEvent& e)
{
    if (! enabled && onNeedB)
        setMorphEnabled (onNeedB());   // no B yet: pick one so the fader always works
    if (! enabled)
        return;
    beginEdit();
    const auto t = trackArea();
    const float hx = t.getX() + t.getWidth() * getNormalised();
    grabOffset = std::abs (e.position.x - hx) < 12.0f ? e.position.x - hx : 0.0f;
    setNormalised (valueAt (e.position.x - grabOffset));
}

void MorphFader::mouseDrag (const juce::MouseEvent& e)
{
    if (! enabled)
        return;
    setNormalised (valueAt (e.position.x - grabOffset));
}

void MorphFader::mouseUp (const juce::MouseEvent&)
{
    if (enabled)
        endEdit();
}

void MorphFader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (enabled)
        setNormalisedComplete (0.5f);
}

// ===========================================================================
StepsBar::StepsBar (juce::AudioProcessorValueTreeState& state, std::function<void()> gestureEnded)
    : onGestureEnd (std::move (gestureEnded))
{
    for (int i = 0; i < vk::kNumSteps; ++i)
    {
        auto* p = state.getParameter ("step" + juce::String (i + 1));
        params.push_back (p);
        attachments.push_back (std::make_unique<juce::ParameterAttachment> (*p, [this] (float) { repaint(); }));
    }
    setTooltip ("Steps: the effect runs on the lit steps (Trigger = Steps).");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

StepsBar::~StepsBar() = default;

void StepsBar::setPlayingStep (int step, bool show)
{
    if (step != playingStep || show != showPlaying)
    {
        playingStep = step;
        showPlaying = show;
        repaint();
    }
}

juce::Rectangle<float> StepsBar::cellBounds (int i) const
{
    const float w = (float) getWidth() / (float) vk::kNumSteps;
    const float s = juce::jmin (w - 3.0f, 24.0f);
    return juce::Rectangle<float> (s, s).withCentre ({ w * ((float) i + 0.5f), s * 0.5f + 1.0f });
}

int StepsBar::stepAt (juce::Point<float> p) const
{
    return juce::jlimit (0, vk::kNumSteps - 1, (int) (p.x / ((float) getWidth() / (float) vk::kNumSteps)));
}

bool StepsBar::isOn (int i) const { return params[(size_t) i]->getValue() > 0.5f; }

void StepsBar::setStep (int i, bool on)
{
    if (isOn (i) != on)
        attachments[(size_t) i]->setValueAsCompleteGesture (on ? 1.0f : 0.0f);
}

void StepsBar::paint (juce::Graphics& g)
{
    for (int i = 0; i < vk::kNumSteps; ++i)
    {
        const auto c = cellBounds (i);
        const bool on = isOn (i);
        g.setColour (on ? Colours::red : juce::Colour (0xff0e0e0e));
        g.fillRect (c.reduced (2.0f));
        g.setColour (Colours::paper.withAlpha (0.85f));
        g.drawRect (c, 1.5f);
        if (showPlaying && i == playingStep)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.2f));
            g.fillRect (c.reduced (4.0f));
        }
        g.setColour (Colours::paper);
        g.setFont (Theme::typewriter (i >= 9 ? 10.0f : 11.5f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (c.getX() - 3.0f, c.getBottom() + 3.0f, c.getWidth() + 6.0f, 14.0f),
                    juce::Justification::centred, false);
    }
}

void StepsBar::mouseDown (const juce::MouseEvent& e)
{
    const int i = stepAt (e.position);
    paintValue = ! isOn (i);
    lastPainted = i;
    setStep (i, paintValue);
}

void StepsBar::mouseDrag (const juce::MouseEvent& e)
{
    const int i = stepAt (e.position);
    if (i != lastPainted)
    {
        lastPainted = i;
        setStep (i, paintValue);
    }
}

void StepsBar::mouseUp (const juce::MouseEvent&)
{
    lastPainted = -1;
    if (onGestureEnd) onGestureEnd();
}

// ===========================================================================
TriggerBar::TriggerBar (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : ParamControl (p, std::move (gestureEnded))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> TriggerBar::buttonBounds (int i) const
{
    static const float widths[] = { 1.2f, 0.85f, 1.0f, 1.0f, 1.25f, 0.9f };
    float total = 0.0f;
    for (auto w : widths) total += w;
    const float gap = 6.0f;
    const float unit = ((float) getWidth() - gap * 5.0f) / total;
    float x = 0.0f;
    for (int k = 0; k < i; ++k) x += widths[k] * unit + gap;
    return { x, 2.0f, widths[i] * unit, (float) getHeight() - 4.0f };
}

int TriggerBar::buttonAt (juce::Point<float> p) const
{
    for (int i = 0; i < (int) vk::TriggerMode::count; ++i)
        if (buttonBounds (i).expanded (3.0f, 0.0f).contains (p))
            return i;
    return -1;
}

void TriggerBar::paint (juce::Graphics& g)
{
    const int active = juce::roundToInt (getPlain());
    for (int i = 0; i < (int) vk::TriggerMode::count; ++i)
    {
        const auto r = buttonBounds (i);
        const bool on = i == active;
        g.setColour (juce::Colour (0xff0e0e0e));
        g.fillRect (r);
        g.setColour (on ? Colours::red : Colours::paper.withAlpha (0.8f));
        g.drawRect (r, on ? 2.0f : 1.2f);
        g.setFont (Theme::typewriter (12.5f));
        g.setColour (on ? Colours::red : Colours::paper);
        g.drawFittedText (vk::triggerLabel ((vk::TriggerMode) i), r.reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void TriggerBar::mouseUp (const juce::MouseEvent& e)
{
    const int i = buttonAt (e.position);
    if (i >= 0)
        setNormalisedComplete (param.convertTo0to1 ((float) i));
}

juce::String TriggerBar::getTooltip()
{
    static const char* tips[] = {
        "Always: the effect runs all the time.",
        "Hold: runs only while a card or MIDI key is held.",
        "Every 4: runs on the end of every 4-bar phrase.",
        "Every 8: runs on the end of every 8-bar phrase.",
        "Last Beat: runs on the last beat of every bar.",
        "Steps: runs on the lit steps of the 16-step bar."
    };
    const int i = buttonAt (getMouseXYRelative().toFloat());
    return i >= 0 ? juce::String (tips[i]) : juce::String ("When the effect runs.");
}
} // namespace vk::ui
