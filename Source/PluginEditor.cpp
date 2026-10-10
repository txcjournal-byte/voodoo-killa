#include "PluginEditor.h"

using namespace vk::ui;

namespace
{
// Layout on the 1000 x 625 base canvas (Design/trapwin-concept.png)
const juce::Rectangle<int> kLogo        { 12, 17, 150, 50 };
const juce::Rectangle<int> kTitle       { 186, 18, 200, 38 };
const juce::Rectangle<int> kTagline     { 188, 56, 220, 12 };
const juce::Rectangle<int> kTopBar      { 418, 21, 332, 44 };
const juce::Rectangle<int> kKill        { 774, 16, 136, 54 };
const juce::Rectangle<int> kLock        { 912, 26, 34, 34 };
const juce::Rectangle<int> kGear        { 952, 26, 34, 34 };
const juce::Rectangle<int> kCategories  { 16, 84, 138, 409 };
const juce::Rectangle<int> kWaveform    { 166, 84, 524, 62 };
const juce::Rectangle<int> kCardGrid    { 162, 151, 532, 346 };
const juce::Rectangle<int> kPad         { 702, 84, 282, 206 };
const juce::Rectangle<int> kHalftime    { 702, 300, 282, 193 };
const juce::Rectangle<int> kMacros      { 16, 505, 508, 104 };
const juce::Rectangle<int> kMorph       { 536, 505, 226, 104 };
const juce::Rectangle<int> kSide        { 774, 505, 210, 104 };

constexpr float kMinScale = 0.6f, kMaxScale = 1.5f;
constexpr int kNumSizes = 6;
const float kSizes[kNumSizes] = { 0.6f, 0.7f, 0.85f, 1.0f, 1.25f, 1.5f };

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
      halftime (p),
      when (*p.apvts.getParameter (ParamIDs::trigger), p.apvts, [&p] { p.pushUndoSnapshot(); }),
      morphFader (*p.apvts.getParameter (ParamIDs::morph), [&p] { p.pushUndoSnapshot(); }),
      duckSwitch (*p.apvts.getParameter (ParamIDs::duck), [&p] { p.pushUndoSnapshot(); })
{
    lockButton.setTooltip ("Lock: KILL picks only from the current category.");
    gearButton.setTooltip ("Settings: interface size.");
    duckSwitch.setTooltip ("Duck for 808: ducks the melody on 808 hits (sidechain input or MIDI notes below C2).");

    auto formatter = [] (int i) -> std::function<juce::String (float)>
    {
        switch (i)
        {
            case 0: case 3: case 4: return [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
            case 1: return [] (float v) { return VoodooKillaAudioProcessor::speedChoices()[juce::roundToInt (v)] + juce::String (juce::CharPointer_UTF8 ("\xc3\x97")); };
            case 2: return [] (float v) { const int t = juce::roundToInt (v); return (t > 0 ? "+" : "") + juce::String (t); };
            default: return [] (float v) { return juce::String (v, 1) + " dB"; };
        }
    };

    for (int i = 0; i < 6; ++i)
    {
        knobs.push_back (std::make_unique<Knob> (*p.apvts.getParameter (kKnobIds[i]), kKnobLabels[i],
                                                 [&p] { p.pushUndoSnapshot(); }, formatter (i), i == 2 || i == 5));
        knobs.back()->setTooltip (kKnobTips[i]);
        addAndMakeVisible (*knobs.back());
    }

    for (auto* c : std::initializer_list<juce::Component*> { &topBar, &killButton, &lockButton, &gearButton, &categories,
                                                             &waveform, &grid, &pad, &halftime, &when, &morphFader, &duckSwitch })
        addAndMakeVisible (c);

    addAndMakeVisible (thread);
    thread.setAlwaysOnTop (true);

    pad.onDotMoved = [this] { updateThread(); };
    morphFader.onNeedB = [this] { return proc.ensureSlotB(); };
    morphFader.setTooltip ("Morph A-B. Shift/right-click a card to choose B (otherwise the next card is used).");
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
    halftime.setBounds (kHalftime);

    const auto knobArea = kMacros.reduced (14, 8);
    const int kw = knobArea.getWidth() / 6;
    for (int i = 0; i < (int) knobs.size(); ++i)
        knobs[(size_t) i]->setBounds (knobArea.getX() + i * kw, knobArea.getY(), kw, knobArea.getHeight());

    morphFader.setBounds (kMorph.getX() + 14, kMorph.getY() + 26, kMorph.getWidth() - 28, 66);
    when.setBounds (kSide.getX() + 14, kSide.getY() + 20, kSide.getWidth() - 28, 34);
    duckSwitch.setBounds (kSide.getRight() - 66, kSide.getY() + 58, 52, 34);
    thread.setBounds (getLocalBounds());
    staticScale = 0.0f;
    updateThread();
}

void MainView::renderStatic (juce::Graphics& g)
{
    Theme::drawBackground (g, getLocalBounds().toFloat());

    for (auto r : { kCategories, kWaveform, kPad, kHalftime, kMacros, kMorph, kSide })
        Theme::drawCard (g, r.toFloat(), 14.0f);

    Theme::drawLogo (g, kLogo.toFloat());
    g.setColour (Colours::line2);
    g.drawVerticalLine (174, 20.0f, 66.0f);
    Theme::drawChromeText (g, "TIMER", kTitle.toFloat(), Theme::display (34.0f), juce::Justification::centredLeft);
    g.setColour (Colours::text2);
    g.setFont (Theme::monoLight (6.8f, 0.3f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("HALFTIME \xc2\xb7 TAPE \xc2\xb7 STUTTER \xc2\xb7 REVERSE")),
                kTagline.toFloat(), juce::Justification::centredLeft, false);

    g.setColour (Colours::text2);
    g.setFont (Theme::mono (7.5f, 0.18f));
    g.drawText ("DUCK FOR 808", juce::Rectangle<float> ((float) kSide.getX() + 14.0f, (float) kSide.getY() + 58.0f, 120.0f, 34.0f),
                juce::Justification::centredLeft, false);
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
    const float railY = (float) kWaveform.getCentreY() + 8.0f;
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
        morphFader.setNames (proc.getSlotA().valid ? proc.getSlotA().info.name : juce::String(),
                             proc.getSlotB().valid ? proc.getSlotB().info.name : juce::String());
        halftime.refresh();
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

    waveform.setBpm (proc.getMeters().bpm.load (std::memory_order_relaxed));
}

void MainView::showSettings (juce::Component& target)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Interface size");
    const float current = proc.getUiScale();
    for (int i = 0; i < kNumSizes; ++i)
        menu.addItem (i + 1, juce::String (juce::roundToInt (kSizes[i] * 100.0f)) + " %", true, std::abs (current - kSizes[i]) < 0.01f);
    menu.addItem (9, "Drag the bottom-right corner for any size", false, false);
    menu.addSeparator();
    menu.addItem (10, "Open user preset folder");
    menu.addSectionHeader (juce::String ("TrapWin Timer ") + JucePlugin_VersionString);

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
        if (r >= 1 && r <= kNumSizes && safe->onScaleChosen)
            safe->onScaleChosen (kSizes[r - 1]);
    });
}

