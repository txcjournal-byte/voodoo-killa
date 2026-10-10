#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace vk::ui
{
/** TrapWin "Chrome" identity (same tokens as the trapwin.com theme). */
namespace Colours
{
    inline const juce::Colour bg       { 0xff04060a };
    inline const juce::Colour card     { 0xff0a0f18 };
    inline const juce::Colour card2    { 0xff0b111c };
    inline const juce::Colour well     { 0xff05070b };
    inline const juce::Colour track    { 0xff121a28 };
    inline const juce::Colour text     { 0xffe6ecf7 };
    inline const juce::Colour text2    { 0xffa9b6cc };
    inline const juce::Colour muted    { 0xff6f7c93 };
    inline const juce::Colour accent   { 0xff7fa6e8 };
    inline const juce::Colour accentL  { 0xff9fc3ff };
    inline const juce::Colour accentD  { 0xff4f78c4 };
    inline const juce::Colour glow     { 0xff4676c4 };
    inline const juce::Colour line     { 0x299fc3ff };   // ~16 %
    inline const juce::Colour line2    { 0x4d9fc3ff };   // ~30 %
}

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

    juce::Typeface::Ptr displayFace, boldFace, monoFace, monoLightFace;
    std::map<juce::String, juce::Image> images;
    std::map<juce::String, juce::Image> photos;

    JUCE_DECLARE_NON_COPYABLE (ThemeResources)
};

struct Theme
{
    // ---- fonts
    static juce::Font display (float height);     // Archivo Black Italic, expanded (headlines)
    static juce::Font bold (float height);        // Archivo ExtraBold Italic (names, values)
    static juce::Font mono (float height, float tracking = 0.16f);       // IBM Plex Mono Medium, upper-case labels
    static juce::Font monoLight (float height, float tracking = 0.16f);  // IBM Plex Mono Regular

    // ---- assets (BinaryData by original file name, cached)
    static juce::Image getAsset (const juce::String& fileName);
    /** Card photo: grayscale, slightly blue-tinted. `isPlaceholder` = the preset has no own photo yet. */
    static juce::Image photo (const juce::String& fileName, bool& isPlaceholder);

    // ---- surfaces
    static void drawBackground (juce::Graphics&, juce::Rectangle<float> area);
    static void drawCard (juce::Graphics&, juce::Rectangle<float> area, float radius = 14.0f, float glow = 0.10f);
    static void drawWell (juce::Graphics&, juce::Rectangle<float> area, float radius);
    static void drawChromePill (juce::Graphics&, juce::Rectangle<float> area, float glow = 0.45f);
    static juce::ColourGradient chromeGradient (juce::Rectangle<float> area);
    static void drawChromeText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                                const juce::Font& font, juce::Justification just, bool glow = true);
    static void drawGlowDot (juce::Graphics&, juce::Point<float> centre, float radius, float glowRadius);
    static void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

    // ---- icons
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
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
};
} // namespace vk::ui
