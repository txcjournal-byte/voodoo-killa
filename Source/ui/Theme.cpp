#include "Theme.h"
#include <BinaryData.h>

namespace vk::ui
{
namespace
{
    const void* findResource (const juce::String& originalName, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (originalName == BinaryData::originalFilenames[i])
                return BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    juce::Typeface::Ptr loadTypeface (const char* originalName)
    {
        int size = 0;
        if (const void* data = findResource (originalName, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    juce::Font makeFont (const juce::Typeface::Ptr& tf, float height, const char* fallback, bool boldFallback)
    {
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf).withHeight (height));
        return juce::Font (juce::FontOptions (fallback, height, boldFallback ? juce::Font::bold : juce::Font::plain));
    }

    ThemeResources& res()
    {
        // the editor holds a SharedResourcePointer, so this only bumps a reference count
        juce::SharedResourcePointer<ThemeResources> ptr;
        return *ptr;
    }
}

// ===========================================================================
// Nothing graphical has static storage duration: static destructors run while Windows unloads the plugin
// DLL (loader lock held), and releasing DirectWrite fonts / images there can hang the host.
ThemeResources::ThemeResources()
{
    displayFace   = loadTypeface ("Archivo-BlackItalicExpanded.ttf");
    boldFace      = loadTypeface ("Archivo-ExtraBoldItalic.ttf");
    monoFace      = loadTypeface ("IBMPlexMono-Medium.ttf");
    monoLightFace = loadTypeface ("IBMPlexMono-Regular.ttf");
}

ThemeResources::~ThemeResources() = default;

juce::Font Theme::display (float h)  { return makeFont (res().displayFace, h, "Arial", true); }
juce::Font Theme::bold (float h)     { return makeFont (res().boldFace, h, "Arial", true); }
juce::Font Theme::mono (float h, float tracking)      { return makeFont (res().monoFace, h, "Courier New", false).withExtraKerningFactor (tracking); }
juce::Font Theme::monoLight (float h, float tracking) { return makeFont (res().monoLightFace, h, "Courier New", false).withExtraKerningFactor (tracking); }

// ===========================================================================
juce::Image Theme::getAsset (const juce::String& fileName)
{
    auto& cache = res().images;
    if (auto it = cache.find (fileName); it != cache.end())
        return it->second;

    juce::Image img;
    int size = 0;
    if (const void* data = findResource (fileName, size))
        img = juce::ImageFileFormat::loadFrom (data, (size_t) size);   // not ImageCache: freed with the editor
    cache[fileName] = img;
    return img;
}

juce::Image Theme::photo (const juce::String& fileName, bool& isPlaceholder)
{
    auto src = getAsset (fileName);
    isPlaceholder = ! src.isValid();
    const auto key = isPlaceholder ? juce::String ("placeholder.jpg") : fileName;
    if (isPlaceholder)
        src = getAsset (key);

    auto& cache = res().photos;
    if (auto it = cache.find (key); it != cache.end())
        return it->second;
    if (! src.isValid())
        return {};

    // grayscale, a touch more contrast, cool blue shadows (matches the TrapWin product shots)
    juce::Image out (juce::Image::RGB, src.getWidth(), src.getHeight(), false);
    {
        juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData o (out, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < src.getHeight(); ++y)
        {
            for (int x = 0; x < src.getWidth(); ++x)
            {
                const auto c = in.getPixelColour (x, y);
                float l = 0.299f * c.getFloatRed() + 0.587f * c.getFloatGreen() + 0.114f * c.getFloatBlue();
                l = juce::jlimit (0.0f, 1.0f, (l - 0.5f) * 1.08f + 0.5f - 0.04f);
                const float shadow = 1.0f - l;
                o.setPixelColour (x, y, juce::Colour::fromFloatRGBA (l * (1.0f - 0.06f * shadow),
                                                                    l * (1.0f - 0.02f * shadow),
                                                                    juce::jmin (1.0f, l + 0.05f * shadow * l + 0.015f), 1.0f));
            }
        }
    }
    cache[key] = out;
    return out;
}

// ===========================================================================
void Theme::drawBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Colours::bg);
    g.fillRect (r);