// ===========================================================================
VoodooKillaAudioProcessorEditor::VoodooKillaAudioProcessorEditor (VoodooKillaAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    view.onScaleChosen = [this] (float s) { applyScale (s); };

    // read the stored size first: setting the resize limits below resizes the (still empty) window
    float s = proc.getUiScale();

    // freely resizable (fixed aspect ratio) via the corner
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kBaseWidth * kMinScale), juce::roundToInt (kBaseHeight * kMinScale),
                     juce::roundToInt (kBaseWidth * kMaxScale), juce::roundToInt (kBaseHeight * kMaxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);

    if (s <= 0.0f)
    {
        // first open: roughly the physical size of a comfortable size (~1250 px wide), whatever the Windows DPI
        float dpi = 1.0f;
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            dpi = (float) d->scale;
        s = std::round (juce::jlimit (kMinScale, 1.0f, 1.25f / juce::jmax (1.0f, dpi)) * 20.0f) / 20.0f;
    }
    sizeInitialised = true;
    applyScale (s);
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
    s = juce::jlimit (kMinScale, kMaxScale, s);
    setSize (juce::roundToInt (kBaseWidth * s), juce::roundToInt (kBaseHeight * s));
}

void VoodooKillaAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseWidth;
    view.setBounds (0, 0, kBaseWidth, kBaseHeight);
    view.setTransform (juce::AffineTransform::scale (s));
    if (sizeInitialised)
        proc.setUiScale (s);   // remember the user's size (ignored while the constructor sets things up)
}

void VoodooKillaAudioProcessorEditor::timerCallback()
{
    view.tick();
}
