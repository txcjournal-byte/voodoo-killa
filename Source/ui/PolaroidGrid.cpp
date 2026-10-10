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
    return getLocalBounds().toFloat().reduced (5.0f, 9.0f).withTrimmedTop (1.0f);
}

juce::Rectangle<float> PolaroidCard::photoBounds() const
{
    const auto f = frameBounds().reduced (7.0f);
    return f.withHeight (f.getHeight() - 30.0f);
}

juce::Point<float> PolaroidCard::getPinPosition() const
{
    const auto f = frameBounds();
    return { f.getRight() - 20.0f, f.getY() };
}

void PolaroidCard::paint (juce::Graphics& g)
{
    const auto f = frameBounds();
    juce::Path outline;
    outline.addRoundedRectangle (f, 14.0f);
    if (slot > 0)   // glow behind the card
        juce::DropShadow (Colours::glow.withAlpha (slot == 1 ? 0.75f : 0.5f), 18, {}).drawForPath (g, outline);
    Theme::drawCard (g, f, 14.0f, slot > 0 ? 0.0f : 0.08f);
    if (slot > 0)
    {
        g.setColour (slot == 1 ? Colours::accentL : Colours::accent);
        g.strokePath (outline, juce::PathStrokeType (1.6f));
    }

    // photo: rounded, grayscale, cool fade to the card
    const auto ph = photoBounds();
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (ph, 9.0f);
        g.reduceClipRegion (clip);
        g.setColour (Colours::well);
        g.fillRect (ph);
        if (photo.isValid())
            g.drawImage (photo, ph, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        g.setGradientFill (juce::ColourGradient (Colours::glow.withAlpha (0.10f), 0.0f, ph.getY(),
                                                 Colours::bg.withAlpha (0.55f), 0.0f, ph.getBottom(), false));
        g.fillRect (ph);
        if (placeholder)
        {
            g.setColour (Colours::muted);
            g.setFont (Theme::mono (8.0f, 0.12f));
            g.drawFittedText (name.toUpperCase(), ph.reduced (8.0f).toNearestInt(), juce::Justification::centred, 2);
        }
    }
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (ph, 9.0f, 1.0f);

    // name + description
    const auto text = f.reduced (8.0f, 0.0f).withTop (ph.getBottom() + 5.0f);
    g.setColour (Colours::text);
    g.setFont (Theme::bold (12.5f));
    g.drawFittedText (name.toUpperCase(), text.withHeight (14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    g.setColour (Colours::muted);
    g.setFont (Theme::mono (7.0f, 0.14f));
    g.drawFittedText (desc.toUpperCase(), text.withTrimmedTop (15.0f).withHeight (10.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);

    // A / B chip (chrome pill on the top edge)
    if (slot > 0)
    {
        const auto chip = juce::Rectangle<float> (19.0f, 18.0f).withCentre (getPinPosition());
        Theme::drawChromePill (g, chip, 0.8f);
        g.setColour (Colours::well);
        g.setFont (Theme::bold (11.0f));
        g.drawText (slot == 1 ? "A" : "B", chip, juce::Justification::centred, false);
    }
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
        g.setColour (Colours::accentL.withAlpha (0.25f + 0.45f * p));
        g.drawRoundedRectangle (f.expanded (1.5f), 15.0f, 1.5f + p);

        // LIVE badge on the photo
        const auto badge = juce::Rectangle<float> (f.getX() + 13.0f, f.getY() + 13.0f, 40.0f, 14.0f);
        g.setColour (Colours::bg.withAlpha (0.7f));
        g.fillRoundedRectangle (badge, 7.0f);
        g.setColour (Colours::line2);
        g.drawRoundedRectangle (badge, 7.0f, 1.0f);
        const auto dot = juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ badge.getX() + 8.0f, badge.getCentreY() });
        g.setColour (Colours::accentL.withAlpha (0.4f + 0.6f * p));
        g.fillEllipse (dot);
        g.setColour (Colours::text);
        g.setFont (Theme::mono (6.5f, 0.2f));
        g.drawText ("LIVE", badge.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);
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
            return relativeTo.getLocalPoint (c.get(), c->getPinPosition().translated (0.0f, -9.0f));   // top of the chip
    return std::nullopt;
}
} // namespace vk::ui
