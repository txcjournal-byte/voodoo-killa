#pragma once

#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Knob.h"
#include "ui/MorphFader.h"
#include "ui/CategoryList.h"
#include "ui/PolaroidGrid.h"
#include "ui/KillPad.h"
#include "ui/Waveform.h"
#include "ui/TopBar.h"

/** The whole 1000 x 625 interface; the editor scales it (100 / 125 / 150 %). */
class MainView : public juce::Component
{
public:
    explicit MainView (VoodooKillaAudioProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();   // ~30 fps from the editor

    std::function<void (float)> onScaleChosen;

private:
    void renderStatic (juce::Graphics&);
    void updateThread();
    void showSettings (juce::Component& target);

    VoodooKillaAudioProcessor& proc;

    vk::ui::TopBar topBar;
    vk::ui::KillButton killButton;
    vk::ui::IconButton lockButton, gearButton;
    vk::ui::CategoryList categories;
    vk::ui::Waveform waveform;
    vk::ui::PolaroidGrid grid;
    vk::ui::KillPad pad;
    vk::ui::StepsBar steps;
    vk::ui::TriggerBar triggerBar;
    std::vector<std::unique_ptr<vk::ui::Knob>> knobs;
    vk::ui::MorphFader morphFader;
    vk::ui::RockerSwitch duckSwitch;
    vk::ui::Thread thread;

    juce::Image staticCache;
    float staticScale = 0.0f;
    uint32_t lastSelection = 0;
    float pulsePhase = 0.0f;
    bool lastUndo = false, lastRedo = false;
};

class VoodooKillaAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoodooKillaAudioProcessorEditor (VoodooKillaAudioProcessor&);
    ~VoodooKillaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    void timerCallback() override;
    void applyScale (float s);
    bool sizeInitialised = false;

    VoodooKillaAudioProcessor& proc;
    // declared first: fonts/images outlive every component of this window and die with it
    juce::SharedResourcePointer<vk::ui::ThemeResources> themeResources;
    vk::ui::KillaLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoodooKillaAudioProcessorEditor)
};
