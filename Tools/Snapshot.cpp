// Renders the Voodoo Killa editor to a PNG (design check without a DAW).
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;

    VoodooKillaAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 256);
    proc.prepareToPlay (44100.0, 256);

    // state similar to the reference: A = Halfcut, B = Slow Grave, morph towards B
    proc.loadPreset (0, VoodooKillaAudioProcessor::Slot::A);
    proc.loadPreset (1, VoodooKillaAudioProcessor::Slot::B);
    proc.apvts.getParameter (ParamIDs::morph)->setValueNotifyingHost (0.85f);
    proc.apvts.getParameter (ParamIDs::amount)->setValueNotifyingHost (0.65f);
    proc.apvts.getParameter (ParamIDs::speed)->setValueNotifyingHost (0.25f);
    proc.apvts.getParameter (ParamIDs::space)->setValueNotifyingHost (0.45f);
    proc.apvts.getParameter (ParamIDs::mix)->setValueNotifyingHost (0.8f);
    proc.apvts.getParameter (ParamIDs::tone)->setValueNotifyingHost (0.5f);

    // feed ~1.7 bars of a test melody so the waveform has content
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    juce::Random r (3);
    for (int b = 0; b < 300; ++b)
    {
        for (int i = 0; i < 256; ++i)
        {
            const double t = (b * 256 + i) / 44100.0;
            const double env = std::exp (-std::fmod (t, 0.25) * 9.0);
            const float x = (float) (0.5 * env * std::sin (2.0 * juce::MathConstants<double>::pi * (220.0 + 110.0 * std::floor (std::fmod (t * 4.0, 3.0))) * t)
                                     + 0.02 * r.nextFloat());
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        proc.processBlock (buf, midi);
    }

    proc.setUiScale (scale);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditorAndMakeActive());
    for (int i = 0; i < 5; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    {
        // full-window repaint cost (worst case; normal frames only repaint changed regions)
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 30; ++i)
            ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const auto full = (juce::Time::getMillisecondCounterHiRes() - t0) / 30.0;

        // typical animation frame: waveform + pad + thread region
        auto* view = ed->getChildComponent (0);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 30; ++i)
            view->createComponentSnapshot ({ 700, 110, 300, 290 }, true, 1.0f);
        const auto part = (juce::Time::getMillisecondCounterHiRes() - t1) / 30.0;
        std::printf ("full repaint %.2f ms, KILL PAD region %.2f ms\n", full, part);
    }
    juce::FileOutputStream os (out);
    os.setPosition (0);
    os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
