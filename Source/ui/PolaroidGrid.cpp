#include "PolaroidGrid.h"
#include "../PluginProcessor.h"

namespace vk::ui
{
PolaroidCard::PolaroidCard (VoodooKillaAudioProcessor& p, int c) : proc (p), card (c)
{
    setBufferedToImage (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click = load as A. Shift/right-click = load as B (morph). Hold with Trigger = Hold.");
}

void PolaroidCard::setContent (int idx, int s)
{
    const auto* info = proc.library.get (idx);
    const auto newName = info != nullptr ? info->name : juce::String();
    if (idx == libraryIndex && s == slot && newName == name)
        return;

    libraryIndex = idx;
    slot = s;
    name = newName;
    desc = info != nullptr ? info->desc : juce::String();
    photo = info != nullptr ? Theme::photo (info->photo, placeholder) : juce::Image();
    repaint();
}

bool PolaroidCard::setPulse (float amount)
{
    // the pulse is drawn by the grid on top of the (buffered) card, so the card itself never re-renders
    amount = juce::jlimit (0.0f, 1.0f, amount);
    if (std::abs (amount - pulse) > 0.04f || (amount == 0.0f && pulse != 0.0f))
    {
        pulse = amount;
        return true;
    }
    return false;
}

juce::Rectangle<float> PolaroidCard::getFrameInParent() const
{
    return frameBounds() + getPosition().toFloat();
}

juce::Rectangle<float> PolaroidCard::frameBounds() const
{
    return getLocalBounds().toFloat().reduced (4.0f, 3.0f);
}

juce::Rectangle<float> PolaroidCard::photoBounds() const
{
    const auto f = frameBounds();
    const float m = 7.0f;
    return { f.getX() + m, f.getY() + m, f.getWidth() - 2.0f * m, f.getHeight() * 0.685f };
}

juce::Point<float> PolaroidCard::getPinPosition() const
{
    const auto f = frameBounds();
    return { f.getCentreX(), f.getY() + 2.0f };
}

void PolaroidCard::paint (juce::Graphics& g)
{
    const auto f = frameBounds();
    Theme::drawPaper (g, f, (uint32_t) (card * 977 + 13), false, Colours::paper.brighter (0.12f));

    // photo
    const auto ph = photoBounds();
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (ph);
    if (photo.isValid())
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (ph.toNearestInt());
        g.drawImage (photo, ph, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    if (placeholder)
    {
        g.setColour (Colours::grey.withAlpha (0.8f));
        g.setFont (Theme::typewriter (13.0f));
        g.drawFittedText (name.toUpperCase(), ph.reduced (8.0f).toNearestInt(), juce::Justification::centred, 2);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRect (ph, 1.0f);

    // name + description
    const auto textArea = f.withTop (ph.getBottom() + 2.0f).reduced (6.0f, 0.0f);
    const bool red = slot == 2;
    g.setColour (red ? Colours::red : Colours::ink);
    g.setFont (Theme::marker (21.0f));
    g.drawFittedText (name.toUpperCase(), textArea.withHeight (textArea.getHeight() * 0.58f).toNearestInt(),
                      juce::Justification::centredBottom, 1, 0.75f);
    g.setColour (Colours::ink.withAlpha (0.85f));
    g.setFont (Theme::typewriter (12.5f));
    g.drawFittedText (desc, textArea.withTrimmedTop (textArea.getHeight() * 0.58f).toNearestInt(),
                      juce::Justification::centredTop, 1, 0.8f);

    // selection frame + pin with A/B tag
    if (slot > 0)
    {
        g.setColour (Colours::red.withMultipliedBrightness (0.8f).withAlpha (0.9f));
        g.drawRect (f.reduced (1.0f), 2.5f);

        const auto pin = getPinPosition();
        auto tag = juce::Rectangle<float> (18.0f, 18.0f).withCentre (pin.translated (14.0f, 16.0f));
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (0.12f, tag.getCentreX(), tag.getCentreY()));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (tag.translated (1.0f, 1.5f));
        g.setColour (Colours::paper);
        g.fillRect (tag);
        g.setColour (Colours::ink);
        g.drawRect (tag, 0.8f);
        g.setFont (Theme::typewriter (14.0f));
        g.drawText (slot == 1 ? "A" : "B", tag, juce::Justification::centred, false);
    }
    if (slot > 0)
        Theme::drawPin (g, getPinPosition(), 7.0f);
}

void PolaroidCard::mouseDown (const juce::MouseEvent& e)
{
    if (libraryIndex < 0)
        return;

    const bool toB = e.mods.isRightButtonDown() || e.mods.isShiftDown()
                  || proc.getClickSlot() == VoodooKillaAudioProcessor::Slot::B;
    proc.loadPreset (libraryIndex, toB ? VoodooKillaAudioProcessor::Slot::B : VoodooKillaAudioProcessor::Slot::A);

    holding = true;
    proc.setHoldFromUI (true);
}

void PolaroidCard::mouseUp (const juce::MouseEvent&)
{
    if (holding)
    {
        holding = false;
        proc.setHoldFromUI (false);
    }
}

// ===========================================================================
PolaroidGrid::PolaroidGrid (VoodooKillaAudioProcessor& p) : proc (p)
{
    for (int i = 0; i < vk::kPresetsPerCategory; ++i)
    {
        cards.push_back (std::make_unique<PolaroidCard> (proc, i));
        addAndMakeVisible (*cards.back());
    }
    refresh();
}

void PolaroidGrid::refresh()
{
    const int cat = proc.getCurrentCategory();
    const auto& a = proc.getSlotA();
    const auto& b = proc.getSlotB();
    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const int idx = proc.library.factoryIndex (cat, i);
        int slot = 0;
        if (a.valid && a.libraryIndex == idx) slot = 1;
        else if (b.valid && b.libraryIndex == idx) slot = 2;
        cards[(size_t) i]->setContent (idx, slot);
    }
}

void PolaroidGrid::updatePulse (bool active, float phase)
{
    const float amount = active ? 0.5f + 0.5f * std::sin (phase) : 0.0f;
    const bool bSide = proc.getSlotB().valid && proc.apvts.getRawParameterValue (ParamIDs::morph)->load() >= 0.5f;
    for (auto& c : cards)
    {
        const bool running = (c->getSlot() == 1 && ! bSide) || (c->getSlot() == 2 && bSide);
        if (c->setPulse (running ? amount : 0.0f))
            repaint (c->getBounds());   // cheap: the card is a cached image, only the glow is drawn
    }
}

void PolaroidGrid::paintOverChildren (juce::Graphics& g)
{
    for (auto& c : cards)
    {
        const float p = c->getPulse();
        if (p <= 0.0f || c->getSlot() == 0)
            continue;
        const auto f = c->getFrameInParent();
        g.setColour (Colours::red.withAlpha (0.5f + 0.5f * p));
        g.drawRect (f.reduced (1.0f), 2.5f + p);
        g.setColour (Colours::red.withAlpha (0.18f * p));
        g.drawRect (f.reduced (4.0f), 3.0f);
    }
}

void PolaroidGrid::resized()
{
    const int cols = 4, rows = 2;
    const float w = (float) getWidth() / cols, h = (float) getHeight() / rows;
    for (int i = 0; i < (int) cards.size(); ++i)
        cards[(size_t) i]->setBounds (juce::Rectangle<float> (w * (float) (i % cols), h * (float) (i / cols), w, h).toNearestInt());
}

std::optional<juce::Point<float>> PolaroidGrid::getPin (int slot, juce::Component& relativeTo) const
{
    for (auto& c : cards)
        if (c->getSlot() == slot)
            return relativeTo.getLocalPoint (c.get(), c->getPinPosition());
    return std::nullopt;
}
} // namespace vk::ui
