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
    const float top = 26.0f;
    const float h = ((float) getHeight() - top - 8.0f) / (float) vk::kNumCategories;
    return { 8.0f, top + h * (float) i, (float) getWidth() - 16.0f, h - 2.0f };
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
    g.setColour (Colours::accent);
    g.setFont (Theme::mono (8.0f, 0.2f));
    g.drawText ("CATEGORIES", juce::Rectangle<float> (14.0f, 8.0f, (float) getWidth() - 20.0f, 14.0f), juce::Justification::centredLeft, false);

    const auto& cats = proc.library.getCategories();
    for (int i = 0; i < (int) cats.size() && i < vk::kNumCategories; ++i)
    {
        const auto r = itemBounds (i);
        const bool on = i == active;
        if (on)
        {
            juce::Path p;
            p.addRoundedRectangle (r, 8.0f);
            juce::DropShadow (Colours::glow.withAlpha (0.25f), 12, {}).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (Colours::accent.withAlpha (0.30f), r.getX(), 0.0f,
                                                     Colours::accent.withAlpha (0.05f), r.getRight(), 0.0f, false));
            g.fillPath (p);
            g.setColour (Colours::accentL);
            g.fillRoundedRectangle (r.withWidth (2.0f).reduced (0.0f, 4.0f), 1.0f);
        }
        else if (i == hover)
        {
            g.setColour (Colours::accent.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        g.setColour (on ? Colours::accentL : Colours::muted);
        g.setFont (Theme::monoLight (7.5f, 0.1f));
        g.drawText (juce::String (i + 1).paddedLeft ('0', 2), r.withTrimmedLeft (8.0f).withWidth (18.0f), juce::Justification::centredLeft, false);

        g.setColour (on ? Colours::text : Colours::text2);
        g.setFont (Theme::mono (9.5f, 0.12f));
        g.drawFittedText (cats[(size_t) i].name.toUpperCase(), r.withTrimmedLeft (28.0f).reduced (0.0f, 1.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.75f);
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
    if (i != hover) { hover = i; repaint(); }
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
