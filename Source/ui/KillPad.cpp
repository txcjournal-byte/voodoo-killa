#include "KillPad.h"
#include "../PluginProcessor.h"

namespace vk::ui
{
KillPad::KillPad (VoodooKillaAudioProcessor& p)
    : proc (p),
      morph (*p.apvts.getParameter (ParamIDs::morph)),
      amount (*p.apvts.getParameter (ParamIDs::amount)),
      tone (*p.apvts.getParameter (ParamIDs::tone))
{
    morphAtt  = std::make_unique<juce::ParameterAttachment> (morph,  [this] (float) { paramChanged(); });
    amountAtt = std::make_unique<juce::ParameterAttachment> (amount, [this] (float) { paramChanged(); });
    toneAtt   = std::make_unique<juce::ParameterAttachment> (tone,   [this] (float) { paramChanged(); });
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    refresh();
}

KillPad::~KillPad() = default;

void KillPad::refresh()
{
    const bool m = proc.getSlotB().valid;
    if (m != morphMode)
    {
        morphMode = m;
        setTooltip (morphMode ? "Kill Pad: X = morph A-B, Y = tone. Double-click to centre."
                              : "Kill Pad: X = amount, Y = tone. Double-click to centre.");
        paramChanged();
    }
    if (getTooltip().isEmpty())
        setTooltip ("Kill Pad: X = amount, Y = tone. Double-click to centre.");
}

juce::RangedAudioParameter& KillPad::xParam() const { return morphMode ? morph : amount; }

juce::Rectangle<float> KillPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (26.0f, 20.0f).withTrimmedBottom (8.0f);
}

juce::Point<float> KillPad::getDotPosition() const
{
    const auto a = padArea();
    return { a.getX() + a.getWidth() * xParam().getValue(), a.getBottom() - a.getHeight() * tone.getValue() };
}

void KillPad::paramChanged()
{
    repaint();
    if (onDotMoved) onDotMoved();
}

void KillPad::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Theme::drawPaper (g, b, 4242u, true);

    const auto a = padArea();

    // millimetre paper grid (cached)
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int iw = juce::roundToInt (a.getWidth() * scale), ih = juce::roundToInt (a.getHeight() * scale);
    if (gridCache.getWidth() != iw || gridCache.getHeight() != ih)
    {
        gridCache = juce::Image (juce::Image::ARGB, juce::jmax (1, iw), juce::jmax (1, ih), true);
        juce::Graphics gg (gridCache);
        gg.addTransform (juce::AffineTransform::scale (scale));
        const float w = a.getWidth(), h = a.getHeight();
        for (int i = 0; i <= 40; ++i)
        {
            const bool major = i % 5 == 0;
            gg.setColour (Colours::ink.withAlpha (major ? 0.22f : 0.08f));
            const float x = w * (float) i / 40.0f, y = h * (float) i / 40.0f;
            gg.drawLine (x, 0.0f, x, h, major ? 1.0f : 0.6f);
            gg.drawLine (0.0f, y, w, y, major ? 1.0f : 0.6f);
        }
        gg.setColour (Colours::ink.withAlpha (0.75f));
        gg.drawRect (juce::Rectangle<float> (0.0f, 0.0f, w, h), 1.5f);
    }
    g.drawImage (gridCache, a);

    // axis labels
    g.setColour (Colours::ink);
    g.setFont (Theme::typewriter (15.0f));
    if (morphMode)
    {
        g.drawText ("A", juce::Rectangle<float> (a.getX() + 4.0f, a.getCentreY() - 10.0f, 20.0f, 20.0f), juce::Justification::centred);
        g.drawText ("B", juce::Rectangle<float> (a.getRight() - 24.0f, a.getCentreY() - 10.0f, 20.0f, 20.0f), juce::Justification::centred);
        g.drawText ("MORPH", juce::Rectangle<float> (a.getX(), a.getBottom() + 2.0f, a.getWidth(), 18.0f), juce::Justification::centred);
    }
    else
    {
        g.drawText ("AMOUNT", juce::Rectangle<float> (a.getX(), a.getBottom() + 2.0f, a.getWidth(), 18.0f), juce::Justification::centred);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        const auto c = juce::Point<float> (a.getX() - 13.0f, a.getCentreY());
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, c.x, c.y));
        g.drawText ("TONE", juce::Rectangle<float> (80.0f, 18.0f).withCentre (c), juce::Justification::centred);
    }

    // dot + glow
    const auto d = getDotPosition();
    juce::ColourGradient glow (Colours::red.withAlpha (dragging ? 0.55f : 0.4f), d.x, d.y,
                               Colours::red.withAlpha (0.0f), d.x + 30.0f, d.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (60.0f, 60.0f).withCentre (d));
    Theme::drawPin (g, d, 8.0f);
}

void KillPad::setFromPosition (juce::Point<float> p)
{
    const auto a = padArea();
    const float x = juce::jlimit (0.0f, 1.0f, (p.x - a.getX()) / a.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight());
    auto& xa = morphMode ? *morphAtt : *amountAtt;
    xa.setValueAsPartOfGesture (xParam().convertFrom0to1 (x));
    toneAtt->setValueAsPartOfGesture (tone.convertFrom0to1 (y));
}

void KillPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    (morphMode ? *morphAtt : *amountAtt).beginGesture();
    toneAtt->beginGesture();
    setFromPosition (e.position);
}

void KillPad::mouseDrag (const juce::MouseEvent& e)
{
    setFromPosition (e.position);
}

void KillPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    (morphMode ? *morphAtt : *amountAtt).endGesture();
    toneAtt->endGesture();
    proc.pushUndoSnapshot();
    repaint();
}

void KillPad::mouseDoubleClick (const juce::MouseEvent&)
{
    (morphMode ? *morphAtt : *amountAtt).setValueAsCompleteGesture (xParam().convertFrom0to1 (0.5f));
    toneAtt->setValueAsCompleteGesture (0.0f);
    proc.pushUndoSnapshot();
}
} // namespace vk::ui
