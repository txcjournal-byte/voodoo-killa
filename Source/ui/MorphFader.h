#pragma once

#include "Knob.h"

namespace vk::ui
{
/** MORPH slider: chrome "A" / "B", glowing fill, chrome thumb. Works even without B (asks for one). */
class MorphFader : public ParamControl
{
public:
    MorphFader (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);

    void setMorphEnabled (bool enabled);
    void setNames (const juce::String& a, const juce::String& b);
    /** Called when the fader is grabbed without a B card; returns true when a B is now available. */
    std::function<bool()> onNeedB;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> trackArea() const;
    float valueAt (float x) const;
    bool enabled = false;
    juce::String nameA, nameB;
};

/** 16-step pattern editor (used by WHEN = Pattern, shown in a call-out). */
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

/** WHEN: dropdown pill for the trigger (Always / Hold / End of 4 / End of 8 / Last beat / Pattern). */
class WhenSelector : public ParamControl
{
public:
    WhenSelector (juce::RangedAudioParameter& p, juce::AudioProcessorValueTreeState& state, std::function<void()> gestureEnded);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    static juce::String labelFor (int index);

private:
    juce::Rectangle<float> pillArea() const;
    void showPatternEditor();
    juce::AudioProcessorValueTreeState& state;
    std::function<void()> gestureEnd;
};
} // namespace vk::ui
