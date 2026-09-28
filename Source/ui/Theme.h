#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace vk::ui
{
namespace Colours
{
    inline const juce::Colour background { 0xff0c0c0c };
    inline const juce::Colour ink        { 0xff121212 };
    inline const juce::Colour paper      { 0xffd3ccc2 };
    inline const juce::Colour paperDark  { 0xffbdb5aa };
    inline const juce::Colour tape       { 0xffc6beb3 };
    inline const juce::Colour grey       { 0xff8c8c8c };
    inline const juce::Colour greyDark   { 0xff4a4a4a };
    inline const juce::Colour red        { 0xffff2e3e };
    inline const juce::Colour redDark    { 0xffb3202c };
}

/** Grid unit (the layout is built on an 8 px grid at 100 %). */
constexpr int kGrid = 8;
constexpr int kBaseWidth = 1000;
constexpr int kBaseHeight = 625;

/**
    Fonts and bitmaps used by the UI. Owned by the editor through a juce::SharedResourcePointer, so they are
    released when the last plugin window closes (never by static destructors during DLL unload).
*/
struct ThemeResources
{
    ThemeResources();
    ~ThemeResources();

    juce::Typeface::Ptr typewriterFace, markerFace;
    std::map<juce::String, juce::Image> images;
    juce::Image backgroundTile, paperTile;

    JUCE_DECLARE_NON_COPYABLE (ThemeResources)
};

/**
    Fonts, textures and small drawing helpers.
    Every bitmap is looked up by its fixed file name in BinaryData (Resources/...). When an asset is
    missing a procedural placeholder is generated, so artwork can be dropped in without code changes.
*/
struct Theme
{
    static juce::Font typewriter (float height);   // Special Elite
    static juce::Font marker (float height);       // Permanent Marker

    /** BinaryData lookup by original file name (e.g. "halfcut_2.jpg"). Cached. Invalid image if missing. */
    static juce::Image getAsset (const juce::String& fileName);

    static const juce::Image& backgroundTile();
    static const juce::Image& paperTile();

    static void fillBackground (juce::Graphics&, juce::Rectangle<float> area);
    static void drawPaper (juce::Graphics&, juce::Rectangle<float> area, uint32_t seed, bool tornEdges = false,
                           juce::Colour tint = Colours::paper);
    static void drawTape (juce::Graphics&, juce::Rectangle<float> area, float angleRadians, uint32_t seed,
                          juce::Colour tint = Colours::tape);
    static void drawPin (juce::Graphics&, juce::Point<float> centre, float radius);
    static void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

    /** Photo for a preset (Resources/Photos/<photo>) or the placeholder. `isPlaceholder` tells the caller to label it. */
    static juce::Image photo (const juce::String& fileName, bool& isPlaceholder);

    // simple vector icons for the top bar
    static juce::Path starIcon (juce::Rectangle<float>);
    static juce::Path undoIcon (juce::Rectangle<float>, bool redo);
    static juce::Path saveIcon (juce::Rectangle<float>);
    static void drawDice (juce::Graphics&, juce::Rectangle<float>, juce::Colour body, juce::Colour pips);
    static juce::Path lockIcon (juce::Rectangle<float>, bool locked);
    static juce::Path gearIcon (juce::Rectangle<float>);
};

class KillaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KillaLookAndFeel();

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
} // namespace vk::ui
