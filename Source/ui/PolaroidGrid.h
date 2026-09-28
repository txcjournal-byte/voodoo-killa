#pragma once

#include "Theme.h"

class VoodooKillaAudioProcessor;

namespace vk::ui
{
/** One polaroid (preset card): photo, marker name, typewriter description, A/B pin, pulse while running. */
class PolaroidCard : public juce::Component, public juce::SettableTooltipClient
{
public:
    PolaroidCard (VoodooKillaAudioProcessor&, int cardIndex);

    /** slot: 0 = none, 1 = A, 2 = B. */
    void setContent (int libraryIndex, int slot);
    /** Returns true when the glow changed (the grid then repaints this card's area). */
    bool setPulse (float amount);
    float getPulse() const noexcept { return pulse; }
    int getSlot() const noexcept { return slot; }
    juce::Rectangle<float> getFrameInParent() const;

    /** Pin position in this component's coordinates. */
    juce::Point<float> getPinPosition() const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> frameBounds() const;
    juce::Rectangle<float> photoBounds() const;

    VoodooKillaAudioProcessor& proc;
    const int card;
    int libraryIndex = -1;
    int slot = 0;
    float pulse = 0.0f;
    juce::String name, desc;
    juce::Image photo;
    bool placeholder = true;
    bool holding = false;
};

/** 4 x 2 grid of the current category's cards. */
class PolaroidGrid : public juce::Component
{
public:
    explicit PolaroidGrid (VoodooKillaAudioProcessor&);

    void refresh();                 // selection / category changed
    void updatePulse (bool active, float phase);
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    /** Pin positions (in `relativeTo` coordinates) of the visible A / B cards. */
    std::optional<juce::Point<float>> getPin (int slot, juce::Component& relativeTo) const;

private:
    VoodooKillaAudioProcessor& proc;
    std::vector<std::unique_ptr<PolaroidCard>> cards;
};
} // namespace vk::ui
