#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace vk::ui
{
/** Base for controls bound to one APVTS parameter (gestures, host automation, undo hook). */
class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamControl (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);

    float getNormalised() const noexcept { return value; }
    float getPlain() const { return param.convertFrom0to1 (value); }
    juce::RangedAudioParameter& getParameter() noexcept { return param; }

protected:
    void beginEdit();
    void setNormalised (float v);           // inside a gesture
    void setNormalisedComplete (float v);   // single-shot change
    void endEdit();
    virtual void valueChanged() { repaint(); }

    juce::RangedAudioParameter& param;

private:
    juce::ParameterAttachment attachment;
    std::function<void()> onGestureEnd;
    float value = 0.0f;
    bool inGesture = false;
};

/** Chrome knob with an ice-blue value ring. bipolar = ring grows from 12 o'clock. */
void drawChromeKnob (juce::Graphics&, juce::Rectangle<float> area, float normalised, bool bipolar);

/** Macro knob: drag up/down, Ctrl/Cmd+drag = fine, double click = default, wheel. Value above the label. */
class Knob : public ParamControl
{
public:
    Knob (juce::RangedAudioParameter& p, juce::String label, std::function<void()> gestureEnded,
          std::function<juce::String (float plain)> formatter, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::String label;
    std::function<juce::String (float)> format;
    bool bipolar = false;
    float lastDragY = 0.0f, accum = 0.0f;
};

/**
    Small knob not bound to a host parameter (Halftime Engine settings that live in the preset).
    Works on a list of discrete steps or continuously (0..1).
*/
class MiniKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    MiniKnob (juce::String label, int numSteps);   // numSteps <= 1 -> continuous

    std::function<float()> getValue;                     // 0..1
    std::function<void (float, bool finished)> setValue; // 0..1
    std::function<juce::String()> getText;
    float defaultValue = 0.0f;

    void setEnabledLook (bool e) { if (e != enabledLook) { enabledLook = e; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    float quantise (float v) const;
    juce::String label;
    int steps = 0;
    float accum = 0.0f, lastY = 0.0f;
    bool enabledLook = true;
};

/** Pill toggle switch (DUCK FOR 808). */
class ToggleSwitch : public ParamControl
{
public:
    ToggleSwitch (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
};
} // namespace vk::ui
