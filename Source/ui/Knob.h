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

/** Black bakelite knob with a red line. Drag up/down, Ctrl/Cmd+drag = fine, double click = default. */
class Knob : public ParamControl
{
public:
    Knob (juce::RangedAudioParameter& p, juce::String label, std::function<void()> gestureEnded,
          std::function<juce::String (float plain)> formatter);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static void drawKnobBody (juce::Graphics&, juce::Rectangle<float> area, float normalised);

private:
    juce::String label;
    std::function<juce::String (float)> format;
    float dragStartValue = 0.0f;
    float lastDragY = 0.0f;
    float accum = 0.0f;
};

/** Red rocker switch (808 DUCK). */
class RockerSwitch : public ParamControl
{
public:
    RockerSwitch (juce::RangedAudioParameter& p, std::function<void()> gestureEnded);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
};
} // namespace vk::ui
