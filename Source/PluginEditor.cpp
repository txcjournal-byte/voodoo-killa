#include "PluginEditor.h"

using namespace vk::ui;

namespace
{
// Layout on the 1000 x 625 base canvas (8 px grid, measured from Design/ui-reference.png)
const juce::Rectangle<int> kLogo        { 36, 6, 462, 92 };
const juce::Rectangle<int> kSubtitle    { 47, 99, 448, 20 };
const juce::Rectangle<int> kTopBar      { 504, 36, 336, 48 };
const juce::Rectangle<int> kKill        { 848, 36, 104, 52 };
const juce::Rectangle<int> kLock        { 960, 50, 24, 26 };
const juce::Rectangle<int> kGear        { 972, 10, 18, 18 };
const juce::Rectangle<int> kCategories  { 16, 128, 128, 376 };
const juce::Rectangle<int> kWaveform    { 160, 122, 536, 50 };
const juce::Rectangle<int> kCardGrid        { 156, 178, 544, 312 };
const juce::Rectangle<int> kPadTab      { 724, 118, 104, 24 };
const juce::Rectangle<int> kPad         { 712, 140, 272, 240 };
const juce::Rectangle<int> kStepsTab    { 712, 390, 64, 20 };
const juce::Rectangle<int> kSteps       { 712, 414, 272, 42 };
const juce::Rectangle<int> kTriggerTab  { 712, 460, 72, 20 };
const juce::Rectangle<int> kTrigger     { 712, 484, 272, 28 };
const juce::Rectangle<int> kKnobPanel   { 136, 512, 440, 100 };
const juce::Rectangle<int> kMorphPanel  { 588, 528, 240, 72 };
const juce::Rectangle<int> kMorphTab    { 660, 516, 96, 22 };
const juce::Rectangle<int> kDuckTab     { 856, 522, 96, 22 };
const juce::Rectangle<int> kDuck        { 864, 548, 84, 52 };

const char* const kKnobIds[]    = { ParamIDs::amount, ParamIDs::speed, ParamIDs::tone, ParamIDs::space, ParamIDs::mix, ParamIDs::out };
const char* const kKnobLabels[] = { "AMOUNT", "SPEED", "TONE", "SPACE", "MIX", "OUT" };
const char* const kKnobTips[]   = {
    "Amount: strength of the effect.",
    "Speed: lengths and rates one step faster or slower.",
    "Tone: darker (left) or brighter (right).",
    "Space: reverb / delay amount (50 % = as designed).",
    "Mix: dry / wet balance.",
    "Out: output level."
};

void drawTabLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, uint32_t seed, float angle = -0.01f)
{
    Theme::drawTape (g, r.toFloat(), angle, seed);
    g.setColour (Colours::ink);
    g.setFont (Theme::typewriter (14.5f));
    g.drawFittedText (text, r.reduced (4, 0), juce::Justification::centred, 1, 0.75f);
}
}

