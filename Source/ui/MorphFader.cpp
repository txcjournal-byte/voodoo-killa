#include "MorphFader.h"
#include "../engine/Preset.h"

namespace vk::ui
{
MorphFader::MorphFader (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : ParamControl (p, std::move (gestureEnded))
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void MorphFader::setMorphEnabled (bool e)
{
    if (e != enabled) { enabled = e; repaint(); }
}

void MorphFader::setNames (const juce::String& a, const juce::String& b)
{
    if (a != nameA || b != nameB) { nameA = a; nameB = b; repaint(); }
}

juce::Rectangle<float> MorphFader::trackArea() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 34.0f, b.getY() + 44.0f, b.getWidth() - 68.0f, 6.0f };
}

float MorphFader::valueAt (float x) const
{
    const auto t = trackArea();
    return juce::jlimit (0.0f, 1.0f, (x - t.getX()) / t.getWidth());
}

void MorphFader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colours::accent);
    g.setFont (Theme::mono (8.0f, 0.2f));
    g.drawText ("MORPH", b.withHeight (14.0f), juce::Justification::centredLeft, false);
    g.setColour (Colours::muted);
    g.setFont (Theme::mono (7.0f, 0.16f));
    const juce::String names = enabled ? (nameA + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 ")) + nameB).toUpperCase()
                                       : juce::String ("PICK A SECOND CARD");
    g.drawFittedText (names, b.withHeight (14.0f).withTrimmedLeft (52.0f).toNearestInt(), juce::Justification::centredRight, 1, 0.8f);

    const auto t = trackArea();
    juce::Graphics::ScopedSaveState ss (g);
    if (! enabled)
        g.setOpacity (0.45f);

    Theme::drawChromeText (g, "A", { b.getX(), t.getCentreY() - 13.0f, 26.0f, 26.0f }, Theme::display (19.0f), juce::Justification::centred, false);
    Theme::drawChromeText (g, "B", { b.getRight() - 26.0f, t.getCentreY() - 13.0f, 26.0f, 26.0f }, Theme::display (19.0f), juce::Justification::centred, false);

    g.setColour (Colours::track);
    g.fillRoundedRectangle (t, 3.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (t, 3.0f, 1.0f);

    const float x = t.getX() + t.getWidth() * getNormalised();
    if (x > t.getX() + 1.0f)
    {
        const auto fill = t.withRight (x);
        g.setColour (Colours::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (fill.expanded (0.0f, 3.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (Colours::accentD, t.getX(), 0.0f, Colours::accentL, x, 0.0f, false));
        g.fillRoundedRectangle (fill, 3.0f);
    }

    g.setColour (Colours::muted.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float tx = t.getX() + t.getWidth() * (float) i / 10.0f;
        g.drawLine (tx, t.getBottom() + 6.0f, tx, t.getBottom() + 10.0f, 1.0f);
    }

    const auto thumb = juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ x, t.getCentreY() });
    juce::Path tp;
    tp.addEllipse (thumb);
    juce::DropShadow (Colours::glow.withAlpha (0.9f), 12, {}).drawForPath (g, tp);
    g.setGradientFill (Theme::chromeGradient (thumb));
    g.fillPath (tp);
}

void MorphFader::mouseDown (const juce::MouseEvent& e)
{
    if (! enabled && onNeedB)
        setMorphEnabled (onNeedB());   // no B yet: pick one so the fader always works
    if (! enabled)
        return;
    beginEdit();
    setNormalised (valueAt (e.position.x));
}

void MorphFader::mouseDrag (const juce::MouseEvent& e)
{
    if (enabled)
        setNormalised (valueAt (e.position.x));
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
    setTooltip ("Pattern: the effect runs on the lit steps (one bar).");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

StepsBar::~StepsBar() = default;

void StepsBar::setPlayingStep (int step, bool show)
{
    if (step != playingStep || show != showPlaying) { playingStep = step; showPlaying = show; repaint(); }
}

juce::Rectangle<float> StepsBar::cellBounds (int i) const
{
    const float w = (float) getWidth() / (float) vk::kNumSteps;
    const float s = juce::jmin (w - 4.0f, 22.0f);
    return juce::Rectangle<float> (s, s).withCentre ({ w * ((float) i + 0.5f), s * 0.5f + 2.0f });
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
        if (on)
        {
            g.setColour (Colours::accent.withAlpha (0.3f));
            g.fillRoundedRectangle (c.expanded (2.0f), 6.0f);
            g.setGradientFill (juce::ColourGradient (Colours::accentL, 0.0f, c.getY(), Colours::accentD, 0.0f, c.getBottom(), false));
        }
        else
        {
            g.setColour (Colours::well);
        }
        g.fillRoundedRectangle (c, 5.0f);
        g.setColour (on ? Colours::accentL.withAlpha (0.6f) : Colours::line2);
        g.drawRoundedRectangle (c, 5.0f, 1.0f);
        if (showPlaying && i == playingStep)
        {
            g.setColour (Colours::text.withAlpha (0.5f));
            g.drawRoundedRectangle (c.expanded (2.5f), 7.0f, 1.2f);
        }
        g.setColour (i % 4 == 0 ? Colours::text2 : Colours::muted);
        g.setFont (Theme::mono (7.5f, 0.0f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (c.getX() - 4.0f, c.getBottom() + 4.0f, c.getWidth() + 8.0f, 12.0f),
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
    if (i != lastPainted) { lastPainted = i; setStep (i, paintValue); }
}

void StepsBar::mouseUp (const juce::MouseEvent&)
{
    lastPainted = -1;
    if (onGestureEnd) onGestureEnd();
}

// ===========================================================================
WhenSelector::WhenSelector (juce::RangedAudioParameter& p, juce::AudioProcessorValueTreeState& s, std::function<void()> gestureEnded)
    : ParamControl (p, gestureEnded), state (s), gestureEnd (gestureEnded)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("When the effect runs: always, while held, at the end of a 4/8-bar phrase, on the last beat or on a pattern.");
}

juce::String WhenSelector::labelFor (int i)
{
    static const char* names[] = { "Always", "Hold", "End of 4 bars", "End of 8 bars", "Last beat", "Pattern" };
    return names[juce::jlimit (0, 5, i)];
}

juce::Rectangle<float> WhenSelector::pillArea() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTrimmedLeft (52.0f).withSizeKeepingCentre (b.getWidth() - 52.0f, 26.0f);
}

void WhenSelector::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colours::text2);
    g.setFont (Theme::mono (7.5f, 0.18f));
    g.drawText ("WHEN", b.withWidth (52.0f), juce::Justification::centredLeft, false);

    const auto r = pillArea();
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, 13.0f);
    g.setColour (isMouseOver() ? Colours::accentL : Colours::line2);
    g.drawRoundedRectangle (r, 13.0f, 1.0f);
    g.setColour (Colours::text);
    g.setFont (Theme::bold (11.5f));
    g.drawFittedText (labelFor (juce::roundToInt (getPlain())).toUpperCase(), r.reduced (12.0f, 0.0f).withTrimmedRight (12.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);
    juce::Path tri;
    const float x = r.getRight() - 14.0f, y = r.getCentreY();
    tri.addTriangle (x - 4.0f, y - 2.5f, x + 4.0f, y - 2.5f, x, y + 3.0f);
    g.setColour (Colours::accentL);
    g.fillPath (tri);
}

void WhenSelector::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    juce::PopupMenu m;
    const int current = juce::roundToInt (getPlain());
    for (int i = 0; i < (int) vk::TriggerMode::count; ++i)
        m.addItem (i + 1, labelFor (i), true, i == current);
    m.addSeparator();
    m.addItem (100, "Edit pattern...");

    juce::Component::SafePointer<WhenSelector> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        if (r == 100)
        {
            safe->showPatternEditor();
            return;
        }
        safe->setNormalisedComplete (safe->param.convertTo0to1 ((float) (r - 1)));
        if (r - 1 == (int) vk::TriggerMode::steps)
            safe->showPatternEditor();
    });
}

void WhenSelector::showPatternEditor()
{
    auto bar = std::make_unique<StepsBar> (state, gestureEnd);
    bar->setSize (400, 44);
    // shown inside the plugin window (a separate desktop window can hide behind the host's window)
    auto* top = getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously (std::move (bar), top->getLocalArea (this, getLocalBounds()), top);
}
} // namespace vk::ui
