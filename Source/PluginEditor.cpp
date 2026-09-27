#include "PluginEditor.h"

VoodooKillaAudioProcessorEditor::VoodooKillaAudioProcessorEditor (VoodooKillaAudioProcessor& p)
    : AudioProcessorEditor (p)
{
    setSize (1000, 625);
}

void VoodooKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0b));
    g.setColour (juce::Colour (0xffff2e3e));
    g.setFont (40.0f);
    g.drawText ("VOODOO KILLA", getLocalBounds(), juce::Justification::centred);
}
