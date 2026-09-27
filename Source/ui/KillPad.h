#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

class VoodooKillaAudioProcessor;

namespace vk::ui
{
/**
    KILL PAD (XY). With A+B: X = MORPH, Y = TONE. With only A: X = AMOUNT, Y = TONE.
    Bidirectionally bound to the parameters; double click returns to the centre.
*/
class KillPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KillPad (VoodooKillaAudioProcessor&);
    ~KillPad() override;

    void refresh();                               // selection changed (axis mode)
    juce::Point<float> getDotPosition() const;    // local coordinates
    std::function<void()> onDotMoved;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    juce::RangedAudioParameter& xParam() const;
    void setFromPosition (juce::Point<float>);
    void paramChanged();

    VoodooKillaAudioProcessor& proc;
    juce::RangedAudioParameter& morph;
    juce::RangedAudioParameter& amount;
    juce::RangedAudioParameter& tone;
    std::unique_ptr<juce::ParameterAttachment> morphAtt, amountAtt, toneAtt;
    bool morphMode = false;
    bool dragging = false;
    juce::Image gridCache;
};
} // namespace vk::ui
