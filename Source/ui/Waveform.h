#pragma once

#include "Theme.h"
#include "../engine/Engine.h"

namespace vk::ui
{
/** One bar of audio: dry dimmed, wet in blue, bright playhead, beat grid. */
class Waveform : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Waveform (vk::EngineMeters&);
    void update();   // called from the editor timer
    void setBpm (double bpm);
    void paint (juce::Graphics&) override;

private:
    vk::EngineMeters& meters;
    std::array<float, vk::EngineMeters::kColumns> dry {}, wet {};
    float playhead = 0.0f;
    juce::Image background;
    int bpm = 0, cachedBpm = -1;
};

/** The glowing thread: pin A -> pin B -> KILL PAD dot. Drawn by code as cubic Béziers arching above the cards. */
class Thread : public juce::Component
{
public:
    Thread();
    /** Points in this component's coordinates. railY = height of the arch (above the card photos). */
    void setPoints (std::optional<juce::Point<float>> a, std::optional<juce::Point<float>> b,
                    juce::Point<float> dot, float railY);
    void paint (juce::Graphics&) override;
    bool hitTest (int, int) override { return false; }

private:
    juce::Path buildPath() const;
    std::optional<juce::Point<float>> pinA, pinB;
    juce::Point<float> dot;
    float rail = 0.0f;
    juce::Rectangle<int> lastBounds;
};
} // namespace vk::ui
