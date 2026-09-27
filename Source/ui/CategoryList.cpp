#include "CategoryList.h"
#include "../PluginProcessor.h"

namespace vk::ui
{
CategoryList::CategoryList (VoodooKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refresh();
}

void CategoryList::refresh()
{
    if (proc.getCurrentCategory() != active)
    {
        active = proc.getCurrentCategory();
        repaint();
    }
}

juce::Rectangle<float> CategoryList::itemBounds (int i) const
{
    const float h = (float) getHeight() / (float) vk::kNumCategories;
    return { 0.0f, h * (float) i + 2.0f, (float) getWidth(), h - 6.0f };
}

int CategoryList::itemAt (juce::Point<float> p) const
{
    for (int i = 0; i < vk::kNumCategories; ++i)
        if (itemBounds (i).contains (p))
            return i;
    return -1;
}

void CategoryList::paint (juce::Graphics& g)
{
    const auto& cats = proc.library.getCategories();
    for (int i = 0; i < (int) cats.size() && i < vk::kNumCategories; ++i)
    {
        auto r = itemBounds (i).withTrimmedLeft (14.0f);
        const bool on = i == active;
        const float angle = ((float) ((i * 7919) % 5) - 2.0f) * 0.0035f;
        Theme::drawTape (g, r.withTrimmedRight (on ? 0.0f : 4.0f), angle, (uint32_t) (i * 131 + 7),
                         on ? Colours::red : (i == hover ? Colours::tape.brighter (0.15f) : Colours::tape));

        // index (MIDI C2 + i)
        g.setColour (on ? Colours::red : Colours::grey);
        g.setFont (Theme::typewriter (11.0f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (0.0f, r.getY(), 14.0f, r.getHeight()), juce::Justification::centredLeft, false);

        g.setColour (on ? Colours::ink : Colours::ink.withAlpha (0.9f));
        g.setFont (Theme::typewriter (15.5f));
        g.drawFittedText (cats[(size_t) i].name, r.reduced (9.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.72f);
    }
}

void CategoryList::mouseUp (const juce::MouseEvent& e)
{
    const int i = itemAt (e.position);
    if (i >= 0)
    {
        proc.setCurrentCategory (i);
        refresh();
    }
}

void CategoryList::mouseMove (const juce::MouseEvent& e)
{
    const int i = itemAt (e.position);
    if (i != hover)
    {
        hover = i;
        repaint();
    }
}

void CategoryList::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

juce::String CategoryList::getTooltip()
{
    const int i = itemAt (getMouseXYRelative().toFloat());
    const auto& cats = proc.library.getCategories();
    if (i < 0 || i >= (int) cats.size())
        return {};
    static const char* notes[] = { "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2" };
    return cats[(size_t) i].desc + " (MIDI " + notes[i] + ")";
}
} // namespace vk::ui
