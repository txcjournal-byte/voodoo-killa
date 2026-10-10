#include "Waveform.h"

namespace vk::ui
{
Waveform::Waveform (vk::EngineMeters& m) : meters (m)
{
    setTooltip ("One bar of audio: dim = dry, blue = effect. The bright line is the playhead.");
    setOpaque (false);
}

void Waveform::update()
{
    bool changed = false;
    for (int i = 0; i < vk::EngineMeters::kColumns; ++i)
    {
        const float d = meters.dry[i].load (std::memory_order_relaxed);
        const float w = meters.wet[i].load (std::memory_order_relaxed);
        changed = changed || d != dry[(size_t) i] || w != wet[(size_t) i];
        dry[(size_t) i] = d;
        wet[(size_t) i] = w;
    }
    const float ph = meters.playhead.load (std::memory_order_relaxed);
    changed = changed || std::abs (ph - playhead) > 1.0e-4f;
    playhead = ph;
    if (changed)
        repaint();
}

void Waveform::setBpm (double newBpm)
{
    const int b = juce::roundToInt (newBpm);
    if (b != bpm) { bpm = b; repaint(); }
}

void Waveform::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto a = b.reduced (12.0f, 0.0f).withTrimmedTop (24.0f).withTrimmedBottom (8.0f);
    const float mid = a.getCentreY();

    // header + beat grid rendered once per size / scale (the waveform itself redraws at 30 fps)
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int iw = juce::roundToInt (b.getWidth() * scale), ih = juce::roundToInt (b.getHeight() * scale);
    if (background.getWidth() != iw || background.getHeight() != ih || cachedBpm != bpm)
    {
        cachedBpm = bpm;
        background = juce::Image (juce::Image::ARGB, juce::jmax (1, iw), juce::jmax (1, ih), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (scale));
        bg.setColour (Colours::accent);
        bg.setFont (Theme::mono (8.0f, 0.2f));
        bg.drawText ("SIGNAL", juce::Rectangle<float> (12.0f, 6.0f, 100.0f, 16.0f), juce::Justification::centredLeft, false);
        bg.setColour (Colours::muted);
        bg.setFont (Theme::monoLight (7.0f, 0.2f));
        const juce::String info = juce::String (juce::CharPointer_UTF8 ("1 \xc2\xb7 2 \xc2\xb7 3 \xc2\xb7 4   |   "))
                                + (bpm > 0 ? juce::String (bpm) + " BPM" : juce::String ("-- BPM"));
        bg.drawText (info, juce::Rectangle<float> (b.getWidth() - 232.0f, 6.0f, 220.0f, 16.0f), juce::Justification::centredRight, false);
        for (int i = 1; i < 4; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / 4.0f;
            bg.setColour (Colours::accent.withAlpha (0.10f));
            bg.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        }
    }
    g.drawImage (background, b);

    const int n = vk::EngineMeters::kColumns;
    const float colW = a.getWidth() / (float) n;
    auto shape = [&] (const std::array<float, vk::EngineMeters::kColumns>& v, int from, int to)
    {
        juce::Path p;
        for (int i = from; i < to; ++i)
        {
            const float h = juce::jmin (1.0f, std::sqrt (v[(size_t) i])) * a.getHeight() * 0.5f;
            const float x = a.getX() + colW * (float) i;
            p.addRectangle (x, mid - h, juce::jmax (1.0f, colW * 0.6f), h * 2.0f + 0.5f);
        }
        return p;
    };

    // played part bright, the rest of the bar dimmed
    const int split = juce::jlimit (0, n, (int) std::ceil (playhead * (float) n));
    g.setColour (Colours::muted.withAlpha (0.45f));
    g.fillPath (shape (dry, 0, n));
    g.setColour (Colours::accent.withAlpha (0.35f));
    g.fillPath (shape (wet, split, n));
    g.setColour (Colours::accentL);
    g.fillPath (shape (wet, 0, split));

    // playhead
    const float x = a.getX() + a.getWidth() * playhead;
    const auto ph = juce::Rectangle<float> (2.0f, a.getHeight() + 6.0f).withCentre ({ x, mid });
    g.setColour (Colours::accent.withAlpha (0.35f));
    g.fillRect (ph.expanded (2.0f, 0.0f));
    g.setColour (Colours::text);
    g.fillRect (ph);
}

// ===========================================================================
Thread::Thread()
{
    setInterceptsMouseClicks (false, false);
}

void Thread::setPoints (std::optional<juce::Point<float>> a, std::optional<juce::Point<float>> b,
                        juce::Point<float> d, float railY)
{
    if (a == pinA && b == pinB && d == dot && railY == rail)
        return;
    pinA = a;
    pinB = b;
    dot = d;
    rail = railY;

    const auto nb = buildPath().getBounds().expanded (6.0f).getSmallestIntegerContainer();
    repaint (lastBounds.getUnion (nb));
    lastBounds = nb;
}

juce::Path Thread::buildPath() const
{
    juce::Path p;
    if (! pinA.has_value())
        return p;

    auto start = *pinA;
    p.startNewSubPath (start);

    // A -> B : gentle upward arch with a little sag in the middle
    if (pinB.has_value())
    {
        const auto end = *pinB;
        const float dx = end.x - start.x;
        const float lift = juce::jlimit (6.0f, 26.0f, std::abs (dx) * 0.08f) + 4.0f;
        const float top = juce::jmin (start.y, end.y) - lift;
        p.cubicTo (start.x + dx * 0.25f, top, end.x - dx * 0.25f, top + 2.0f, end.x, end.y);
        start = end;
    }

    // last pin -> dot : rises to the rail above the cards, runs right, then drops into the pad
    const float dx = dot.x - start.x;
    const float railY = juce::jmin (rail, start.y - 4.0f);
    p.cubicTo (start.x + dx * 0.30f, railY - 2.0f,
               dot.x - juce::jmax (40.0f, dx * 0.35f), railY + (dot.y - railY) * 0.15f,
               dot.x, dot.y);
    return p;
}

void Thread::paint (juce::Graphics& g)
{
    const auto p = buildPath();
    if (p.isEmpty())
        return;
    const juce::PathStrokeType wide (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (Colours::glow.withAlpha (0.22f));
    g.strokePath (p, wide);
    g.setColour (Colours::accentL.withAlpha (0.9f));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace vk::ui