    juce::ColourGradient top (Colours::glow.withAlpha (0.22f), r.getX() + r.getWidth() * 0.72f, r.getY() - r.getHeight() * 0.10f,
                              Colours::glow.withAlpha (0.0f), r.getX() + r.getWidth() * 0.72f + 620.0f, r.getY() + 220.0f, true);
    g.setGradientFill (top);
    g.fillRect (r);

    juce::ColourGradient low (Colours::glow.withAlpha (0.10f), r.getX(), r.getBottom() + 40.0f,
                              Colours::glow.withAlpha (0.0f), r.getX() + 520.0f, r.getBottom() - 200.0f, true);
    g.setGradientFill (low);
    g.fillRect (r);
}

void Theme::drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius, float glow)
{
    juce::Path p;
    p.addRoundedRectangle (r, radius);
    if (glow > 0.0f)
        juce::DropShadow (Colours::glow.withAlpha (glow), 22, {}).drawForPath (g, p);

    juce::ColourGradient fill (Colours::card2, r.getX(), r.getY(), Colours::card, r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (p);
    g.setColour (Colours::line);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void Theme::drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

juce::ColourGradient Theme::chromeGradient (juce::Rectangle<float> r)
{
    juce::ColourGradient grad (juce::Colour (0xfff4f6fb), r.getX(), r.getY(), juce::Colour (0xffe6eaf2), r.getX(), r.getBottom(), false);
    grad.addColour (0.48, juce::Colour (0xffc9d0dd));
    grad.addColour (0.52, juce::Colour (0xff8e98ab));
    return grad;
}

void Theme::drawChromePill (juce::Graphics& g, juce::Rectangle<float> r, float glow)
{
    juce::Path p;
    p.addRoundedRectangle (r, r.getHeight() * 0.5f);
    if (glow > 0.0f)
        juce::DropShadow (Colours::glow.withAlpha (glow), 16, {}).drawForPath (g, p);
    g.setGradientFill (chromeGradient (r));
    g.fillPath (p);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawLine (r.getX() + r.getHeight() * 0.5f, r.getY() + 0.8f, r.getRight() - r.getHeight() * 0.5f, r.getY() + 0.8f, 0.8f);
}

void Theme::drawChromeText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                            const juce::Font& font, juce::Justification just, bool glow)
{
    juce::GlyphArrangement ga;
    ga.addFittedText (font, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1, 0.8f);
    juce::Path p;
    ga.createPath (p);
    if (p.isEmpty())
        return;

    if (glow)
        juce::DropShadow (Colours::glow.withAlpha (0.55f), 12, {}).drawForPath (g, p);

    const auto b = p.getBounds();
    juce::ColourGradient chrome (juce::Colour (0xff9aa3b6), b.getX(), b.getY(), juce::Colour (0xfff7f9fd), b.getX(), b.getBottom(), false);
    chrome.addColour (0.32, juce::Colour (0xfff5f7fc));
    chrome.addColour (0.47, juce::Colour (0xff9099ad));
    chrome.addColour (0.50, juce::Colour (0xff151b29));
    chrome.addColour (0.57, juce::Colour (0xff6f7a90));
    chrome.addColour (0.82, juce::Colour (0xffdfe5ef));
    g.setGradientFill (chrome);
    g.fillPath (p);
}

void Theme::drawGlowDot (juce::Graphics& g, juce::Point<float> c, float radius, float glowRadius)
{
    juce::ColourGradient glow (Colours::accent.withAlpha (0.75f), c.x, c.y, Colours::accent.withAlpha (0.0f), c.x + glowRadius, c.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (glowRadius * 2.0f, glowRadius * 2.0f).withCentre (c));
    g.setColour (Colours::accentL);
    g.drawEllipse (juce::Rectangle<float> (radius * 3.2f, radius * 3.2f).withCentre (c), 1.2f);
    g.setColour (Colours::text);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
}

void Theme::drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    const bool hiDpi = g.getInternalContext().getPhysicalPixelScaleFactor() > 1.01f;
    auto img = getAsset (hiDpi ? "logo@2x.png" : "logo.png");
    if (! img.isValid())
        img = getAsset ("logo@2x.png");
    if (img.isValid())
    {
        g.drawImage (img, area, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
        return;
    }
    drawChromeText (g, "TRAPWIN", area, display (area.getHeight() * 0.6f), juce::Justification::centredLeft);
}

// ===========================================================================
juce::Path Theme::starIcon (juce::Rectangle<float> r)
{
    juce::Path p;
    p.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
    return p;
}

juce::Path Theme::undoIcon (juce::Rectangle<float> r, bool redo)
{
    juce::Path p;
    const auto c = r.getCentre().translated (0.0f, r.getHeight() * 0.08f);
    const float rad = r.getWidth() * 0.34f;
    p.addCentredArc (c.x, c.y, rad, rad, 0.0f, -juce::MathConstants<float>::pi * 0.95f, juce::MathConstants<float>::pi * 0.35f, true);
    juce::PathStrokeType (r.getWidth() * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (p, p);
    const float ax = c.x - rad * 0.99f, ay = c.y - rad * 0.1f;
    juce::Path arrow;
    arrow.addTriangle (ax - r.getWidth() * 0.18f, ay - r.getHeight() * 0.02f, ax + r.getWidth() * 0.16f, ay - r.getHeight() * 0.02f,
                       ax, ay + r.getHeight() * 0.2f);
    p.addPath (arrow);
    if (redo)
        p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, r.getCentreX(), r.getCentreY()));
    return p;
}

