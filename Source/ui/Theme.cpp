#include "Theme.h"
#include <BinaryData.h>
#include <map>

namespace vk::ui
{
namespace
{
    juce::Typeface::Ptr loadTypeface (const char* originalName)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (juce::String (BinaryData::originalFilenames[i]) == originalName)
            {
                int size = 0;
                if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            }
        }
        return nullptr;
    }

    juce::Font makeFont (const juce::Typeface::Ptr& tf, float height, const char* fallbackName)
    {
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf).withHeight (height));
        return juce::Font (juce::FontOptions (fallbackName, height, juce::Font::bold));
    }

    uint32_t mix (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }

    float noise01 (uint32_t& s)
    {
        s = mix (s + 0x9E3779B9u);
        return (float) (s >> 8) / 16777216.0f;
    }

    juce::Image makeGrainTile (juce::Colour base, float amount, int fibres, uint32_t seed)
    {
        constexpr int size = 256;
        juce::Image img (juce::Image::RGB, size, size, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        uint32_t s = seed;
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float n = (noise01 (s) - 0.5f) * amount;
                bd.setPixelColour (x, y, base.brighter (n > 0 ? n : 0.0f).darker (n < 0 ? -n : 0.0f));
            }
        }
        juce::Graphics g (img);
        for (int i = 0; i < fibres; ++i)
        {
            const float x = noise01 (s) * size, y = noise01 (s) * size;
            const float a = noise01 (s) * juce::MathConstants<float>::twoPi;
            const float len = 4.0f + noise01 (s) * 18.0f;
            g.setColour (base.darker (0.25f + noise01 (s) * 0.3f).withAlpha (0.18f));
            g.drawLine (x, y, x + std::cos (a) * len, y + std::sin (a) * len, 0.6f);
        }
        return img;
    }
}

// ===========================================================================
juce::Font Theme::typewriter (float h)
{
    static auto tf = loadTypeface ("SpecialElite-Regular.ttf");
    return makeFont (tf, h, "Courier New");
}

juce::Font Theme::marker (float h)
{
    static auto tf = loadTypeface ("PermanentMarker-Regular.ttf");
    return makeFont (tf, h, "Arial");
}

juce::Image Theme::getAsset (const juce::String& fileName)
{
    static std::map<juce::String, juce::Image> cache;
    if (auto it = cache.find (fileName); it != cache.end())
        return it->second;

    juce::Image img;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (juce::String (BinaryData::originalFilenames[i]) == fileName)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                img = juce::ImageCache::getFromMemory (data, size);
            break;
        }
    }
    cache[fileName] = img;
    return img;
}

const juce::Image& Theme::backgroundTile()
{
    static const juce::Image img = []
    {
        auto asset = getAsset ("background.png");
        return asset.isValid() ? asset : makeGrainTile (Colours::background, 0.35f, 40, 1234u);
    }();
    return img;
}

const juce::Image& Theme::paperTile()
{
    static const juce::Image img = []
    {
        auto asset = getAsset ("paper.png");
        return asset.isValid() ? asset : makeGrainTile (Colours::paper, 0.12f, 260, 777u);
    }();
    return img;
}

void Theme::fillBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setTiledImageFill (backgroundTile(), 0, 0, 1.0f);
    g.fillRect (area);
}

void Theme::drawPaper (juce::Graphics& g, juce::Rectangle<float> r, uint32_t seed, bool torn, juce::Colour tint)
{
    juce::Path shape;
    if (torn)
    {
        // decent, subtle torn edges: small jitter along each side
        uint32_t s = seed;
        const float step = 6.0f, j = 1.6f;
        shape.startNewSubPath (r.getX(), r.getY());
        for (float x = r.getX() + step; x < r.getRight(); x += step) shape.lineTo (x, r.getY() + noise01 (s) * j);
        for (float y = r.getY() + step; y < r.getBottom(); y += step) shape.lineTo (r.getRight() - noise01 (s) * j, y);
        for (float x = r.getRight() - step; x > r.getX(); x -= step) shape.lineTo (x, r.getBottom() - noise01 (s) * j);
        for (float y = r.getBottom() - step; y > r.getY(); y -= step) shape.lineTo (r.getX() + noise01 (s) * j, y);
        shape.closeSubPath();
    }
    else
    {
        shape.addRectangle (r);
    }

    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (shape, juce::AffineTransform::translation (1.5f, 2.0f));

    const auto& tile = paperTile();
    const int ox = (int) (mix (seed) % 200), oy = (int) (mix (seed * 3u + 1u) % 200);
    g.setFillType (juce::FillType (tile, juce::AffineTransform::translation ((float) -ox, (float) -oy)));
    g.fillPath (shape);

    if (tint != Colours::paper)
    {
        g.setColour (tint.withAlpha (0.55f));
        g.fillPath (shape);
    }

    // gentle darkening towards the edges
    juce::ColourGradient vign (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                               juce::Colours::black.withAlpha (0.12f), r.getX(), r.getY(), true);
    g.setGradientFill (vign);
    g.fillPath (shape);
}

