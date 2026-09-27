#pragma once

#include "Theme.h"

class VoodooKillaAudioProcessor;

namespace vk::ui
{
/** 12 masking-tape labels; the active category is red. Number 1-12 = MIDI C2..B2. */
class CategoryList : public juce::Component, public juce::TooltipClient
{
public:
    explicit CategoryList (VoodooKillaAudioProcessor&);

    void refresh();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    juce::Rectangle<float> itemBounds (int i) const;
    int itemAt (juce::Point<float>) const;

    VoodooKillaAudioProcessor& proc;
    int active = -1;
    int hover = -1;
};
} // namespace vk::ui