juce::Path Theme::saveIcon (juce::Rectangle<float> r)
{
    r = r.reduced (r.getWidth() * 0.12f);
    juce::Path p;
    p.addRoundedRectangle (r, r.getWidth() * 0.08f);
    juce::Path hole;
    hole.addRectangle (r.getX() + r.getWidth() * 0.22f, r.getY(), r.getWidth() * 0.5f, r.getHeight() * 0.32f);
    hole.addRectangle (r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.52f, r.getWidth() * 0.64f, r.getHeight() * 0.36f);
    p.addPath (hole);
    p.setUsingNonZeroWinding (false);
    return p;
}

void Theme::drawDice (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour body, juce::Colour pips)
{
    r = r.reduced (r.getWidth() * 0.08f);
    g.setColour (body);
    g.fillRoundedRectangle (r, r.getWidth() * 0.22f);
    g.setColour (pips);
    const float d = r.getWidth() * 0.18f;
    auto pip = [&] (float fx, float fy) { g.fillEllipse (r.getX() + r.getWidth() * fx - d * 0.5f, r.getY() + r.getHeight() * fy - d * 0.5f, d, d); };
    pip (0.28f, 0.28f); pip (0.72f, 0.28f); pip (0.5f, 0.5f); pip (0.28f, 0.72f); pip (0.72f, 0.72f);
}

juce::Path Theme::lockIcon (juce::Rectangle<float> r, bool locked)
{
    juce::Path p;
    const auto body = juce::Rectangle<float> (r.getWidth() * 0.7f, r.getHeight() * 0.5f)
                          .withCentre ({ r.getCentreX(), r.getY() + r.getHeight() * 0.68f });
    p.addRoundedRectangle (body, r.getWidth() * 0.08f);
    juce::Path shackle;
    const float sw = body.getWidth() * 0.62f;
    const float sx = body.getCentreX() - sw * 0.5f + (locked ? 0.0f : sw * 0.45f);
    shackle.addCentredArc (sx + sw * 0.5f, body.getY() - 0.5f, sw * 0.5f, r.getHeight() * 0.3f, 0.0f,
                           -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    juce::PathStrokeType (r.getWidth() * 0.12f).createStrokedPath (shackle, shackle);
    p.addPath (shackle);
    juce::Path hole;
    hole.addEllipse (juce::Rectangle<float> (body.getWidth() * 0.18f, body.getWidth() * 0.18f).withCentre (body.getCentre()));
    p.addPath (hole);
    p.setUsingNonZeroWinding (false);
    return p;
}

juce::Path Theme::gearIcon (juce::Rectangle<float> r)
{
    juce::Path p;
    const auto c = r.getCentre();
    const float ro = r.getWidth() * 0.46f, ri = r.getWidth() * 0.34f;
    const int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / (teeth * 2);
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (teeth * 2);
        const float rr = (i % 2 == 0) ? ro : ri;
        const auto p0 = c.getPointOnCircumference (rr, a0);
        const auto p1 = c.getPointOnCircumference (rr, a1);
        if (i == 0) p.startNewSubPath (p0); else p.lineTo (p0);
        p.lineTo (p1);
    }
    p.closeSubPath();
    p.addEllipse (juce::Rectangle<float> (ri * 0.9f, ri * 0.9f).withCentre (c));
    p.setUsingNonZeroWinding (false);
    return p;
}

