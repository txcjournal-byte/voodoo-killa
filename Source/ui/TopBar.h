#pragma once

#include "Theme.h"

class VoodooKillaAudioProcessor;

namespace vk::ui
{
/** [◀ NAME ▶ | ★ | A|B | ↶ ↷ | 💾 | 🎲] on a strip of paper. */
class TopBar : public juce::Component, public juce::TooltipClient
{
public:
    explicit TopBar (VoodooKillaAudioProcessor&);

    void refresh();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    enum Region { prev, name, next, star, ab, undo, redo, save, dice, numRegions };
    juce::Rectangle<float> regionBounds (Region) const;
    int regionAt (juce::Point<float>) const;
    void showPresetMenu();
    void showSaveDialog();

    VoodooKillaAudioProcessor& proc;
    int hover = -1;
    juce::String shownName;
    std::unique_ptr<juce::Component> savePanel;
};

/** KILL: random preset. Right click = Mutate (±15 %). */
class KillButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KillButton (VoodooKillaAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    VoodooKillaAudioProcessor& proc;
    bool pressed = false;
};

/** Small icon toggle / button (lock, settings). */
class IconButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { lock, gear };
    IconButton (Kind, std::function<bool()> isOn, std::function<void (IconButton&)> onClick);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    Kind kind;
    std::function<bool()> isOn;
    std::function<void (IconButton&)> click;
};
} // namespace vk::ui
