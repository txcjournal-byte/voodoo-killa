#pragma once

#include "Knob.h"

class VoodooKillaAudioProcessor;

namespace vk::ui
{
/**
    HALFTIME ENGINE: SPEED (1.5x / 2x / 3x / 4x), LOOP (1/4 bar .. 4 bars, triplet), MODE (TAPE / KEY / HOLD),
    REVERSE, and SMOOTH / BRAKE / FADE. Edits the halftime settings stored in slot A's preset.
*/
class HalftimePanel : public juce::Component, public juce::TooltipClient
{
public:
    explicit HalftimePanel (VoodooKillaAudioProcessor&);

    void refresh();   // selection changed
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    struct Hit { int row = -1, index = -1; };
    juce::Rectangle<float> rowArea (int row) const;       // segmented control of a row (0 speed, 1 loop, 2 mode, 3 rev)
    juce::Rectangle<float> cell (int row, int index) const;
    static int numCells (int row);
    Hit hitAt (juce::Point<float>) const;
    int selectedIndex (int row) const;
    bool isOn() const;
    juce::String pitchInfo() const;
    void click (Hit);

    VoodooKillaAudioProcessor& proc;
    MiniKnob smooth { "Smooth", 0 }, brake { "Brake", 7 }, fade { "Fade", 8 };
    Hit hover;
};
} // namespace vk::ui
