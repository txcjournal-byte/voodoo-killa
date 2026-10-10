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
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)));
}

void ParamControl::setNormalisedComplete (float v)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)));
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
void drawChromeKnob (juce::Graphics& g, juce::Rectangle<float> r, float v, bool bipolar)
{
    const auto c = r.getCentre();
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    const float ringR = size * 0.43f, ringW = juce::jmax (2.0f, size * 0.07f);
    const float a0 = -2.356f, a1 = 2.356f;   // -135 .. +135 deg (0 = 12 o'clock)

    auto arc = [&] (float from, float to)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, from, to, true);
        return p;
    };
    const juce::PathStrokeType stroke (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour (Colours::track);
    g.strokePath (arc (a0, a1), stroke);

    const float av = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v);
    const float from = bipolar ? 0.0f : a0;
    if (std::abs (av - from) > 0.02f)
    {
        const auto p = arc (juce::jmin (from, av), juce::jmax (from, av));
        g.setColour (Colours::accent.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (ringW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Colours::accentL);
        g.strokePath (p, stroke);
    }

    // chrome body
    const float bodyR = size * 0.30f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.06f, bodyR * 2.06f).withCentre (c.translated (0.0f, size * 0.02f)));
    juce::ColourGradient body (juce::Colour (0xfff5f7fc), c.x - bodyR * 0.45f, c.y - bodyR * 0.6f,
                               juce::Colour (0xff1a2130), c.x + bodyR, c.y + bodyR * 1.1f, true);
    body.addColour (0.45, juce::Colour (0xffaab3c5));
    body.addColour (0.75, juce::Colour (0xff4b5568));
    g.setGradientFill (body);
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (juce::Rectangle<float> (bodyR * 1.46f, bodyR * 1.46f).withCentre (c), 0.6f);

    // pointer
    const auto p0 = c.getPointOnCircumference (size * 0.08f, av);
    const auto p1 = c.getPointOnCircumference (size * 0.27f, av);
    g.setColour (Colours::well);
    g.drawLine ({ p0, p1 }, juce::jmax (1.6f, size * 0.045f));
}

// ===========================================================================
Knob::Knob (juce::RangedAudioParameter& p, juce::String l, std::function<void()> gestureEnded,
            std::function<juce::String (float)> formatter, bool bip)
    : ParamControl (p, std::move (gestureEnded)), label (std::move (l)), format (std::move (formatter)), bipolar (bip)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto labelArea = b.removeFromBottom (13.0f);
    const auto valueArea = b.removeFromBottom (18.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    drawChromeKnob (g, juce::Rectangle<float> (d, d).withCentre (b.getCentre()), getNormalised(), bipolar);

    g.setColour (Colours::text);
    g.setFont (Theme::bold (13.5f));
    g.drawText (format ? format (getPlain()) : param.getCurrentValueAsText(), valueArea, juce::Justification::centredBottom, false);
    g.setColour (Colours::muted);
    g.setFont (Theme::mono (7.5f, 0.2f));
    g.drawText (label.toUpperCase(), labelArea, juce::Justification::centred, false);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    beginEdit();
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

void Knob::mouseUp (const juce::MouseEvent&)            { endEdit(); }
void Knob::mouseDoubleClick (const juce::MouseEvent&)   { setNormalisedComplete (param.getDefaultValue()); }

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int steps = param.getNumSteps();
    const float delta = steps > 1 && steps < 100 ? 1.0f / (float) (steps - 1)
                                                 : ((e.mods.isCtrlDown() || e.mods.isCommandDown()) ? 0.01f : 0.05f);
    setNormalisedComplete (getNormalised() + (w.deltaY > 0 ? delta : -delta));
}

// ===========================================================================
MiniKnob::MiniKnob (juce::String l, int numSteps) : label (std::move (l)), steps (numSteps)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

float MiniKnob::quantise (float v) const
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (steps > 1)
        v = std::round (v * (float) (steps - 1)) / (float) (steps - 1);
    return v;
}

void MiniKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getHeight(), 34.0f);
    const auto knobArea = b.removeFromLeft (d + 2.0f).withSizeKeepingCentre (d, d);
    juce::Graphics::ScopedSaveState ss (g);
    if (! enabledLook)
        g.setOpacity (0.35f);
    drawChromeKnob (g, knobArea, getValue ? getValue() : 0.0f, false);

    b.removeFromLeft (5.0f);
    g.setColour (Colours::text);
    g.setFont (Theme::bold (11.0f));
    g.drawText (getText ? getText() : juce::String(), b.withTrimmedBottom (b.getHeight() * 0.46f), juce::Justification::bottomLeft, false);
    g.setColour (Colours::muted);
    g.setFont (Theme::mono (6.8f, 0.16f));
    g.drawText (label.toUpperCase(), b.withTrimmedTop (b.getHeight() * 0.56f), juce::Justification::topLeft, false);
}

void MiniKnob::mouseDown (const juce::MouseEvent& e)
{
    accum = getValue ? getValue() : 0.0f;
    lastY = (float) e.position.y;
}

void MiniKnob::mouseDrag (const juce::MouseEvent& e)
{
    const bool fine = e.mods.isCtrlDown() || e.mods.isCommandDown();
    accum = juce::jlimit (0.0f, 1.0f, accum + (lastY - (float) e.position.y) / (fine ? 700.0f : (steps > 1 ? 90.0f : 150.0f)));
    lastY = (float) e.position.y;
    if (setValue) setValue (quantise (accum), false);
    repaint();
}

void MiniKnob::mouseUp (const juce::MouseEvent&)
{
    if (setValue) setValue (quantise (accum), true);
    repaint();
}

void MiniKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (setValue) setValue (defaultValue, true);
    repaint();
}

// ===========================================================================
ToggleSwitch::ToggleSwitch (juce::RangedAudioParameter& p, std::function<void()> gestureEnded)
    : ParamControl (p, std::move (gestureEnded))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    const bool on = getNormalised() > 0.5f;
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (46.0f, 24.0f);
    juce::Path p;
    p.addRoundedRectangle (r, 12.0f);
    if (on)
    {
        juce::DropShadow (Colours::glow.withAlpha (0.6f), 14, {}).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (Colours::accentD, r.getX(), 0.0f, Colours::accentL, r.getRight(), 0.0f, false));
    }
    else
    {
        g.setColour (Colours::track);
    }
    g.fillPath (p);
    g.setColour (on ? Colours::accentL.withAlpha (0.4f) : Colours::line2);
    g.strokePath (p, juce::PathStrokeType (1.0f));

    const auto knob = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ on ? r.getRight() - 12.0f : r.getX() + 12.0f, r.getCentreY() });
    g.setGradientFill (Theme::chromeGradient (knob));
    g.fillEllipse (knob);
}

void ToggleSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()))
        setNormalisedComplete (getNormalised() > 0.5f ? 0.0f : 1.0f);
}
} // namespace vk::ui