// ===========================================================================
KillaLookAndFeel::KillaLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, Colours::card);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
    setColour (juce::PopupMenu::headerTextColourId, Colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::card2);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::TextEditor::backgroundColourId, Colours::well);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::highlightColourId, Colours::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, Colours::line2);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accentL);
    setColour (juce::CaretComponent::caretColourId, Colours::accentL);
    setColour (juce::TextButton::buttonColourId, Colours::card2);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::well);
}

juce::Rectangle<int> KillaLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = Theme::monoLight (11.5f, 0.02f);
    const int w = juce::jmin (340, juce::GlyphArrangement::getStringWidthInt (f, tipText) + 24);
    const int h = 26;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 16, w, h)
               .constrainedWithin (parentArea);
}

void KillaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (Colours::card2);
    g.setColour (Colours::line2);
    g.drawRect (r, 1.0f);
    g.setColour (Colours::text);
    g.setFont (Theme::monoLight (11.5f, 0.02f));
    g.drawFittedText (text, 12, 0, width - 24, height, juce::Justification::centredLeft, 1);
}

juce::Font KillaLookAndFeel::getPopupMenuFont() { return Theme::mono (12.5f, 0.06f); }

void KillaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Colours::card);
    g.setColour (Colours::line2);
    g.drawRect (0, 0, width, height, 1);
}

void KillaLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                          bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                          const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (Colours::line);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (Colours::accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
    }
    if (isTicked)
    {
        g.setColour (Colours::accentL);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 9.0f, (float) r.getCentreY() }));
    }
    g.setColour (! isActive ? Colours::muted : (textColour != nullptr ? *textColour : (isTicked ? Colours::accentL : Colours::text)));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text.toUpperCase(), r.withTrimmedLeft (20).withTrimmedRight (hasSubMenu ? 18 : 6), juce::Justification::centredLeft, 1);
    if (hasSubMenu)
    {
        juce::Path arrow;
        const float x = (float) r.getRight() - 10.0f, y = (float) r.getCentreY();
        arrow.addTriangle (x - 3.0f, y - 4.0f, x - 3.0f, y + 4.0f, x + 2.0f, y);
        g.setColour (Colours::text2);
        g.fillPath (arrow);
    }
    juce::ignoreUnused (shortcutKeyText, icon);
}

juce::Font KillaLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return Theme::bold ((float) h * 0.48f); }

void KillaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    if (b.getToggleState() || b.getProperties()["chrome"])
    {
        Theme::drawChromePill (g, r, highlighted ? 0.55f : 0.35f);
        b.setColour (juce::TextButton::textColourOffId, Colours::well);
    }
    else
    {
        g.setColour (down ? Colours::track : Colours::well);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (highlighted ? Colours::accentL : Colours::line2);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }
}

void KillaLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (Colours::well);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, (float) height * 0.5f);
}

void KillaLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    g.setColour (ed.hasKeyboardFocus (true) ? Colours::accentL : Colours::line2);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, (float) height * 0.5f, 1.0f);
}

void KillaLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    juce::DropShadow (Colours::glow.withAlpha (0.35f), 18, {}).drawForPath (g, path);
    g.setColour (Colours::card);
    g.fillPath (path);
    g.setColour (Colours::line2);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
} // namespace vk::ui
