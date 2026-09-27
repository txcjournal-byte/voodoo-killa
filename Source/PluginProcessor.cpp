#include "PluginProcessor.h"
#include "PluginEditor.h"

VoodooKillaAudioProcessor::VoodooKillaAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void VoodooKillaAudioProcessor::prepareToPlay (double, int) {}

bool VoodooKillaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void VoodooKillaAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) {}

juce::AudioProcessorEditor* VoodooKillaAudioProcessor::createEditor()
{
    return new VoodooKillaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoodooKillaAudioProcessor();
}
