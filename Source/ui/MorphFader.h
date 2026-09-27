#pragma once

#include "Knob.h"

namespace vk::ui
{
/** MORPH fader on a strip of tape: "A" left, "B" right. Greyed out while no B card is selected. */
class MorphFader : public ParamControl
{
public:
    MorphFader (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);

    void setMorphEnabled (bool enabled);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> trackArea() const;
    float valueAt (float x) const;
    bool enabled = false;
    float grabOffset = 0.0f;
};

/** STEPS: 16 toggles (1 bar). */
class StepsBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepsBar (juce::AudioProcessorValueTreeState& state, std::function<void()> gestureEnded);
    ~StepsBar() override;

    void setPlayingStep (int step, bool show);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int stepAt (juce::Point<float>) const;
    juce::Rectangle<float> cellBounds (int i) const;
    bool isOn (int i) const;
    void setStep (int i, bool on);

    std::vector<std::unique_ptr<juce::ParameterAttachment>> attachments;
    std::vector<juce::RangedAudioParameter*> params;
    std::function<void()> onGestureEnd;
    int playingStep = -1;
    bool showPlaying = false;
    bool paintValue = true;
    int lastPainted = -1;
};

/** TRIGGER: six framed buttons, the active one red. */
class TriggerBar : public ParamControl
{
public:
    TriggerBar (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    juce::Rectangle<float> buttonBounds (int i) const;
    int buttonAt (juce::Point<float>) const;
};
} // namespace vk::ui
