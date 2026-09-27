#include "Knob.h"

namespace vk::ui
{
ParamControl::ParamControl (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : param (p),
      attachment (p, [this] (float plain)
                  {
                      value = param.convertTo0to1 (plain);
                      valueChanged();
                  }),
      onGestureEnd (std::move (gestureEnded))
{
    attachment.sendInitialUpdate();
}

void ParamControl::beginEdit()
{
    if (! inGesture)
    {
        inGesture = true;
        attachment.beginGesture();
    }
}

void ParamControl::setNormalised (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (v));
}

void ParamControl::setNormalisedComplete (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (v));
    if (onGestureEnd) onGestureEnd();
}

void ParamControl::endEdit()
{
    if (inGesture)
    {
        inGesture = false;
        attachment.endGesture();
        if (onGestureEnd) onGestureEnd();
    }
}

// ===========================================================================
Knob::Knob (juce::RangedAudioParameter& p, juce::String l, std::function<void()> gestureEnded,
            std::function<juce::String (float)> formatter)
    : ParamControl (p, std::move (gestureEnded)), label (std::move (l)), format (std::move (formatter))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::drawKnobBody (juce::Graphics& g, juce::Rectangle<float> r, float v)
{
    const auto c = r.getCentre();
    const float rad = r.getWidth() * 0.5f;

    // shadow + ridged skirt
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (r.translated (2.0f, 3.0f));
    juce::Path skirt;
    const int ridges = 28;
    for (int i = 0; i < ridges * 2; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (ridges * 2);
        const auto pt = c.getPointOnCircumference (i % 2 == 0 ? rad : rad * 0.95f, a);
        if (i == 0) skirt.startNewSubPath (pt); else skirt.lineTo (pt);
    }
    skirt.closeSubPath();
    g.setColour (juce::Colour (0xff060606));
    g.fillPath (skirt);

    // bakelite cap
    const auto cap = r.reduced (rad * 0.12f);
    juce::ColourGradient grad (juce::Colour (0xff3a3a3a), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.2f,
                               juce::Colour (0xff050505), cap.getRight(), cap.getBottom(), true);
    g.setGradientFill (grad);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (cap.reduced (1.0f), 1.0f);

    // red line
    const float a = juce::jmap (v, -2.35f, 2.35f);
    const auto p0 = c.getPointOnCircumference (rad * 0.18f, a);
    const auto p1 = c.getPointOnCircumference (rad * 0.86f, a);
    g.setColour (Colours::red);
    g.drawLine ({ p0, p1 }, juce::jmax (2.5f, rad * 0.14f));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto labelArea = b.removeFromBottom (16.0f);
    const auto valueArea = b.removeFromBottom (26.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight()) - 4.0f;
    drawKnobBody (g, juce::Rectangle<float> (d, d).withCentre (b.getCentre()), getNormalised());

    g.setColour (Colours::ink);
    g.setFont (Theme::marker (21.0f));
    g.drawText (format ? format (getPlain()) : param.getCurrentValueAsText(), valueArea, juce::Justification::centred, false);
    g.setFont (Theme::typewriter (13.0f));
    g.drawText (label, labelArea, juce::Justification::centred, false);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    beginEdit();
    dragStartValue = getNormalised();
    lastDragY = (float) e.position.y;
    accum = getNormalised();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const bool fine = e.mods.isCtrlDown() || e.mods.isCommandDown();
    const float dy = lastDragY - (float) e.position.y;
    lastDragY = (float) e.position.y;
    accum = juce::jlimit (0.0f, 1.0f, accum + dy / (fine ? 900.0f : 180.0f));
    setNormalised (accum);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    endEdit();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    setNormalisedComplete (param.getDefaultValue());
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = (e.mods.isCtrlDown() || e.mods.isCommandDown()) ? 0.01f : 0.05f;
    const int steps = param.getNumSteps();
    const float delta = steps > 1 && steps < 100 ? 1.0f / (float) (steps - 1) : step;
    setNormalisedComplete (getNormalised() + (w.deltaY > 0 ? delta : -delta));
}

// ===========================================================================
RockerSwitch::RockerSwitch (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : ParamControl (p, std::move (gestureEnded))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RockerSwitch::paint (juce::Graphics& g)
{
    const bool on = getNormalised() > 0.5f;
    auto r = getLocalBounds().toFloat().reduced (4.0f);

    // housing
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (2.0f, 3.0f), 8.0f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRoundedRectangle (r, 8.0f);

    // rocker
    auto rocker = r.reduced (5.0f);
    juce::ColourGradient grad (on ? Colours::red.brighter (0.2f) : juce::Colour (0xffc4303a), rocker.getX(), rocker.getY(),
                               on ? Colours::redDark : juce::Colour (0xff7a1820), rocker.getX(), rocker.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rocker, 6.0f);

    // pressed side shading + indicator
    auto pressed = on ? rocker.withTrimmedRight (rocker.getWidth() * 0.5f) : rocker.withTrimmedLeft (rocker.getWidth() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pressed, 6.0f);

    const auto dot = juce::Rectangle<float> (rocker.getHeight() * 0.6f, rocker.getHeight() * 0.6f)
                         .withCentre ({ rocker.getX() + rocker.getWidth() * 0.27f, rocker.getCentreY() });
    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (dot);
    g.setColour (on ? Colours::red.brighter (0.3f) : juce::Colour (0xff5a0f15));
    g.drawEllipse (dot.reduced (2.0f), 2.0f);
    if (on)
    {
        g.setColour (Colours::red.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (4.0f));
    }
}

void RockerSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()))
        setNormalisedComplete (getNormalised() > 0.5f ? 0.0f : 1.0f);
}
} // namespace vk::ui