void Theme::drawTape (juce::Graphics& g, juce::Rectangle<float> r, float angle, uint32_t seed, juce::Colour tint)
{
    for (int i = 1; i <= 3; ++i)
    {
        if (auto img = getAsset ("tape_" + juce::String (i) + ".png"); img.isValid() && (int) (seed % 3) + 1 == i)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::rotation (angle, r.getCentreX(), r.getCentreY()));
            g.drawImage (img, r, juce::RectanglePlacement::stretchToFit);
            return;
        }
    }

    juce::Path p;
    uint32_t s = seed;
    const float step = 3.0f;
    p.startNewSubPath (r.getX(), r.getY());
    p.lineTo (r.getRight(), r.getY());
    for (float y = r.getY() + step; y < r.getBottom(); y += step) p.lineTo (r.getRight() - noise01 (s) * 2.0f, y);
    p.lineTo (r.getRight(), r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    for (float y = r.getBottom() - step; y > r.getY(); y -= step) p.lineTo (r.getX() + noise01 (s) * 2.0f, y);
    p.closeSubPath();
    p.applyTransform (juce::AffineTransform::rotation (angle, r.getCentreX(), r.getCentreY()));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (p, juce::AffineTransform::translation (1.0f, 1.5f));
    g.setFillType (juce::FillType (paperTile(), juce::AffineTransform::translation ((float) -(int) (seed % 97), 0.0f)));
    g.fillPath (p);
    g.setColour (tint.withAlpha (tint.getSaturation() > 0.5f ? 0.92f : 0.6f));
    g.fillPath (p);
}

void Theme::drawPin (juce::Graphics& g, juce::Point<float> c, float radius)
{
    if (auto img = getAsset ("pin.png"); img.isValid())
    {
        g.drawImage (img, juce::Rectangle<float> (radius * 2.6f, radius * 2.6f).withCentre (c), juce::RectanglePlacement::centred);
        return;
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c.translated (1.5f, 2.0f)));
    juce::ColourGradient grad (Colours::red.brighter (0.5f), c.x - radius * 0.4f, c.y - radius * 0.5f,
                               Colours::redDark.darker (0.4f), c.x + radius, c.y + radius, true);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.55f, radius * 0.45f).withCentre (c.translated (-radius * 0.35f, -radius * 0.4f)));
}

void Theme::drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    const bool hiDpi = g.getInternalContext().getPhysicalPixelScaleFactor() > 1.01f;
    auto img = getAsset (hiDpi ? "logo@2x.png" : "logo.png");
    if (! img.isValid())
        img = getAsset ("logo.png");
    if (img.isValid())
    {
        g.drawImage (img, area, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        return;
    }

    // placeholder logo (code-drawn) until Resources/Logo/logo.png exists
    auto f = marker (area.getHeight() * 0.92f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, "VOODOO", area.getX(), area.getBottom() - area.getHeight() * 0.18f);
    const float w1 = ga.getBoundingBox (0, -1, true).getWidth();
    juce::Path p1;
    ga.createPath (p1);
    g.setColour (juce::Colours::black);
    g.fillPath (p1, juce::AffineTransform::translation (2.0f, 3.0f));
    g.setColour (Colours::paper);
    g.fillPath (p1);

    juce::GlyphArrangement gb;
    gb.addLineOfText (f, "KILLA", area.getX() + w1 + area.getHeight() * 0.12f, area.getBottom() - area.getHeight() * 0.18f);
    juce::Path p2;
    gb.createPath (p2);
    g.setColour (juce::Colours::black);
    g.fillPath (p2, juce::AffineTransform::translation (2.0f, 3.0f));
    g.setColour (Colours::red);
    g.fillPath (p2);
}

juce::Image Theme::photo (const juce::String& fileName, bool& isPlaceholder)
{
    auto img = getAsset (fileName);
    isPlaceholder = ! img.isValid();
    if (isPlaceholder)
        img = getAsset ("placeholder.jpg");
    return img;
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
    g.fillRoundedRectangle (r, r.getWidth() * 0.2f);
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
    setColour (juce::PopupMenu::backgroundColourId, Colours::ink);
    setColour (juce::PopupMenu::textColourId, Colours::paper);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::red);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, Colours::grey);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::paper);
    setColour (juce::TooltipWindow::textColourId, Colours::ink);
    setColour (juce::AlertWindow::backgroundColourId, Colours::ink);
    setColour (juce::AlertWindow::textColourId, Colours::paper);
    setColour (juce::AlertWindow::outlineColourId, Colours::red);
    setColour (juce::TextEditor::backgroundColourId, Colours::paper);
    setColour (juce::TextEditor::textColourId, Colours::ink);
    setColour (juce::TextEditor::highlightColourId, Colours::red.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, Colours::greyDark);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::red);
    setColour (juce::TextButton::buttonColourId, Colours::ink);
    setColour (juce::TextButton::textColourOffId, Colours::paper);
    setColour (juce::TextButton::buttonOnColourId, Colours::red);
    setColour (juce::ComboBox::outlineColourId, Colours::red);
}

juce::Rectangle<int> KillaLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = Theme::typewriter (13.0f);
    const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (f, tipText) + 18);
    const int h = 24;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 16, w, h)
               .constrainedWithin (parentArea);
}

void KillaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.fillAll (Colours::paper);
    g.setColour (Colours::ink);
    g.drawRect (0, 0, width, height, 1);
    g.setFont (Theme::typewriter (13.0f));
    g.drawFittedText (text, 8, 0, width - 16, height, juce::Justification::centredLeft, 1);
}

juce::Font KillaLookAndFeel::getPopupMenuFont()                  { return Theme::typewriter (15.0f); }
juce::Font KillaLookAndFeel::getAlertWindowTitleFont()           { return Theme::marker (22.0f); }
juce::Font KillaLookAndFeel::getAlertWindowMessageFont()         { return Theme::typewriter (15.0f); }
juce::Font KillaLookAndFeel::getAlertWindowFont()                { return Theme::typewriter (14.0f); }
juce::Font KillaLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return Theme::typewriter ((float) h * 0.55f); }

void KillaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Colours::ink);
    g.setColour (Colours::red.withAlpha (0.6f));
    g.drawRect (0, 0, width, height, 1);
}
} // namespace vk::ui
