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
    return getLocalBounds().toFloat().withTrimmedTop (30.0f).reduced (12.0f).withTrimmedBottom (0.0f);
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
    g.setColour (Colours::accent);
    g.setFont (Theme::mono (8.0f, 0.2f));
    g.drawText ("KILL PAD", juce::Rectangle<float> (12.0f, 8.0f, 120.0f, 16.0f), juce::Justification::centredLeft, false);
    g.setColour (Colours::muted);
    g.setFont (Theme::monoLight (7.0f, 0.16f));
    g.drawText (morphMode ? juce::String (juce::CharPointer_UTF8 ("X MORPH \xc2\xb7 Y TONE"))
                          : juce::String (juce::CharPointer_UTF8 ("X AMOUNT \xc2\xb7 Y TONE")),
                juce::Rectangle<float> (b.getWidth() - 172.0f, 8.0f, 160.0f, 16.0f), juce::Justification::centredRight, false);

    const auto a = padArea();

    // dark grid well (cached)
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int iw = juce::roundToInt (a.getWidth() * scale), ih = juce::roundToInt (a.getHeight() * scale);
    if (gridCache.getWidth() != iw || gridCache.getHeight() != ih || cachedMorphMode != morphMode)
    {
        cachedMorphMode = morphMode;
        gridCache = juce::Image (juce::Image::ARGB, juce::jmax (1, iw), juce::jmax (1, ih), true);
        juce::Graphics gg (gridCache);
        gg.addTransform (juce::AffineTransform::scale (scale));
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, a.getWidth(), a.getHeight());
        Theme::drawWell (gg, r, 10.0f);
        juce::Graphics::ScopedSaveState ss (gg);
        juce::Path clip;
        clip.addRoundedRectangle (r, 10.0f);
        gg.reduceClipRegion (clip);
        for (int i = 1; i < 10; ++i)
        {
            const float x = r.getWidth() * (float) i / 10.0f, y = r.getHeight() * (float) i / 10.0f;
            gg.setColour (Colours::accent.withAlpha (i == 5 ? 0.22f : 0.08f));
            gg.drawLine (x, 0.0f, x, r.getHeight(), 1.0f);
            gg.drawLine (0.0f, y, r.getWidth(), y, 1.0f);
        }
        gg.setColour (Colours::muted);
        gg.setFont (Theme::monoLight (7.0f, 0.2f));
        gg.drawText ("BRIGHT", r.withHeight (22.0f), juce::Justification::centred, false);
        gg.drawText ("DARK", r.withTrimmedTop (r.getHeight() - 22.0f), juce::Justification::centred, false);
        if (morphMode)
        {
            gg.drawText ("A", juce::Rectangle<float> (6.0f, r.getCentreY() + 2.0f, 14.0f, 14.0f), juce::Justification::centred, false);
            gg.drawText ("B", juce::Rectangle<float> (r.getRight() - 20.0f, r.getCentreY() + 2.0f, 14.0f, 14.0f), juce::Justification::centred, false);
        }
        else
        {
            gg.drawText ("AMOUNT", juce::Rectangle<float> (r.getRight() - 64.0f, r.getCentreY() + 2.0f, 58.0f, 14.0f), juce::Justification::centredRight, false);
        }
    }
    g.drawImage (gridCache, a);

    // crosshair through the dot
    const auto d = getDotPosition();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (a.reduced (1.0f).toNearestInt());
        g.setColour (Colours::accentL.withAlpha (0.28f));
        const float dash[] = { 3.0f, 4.0f };
        g.drawDashedLine ({ a.getX(), d.y, a.getRight(), d.y }, dash, 2, 1.0f);
        g.drawDashedLine ({ d.x, a.getY(), d.x, a.getBottom() }, dash, 2, 1.0f);
    }

    Theme::drawGlowDot (g, d, dragging ? 7.0f : 6.0f, dragging ? 40.0f : 32.0f);
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