// ===========================================================================
MainView::MainView (VoodooKillaAudioProcessor& p)
    : proc (p),
      topBar (p),
      killButton (p),
      lockButton (IconButton::Kind::lock, [&p] { return p.getCategoryLock(); },
                  [&p] (IconButton&) { p.setCategoryLock (! p.getCategoryLock()); }),
      gearButton (IconButton::Kind::gear, nullptr, [this] (IconButton& b) { showSettings (b); }),
      categories (p),
      waveform (p.getMeters()),
      grid (p),
      pad (p),
      steps (p.apvts, [&p] { p.pushUndoSnapshot(); }),
      triggerBar (*p.apvts.getParameter (ParamIDs::trigger), [&p] { p.pushUndoSnapshot(); }),
      morphFader (*p.apvts.getParameter (ParamIDs::morph), [&p] { p.pushUndoSnapshot(); }),
      duckSwitch (*p.apvts.getParameter (ParamIDs::duck), [&p] { p.pushUndoSnapshot(); })
{
    lockButton.setTooltip ("Lock: KILL picks only from the current category.");
    gearButton.setTooltip ("Settings: interface size.");
    duckSwitch.setTooltip ("808 Duck: ducks the melody on 808 hits (sidechain input or MIDI notes below C2).");

    auto formatter = [] (int i) -> std::function<juce::String (float)>
    {
        switch (i)
        {
            case 0: case 3: case 4: return [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
            case 1: return [] (float v) { return VoodooKillaAudioProcessor::speedChoices()[juce::roundToInt (v)]; };
            case 2: return [] (float v) { const int t = juce::roundToInt (v); return (t > 0 ? "+" : "") + juce::String (t); };
            default: return [] (float v) { return juce::String (v, 1) + " dB"; };
        }
    };

    for (int i = 0; i < 6; ++i)
    {
        knobs.push_back (std::make_unique<Knob> (*p.apvts.getParameter (kKnobIds[i]), kKnobLabels[i],
                                                 [&p] { p.pushUndoSnapshot(); }, formatter (i)));
        knobs.back()->setTooltip (kKnobTips[i]);
        addAndMakeVisible (*knobs.back());
    }

    for (auto* c : std::initializer_list<juce::Component*> { &topBar, &killButton, &lockButton, &gearButton, &categories,
                                                             &waveform, &grid, &pad, &steps, &triggerBar, &morphFader, &duckSwitch })
        addAndMakeVisible (c);

    addAndMakeVisible (thread);
    thread.setAlwaysOnTop (true);

    pad.onDotMoved = [this] { updateThread(); };
    setSize (kBaseWidth, kBaseHeight);
}

MainView::~MainView() = default;

void MainView::resized()
{
    topBar.setBounds (kTopBar);
    killButton.setBounds (kKill);
    lockButton.setBounds (kLock);
    gearButton.setBounds (kGear);
    categories.setBounds (kCategories);
    waveform.setBounds (kWaveform);
    grid.setBounds (kCardGrid);
    pad.setBounds (kPad);
    steps.setBounds (kSteps);
    triggerBar.setBounds (kTrigger);

    const auto knobArea = kKnobPanel.reduced (10, 6);
    const int kw = knobArea.getWidth() / 6;
    for (int i = 0; i < (int) knobs.size(); ++i)
        knobs[(size_t) i]->setBounds (knobArea.getX() + i * kw, knobArea.getY(), kw, knobArea.getHeight());

    morphFader.setBounds (kMorphPanel.reduced (4, 6).withTrimmedTop (8));
    duckSwitch.setBounds (kDuck);
    thread.setBounds (getLocalBounds());
    staticScale = 0.0f;
    updateThread();
}

void MainView::renderStatic (juce::Graphics& g)
{
    Theme::fillBackground (g, getLocalBounds().toFloat());

    Theme::drawLogo (g, kLogo.toFloat());

    // subtitle strip
    Theme::drawPaper (g, kSubtitle.toFloat(), 55u, true);
    g.setColour (Colours::ink);
    g.setFont (Theme::typewriter (12.5f));
    g.drawFittedText (juce::CharPointer_UTF8 ("MELODY FX \xc2\xb7 HALFTIME \xc2\xb7 TAPE STOP \xc2\xb7 STUTTER \xc2\xb7 REVERSE \xc2\xb7 PITCH \xc2\xb7 LO-FI"),
                kSubtitle.reduced (6, 0), juce::Justification::centred, 1, 0.7f);

    // knob panel
    Theme::drawPaper (g, kKnobPanel.toFloat(), 777u, true);

    // morph tape + tab
    Theme::drawTape (g, kMorphPanel.toFloat(), 0.004f, 3u);
    drawTabLabel (g, kMorphTab, "MORPH", 11u);

    // 808 duck tab
    drawTabLabel (g, kDuckTab, "808 DUCK", 21u, 0.012f);

    // right column tabs
    drawTabLabel (g, kPadTab, "KILL PAD", 5u, -0.02f);
    drawTabLabel (g, kStepsTab, "STEPS", 8u);
    drawTabLabel (g, kTriggerTab, "TRIGGER", 9u);
}

void MainView::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! staticCache.isValid() || std::abs (scale - staticScale) > 1.0e-3f)
    {
        staticScale = scale;
        staticCache = juce::Image (juce::Image::RGB, juce::roundToInt ((float) getWidth() * scale),
                                   juce::roundToInt ((float) getHeight() * scale), true);
        juce::Graphics sg (staticCache);
        sg.addTransform (juce::AffineTransform::scale (scale));
        renderStatic (sg);
    }
    g.drawImage (staticCache, getLocalBounds().toFloat());
}

