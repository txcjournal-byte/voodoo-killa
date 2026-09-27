#pragma once
#include "PluginProcessor.h"

class VoodooKillaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoodooKillaAudioProcessorEditor (VoodooKillaAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoodooKillaAudioProcessorEditor)
};
