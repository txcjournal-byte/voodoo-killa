#include "Waveform.h"

namespace vk::ui
{
Waveform::Waveform (vk::EngineMeters& m) : meters (m)
{
    setTooltip ("One bar of audio: grey = dry, red = effect. The red line is the playhead.");
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

void Waveform::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Theme::drawPaper (g, b, 9001u, true);

    const auto a = b.reduced (14.0f, 8.0f);
    const float mid = a.getCentreY();

    // beat grid
    for (int i = 1; i < 16; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / 16.0f;
        g.setColour (Colours::ink.withAlpha (i % 4 == 0 ? 0.18f : 0.07f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    }

    const int n = vk::EngineMeters::kColumns;
    const float colW = a.getWidth() / (float) n;
    auto shape = [&] (const std::array<float, vk::EngineMeters::kColumns>& v)
    {
        juce::Path p;
        for (int i = 0; i < n; ++i)
        {
            const float h = juce::jmin (1.0f, std::sqrt (v[(size_t) i])) * a.getHeight() * 0.5f;
            const float x = a.getX() + colW * (float) i;
            p.addRectangle (x, mid - h, juce::jmax (1.0f, colW * 0.85f), h * 2.0f + 0.5f);
        }
        return p;
    };

    g.setColour (Colours::ink.withAlpha (0.75f));
    g.fillPath (shape (dry));
    g.setColour (Colours::red.withAlpha (0.85f));
    g.fillPath (shape (wet));

    // playhead
    const float x = a.getX() + a.getWidth() * playhead;
    g.setColour (Colours::red);
    g.fillRect (juce::Rectangle<float> (3.0f, b.getHeight() - 4.0f).withCentre ({ x, b.getCentreY() }));
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
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (1.2f, 2.0f));
    g.setColour (Colours::red.darker (0.1f));
    g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::red.brighter (0.4f).withAlpha (0.5f));
    g.strokePath (p, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (-0.3f, -0.4f));
}
} // namespace vk::ui