void MainView::updateThread()
{
    const auto a = grid.getPin (1, *this);
    const auto b = grid.getPin (2, *this);
    const auto dot = getLocalPoint (&pad, pad.getDotPosition());
    const float railY = (float) kCardGrid.getY() + 2.0f;
    thread.setPoints (a, b, dot, railY);
}

void MainView::tick()
{
    const auto sel = proc.getSelectionVersion();
    if (sel != lastSelection)
    {
        lastSelection = sel;
        categories.refresh();
        grid.refresh();
        topBar.refresh();
        pad.refresh();
        morphFader.setMorphEnabled (proc.getSlotB().valid);
        lockButton.repaint();
        updateThread();
    }

    if (proc.canUndo() != lastUndo || proc.canRedo() != lastRedo)
    {
        lastUndo = proc.canUndo();
        lastRedo = proc.canRedo();
        topBar.repaint();
    }

    waveform.update();

    const bool active = proc.isEffectActive();
    pulsePhase += 0.35f;
    if (pulsePhase > juce::MathConstants<float>::twoPi) pulsePhase -= juce::MathConstants<float>::twoPi;
    grid.updatePulse (active, pulsePhase);

    auto& m = proc.getMeters();
    const bool stepsMode = (int) proc.apvts.getRawParameterValue (ParamIDs::trigger)->load() == (int) vk::TriggerMode::steps;
    steps.setPlayingStep (m.currentStep.load (std::memory_order_relaxed), stepsMode);
}

void MainView::showSettings (juce::Component& target)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Interface size");
    const float current = proc.getUiScale();
    menu.addItem (1, "100 %", true, std::abs (current - 1.0f) < 0.01f);
    menu.addItem (2, "125 %", true, std::abs (current - 1.25f) < 0.01f);
    menu.addItem (3, "150 %", true, std::abs (current - 1.5f) < 0.01f);
    menu.addSeparator();
    menu.addItem (10, "Open user preset folder");
    menu.addSectionHeader (juce::String ("Voodoo Killa ") + JucePlugin_VersionString);

    juce::Component::SafePointer<MainView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        if (r == 10)
        {
            auto folder = vk::PresetLibrary::getUserPresetFolder();
            folder.createDirectory();
            folder.startAsProcess();
            return;
        }
        const float s = r == 2 ? 1.25f : r == 3 ? 1.5f : 1.0f;
        if (safe->onScaleChosen) safe->onScaleChosen (s);
    });
}

// ===========================================================================
VoodooKillaAudioProcessorEditor::VoodooKillaAudioProcessorEditor (VoodooKillaAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    view.onScaleChosen = [this] (float s)
    {
        proc.setUiScale (s);
        applyScale (s);
    };
    applyScale (proc.getUiScale());
    startTimerHz (30);
}

VoodooKillaAudioProcessorEditor::~VoodooKillaAudioProcessorEditor()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VoodooKillaAudioProcessorEditor::applyScale (float s)
{
    s = juce::jlimit (1.0f, 1.5f, s);
    view.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (kBaseWidth * s), juce::roundToInt (kBaseHeight * s));
}

void VoodooKillaAudioProcessorEditor::resized()
{
    view.setBounds (0, 0, kBaseWidth, kBaseHeight);
}

void VoodooKillaAudioProcessorEditor::timerCallback()
{
    view.tick();
}
