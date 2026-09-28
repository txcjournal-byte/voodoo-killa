#include "TopBar.h"
#include "../PluginProcessor.h"

namespace vk::ui
{
TopBar::TopBar (VoodooKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TopBar::refresh()
{
    repaint();
}

juce::Rectangle<float> TopBar::regionBounds (Region r) const
{
    // proportional layout of the reference design
    static const float widths[] = { 0.07f, 0.29f, 0.07f, 0.11f, 0.15f, 0.08f, 0.08f, 0.1f, 0.1f };
    auto b = getLocalBounds().toFloat().reduced (10.0f, 6.0f);
    float x = b.getX();
    for (int i = 0; i < (int) r; ++i) x += widths[i] * b.getWidth();
    return { x, b.getY(), widths[(int) r] * b.getWidth(), b.getHeight() };
}

int TopBar::regionAt (juce::Point<float> p) const
{
    for (int i = 0; i < numRegions; ++i)
        if (regionBounds ((Region) i).contains (p))
            return i;
    return -1;
}

void TopBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Theme::drawPaper (g, b, 31337u, true);

    auto hl = [&] (Region r) { return hover == (int) r ? Colours::red : Colours::ink; };
    auto sep = [&] (Region r)
    {
        const auto rb = regionBounds (r);
        g.setColour (Colours::ink.withAlpha (0.25f));
        g.drawVerticalLine ((int) rb.getX(), rb.getY() + 4.0f, rb.getBottom() - 4.0f);
    };

    // ◀ ▶
    for (auto r : { prev, next })
    {
        const auto rb = regionBounds (r).withSizeKeepingCentre (10.0f, 16.0f);
        juce::Path tri;
        if (r == prev) tri.addTriangle (rb.getRight(), rb.getY(), rb.getRight(), rb.getBottom(), rb.getX(), rb.getCentreY());
        else           tri.addTriangle (rb.getX(), rb.getY(), rb.getX(), rb.getBottom(), rb.getRight(), rb.getCentreY());
        g.setColour (hl (r));
        g.fillPath (tri);
    }

    // name
    g.setColour (hover == name ? Colours::red : Colours::ink);
    g.setFont (Theme::typewriter (16.0f));
    g.drawFittedText (proc.getDisplayName().toUpperCase(), regionBounds (name).toNearestInt(), juce::Justification::centred, 1, 0.7f);

    // ★
    sep (star);
    const bool fav = proc.getSlotA().valid && proc.isFavourite (proc.getSlotA().info.id);
    const auto sb = regionBounds (star).withSizeKeepingCentre (22.0f, 22.0f);
    g.setColour (fav ? Colours::red : hl (star));
    if (fav) g.fillPath (Theme::starIcon (sb));
    else     g.strokePath (Theme::starIcon (sb), juce::PathStrokeType (1.6f));

    // A | B (red = where the next plain click goes)
    sep (ab);
    {
        const auto rb = regionBounds (ab);
        const bool bNext = proc.getClickSlot() == VoodooKillaAudioProcessor::Slot::B;
        g.setFont (Theme::typewriter (18.0f));
        g.setColour (! bNext ? Colours::red : Colours::ink);
        g.drawText ("A", rb.withWidth (rb.getWidth() * 0.42f), juce::Justification::centredRight, false);
        g.setColour (Colours::ink);
        g.drawText ("|", rb, juce::Justification::centred, false);
        g.setColour (bNext ? Colours::red : (proc.getSlotB().valid ? Colours::ink : Colours::grey));
        g.drawText ("B", rb.withTrimmedLeft (rb.getWidth() * 0.58f), juce::Justification::centredLeft, false);
    }

    // ↶ ↷
    sep (undo);
    g.setColour (proc.canUndo() ? hl (undo) : Colours::grey);
    g.fillPath (Theme::undoIcon (regionBounds (undo).withSizeKeepingCentre (22.0f, 22.0f), false));
    g.setColour (proc.canRedo() ? hl (redo) : Colours::grey);
    g.fillPath (Theme::undoIcon (regionBounds (redo).withSizeKeepingCentre (22.0f, 22.0f), true));

    // 💾
    sep (save);
    g.setColour (hl (save));
    g.fillPath (Theme::saveIcon (regionBounds (save).withSizeKeepingCentre (22.0f, 22.0f)));

    // 🎲
    sep (dice);
    Theme::drawDice (g, regionBounds (dice).withSizeKeepingCentre (22.0f, 22.0f), hl (dice), Colours::paper);
}

void TopBar::mouseMove (const juce::MouseEvent& e)
{
    const int r = regionAt (e.position);
    if (r != hover)
    {
        hover = r;
        repaint();
    }
}

void TopBar::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void TopBar::mouseUp (const juce::MouseEvent& e)
{
    switch (regionAt (e.position))
    {
        case prev: proc.stepPreset (-1); break;
        case next: proc.stepPreset (1); break;
        case name: showPresetMenu(); break;
        case star:
            if (proc.getSlotA().valid)
                proc.toggleFavourite (proc.getSlotA().info.id);
            break;
        case ab:
            if (e.mods.isPopupMenu() && proc.getSlotB().valid)
                proc.clearSlotB();
            else
                proc.setClickSlot (proc.getClickSlot() == VoodooKillaAudioProcessor::Slot::A ? VoodooKillaAudioProcessor::Slot::B
                                                                                             : VoodooKillaAudioProcessor::Slot::A);
            break;
        case undo: proc.undo(); break;
        case redo: proc.redo(); break;
        case save: showSaveDialog(); break;
        case dice:
        {
            // cast a random A/B pair and morph halfway
            auto& r = juce::Random::getSystemRandom();
            const int a = proc.library.randomIndex ((uint32_t) r.nextInt() | 1u, proc.getCategoryLock() ? proc.getCurrentCategory() : -1, -1);
            const int b = proc.library.randomIndex ((uint32_t) r.nextInt() | 1u, proc.getCategoryLock() ? proc.getCurrentCategory() : -1, a);
            proc.loadPreset (a, VoodooKillaAudioProcessor::Slot::A, false);
            proc.loadPreset (b, VoodooKillaAudioProcessor::Slot::B, false);
            if (auto* m = proc.apvts.getParameter (ParamIDs::morph))
                m->setValueNotifyingHost (0.5f);
            proc.pushUndoSnapshot();
            break;
        }
        default: break;
    }
    repaint();
}

juce::String TopBar::getTooltip()
{
    switch (regionAt (getMouseXYRelative().toFloat()))
    {
        case prev: return "Previous preset.";
        case next: return "Next preset.";
        case name: return "Browse all presets, favourites and user presets.";
        case star: return "Add or remove the current preset from favourites.";
        case ab:   return "Which slot a plain card click loads. Right-click to clear B.";
        case undo: return "Undo.";
        case redo: return "Redo.";
        case save: return "Save the current preset (A, B and morph) as a user preset.";
        case dice: return "Random A/B pair, morphed halfway.";
        default:   return {};
    }
}

void TopBar::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& lib = proc.library;
    const int current = proc.getSlotA().libraryIndex;

    juce::PopupMenu favs;
    for (int i = 0; i < lib.size(); ++i)
        if (proc.isFavourite (lib.get (i)->id))
            favs.addItem (i + 1, lib.get (i)->name, true, i == current);
    menu.addSubMenu ("Favourites", favs, favs.getNumItems() > 0);
    menu.addSeparator();

    for (int c = 0; c < (int) lib.getCategories().size(); ++c)
    {
        juce::PopupMenu sub;
        for (int k = 0; k < vk::kPresetsPerCategory; ++k)
        {
            const int idx = lib.factoryIndex (c, k);
            if (const auto* p = lib.get (idx))
                sub.addItem (idx + 1, p->name + "  -  " + p->desc, true, idx == current);
        }
        menu.addSubMenu (lib.getCategories()[(size_t) c].name, sub);
    }

    menu.addSeparator();
    juce::PopupMenu user;
    for (int i = (int) lib.getFactory().size(); i < lib.size(); ++i)
        user.addItem (i + 1, lib.get (i)->name, true, i == current);
    menu.addSubMenu ("User", user, user.getNumItems() > 0);

    juce::Component::SafePointer<TopBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (220),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->proc.loadPreset (result - 1, VoodooKillaAudioProcessor::Slot::A);
                        });
}

/**
    Inline "save preset" panel drawn inside the plugin window.
    (A separate modal desktop window can end up hidden behind the host's plugin window and block the UI.)
*/
class SavePanel : public juce::Component
{
public:
    SavePanel (VoodooKillaAudioProcessor& p, std::function<void()> onClose) : proc (p), close (std::move (onClose))
    {
        name.setText (proc.getSlotA().info.name.trimCharactersAtEnd ("*"), false);
        name.setFont (Theme::typewriter (17.0f));
        name.setJustification (juce::Justification::centredLeft);
        name.onReturnKey = [this] { save(); };
        name.onEscapeKey = [this] { finish(); };
        addAndMakeVisible (name);
        for (auto* b : { &saveButton, &cancelButton })
            addAndMakeVisible (b);
        saveButton.onClick = [this] { save(); };
        cancelButton.onClick = [this] { finish(); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.6f));
        const auto box = panel();
        Theme::drawPaper (g, box, 99u, true);
        g.setColour (Colours::ink);
        g.setFont (Theme::marker (24.0f));
        g.drawText ("SAVE PRESET", box.withHeight (46.0f).reduced (16.0f, 0.0f), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto box = panel().reduced (16.0f).toNearestInt();
        box.removeFromTop (34);
        name.setBounds (box.removeFromTop (34));
        box.removeFromTop (12);
        auto row = box.removeFromTop (30);
        cancelButton.setBounds (row.removeFromRight (100));
        row.removeFromRight (8);
        saveButton.setBounds (row.removeFromRight (100));
    }

    void grabFocus() { name.grabKeyboardFocus(); name.selectAll(); }

private:
    juce::Rectangle<float> panel() const { return getLocalBounds().toFloat().withSizeKeepingCentre (380.0f, 150.0f); }
    void save()
    {
        if (name.getText().trim().isNotEmpty())
            proc.saveUserPreset (name.getText());
        finish();
    }
    void finish()
    {
        auto fn = close;
        juce::MessageManager::callAsync (fn);   // delete after the click handler returns
    }

    VoodooKillaAudioProcessor& proc;
    std::function<void()> close;
    juce::TextEditor name;
    juce::TextButton saveButton { "Save" }, cancelButton { "Cancel" };
};

void TopBar::showSaveDialog()
{
    auto* root = getTopLevelComponent();
    for (auto* c = getParentComponent(); c != nullptr; c = c->getParentComponent())
        if (c->getParentComponent() != nullptr && dynamic_cast<juce::AudioProcessorEditor*> (c->getParentComponent()) != nullptr)
            root = c;   // the scaled main view
    if (root == nullptr)
        return;

    juce::Component::SafePointer<TopBar> safe (this);
    auto panel = std::make_unique<SavePanel> (proc, [safe]
    {
        if (safe != nullptr)
        {
            safe->savePanel.reset();
            safe->repaint();
        }
    });
    panel->setBounds (root->getLocalBounds());
    root->addAndMakeVisible (*panel);
    panel->grabFocus();
    savePanel = std::move (panel);
}

// ===========================================================================
KillButton::KillButton (VoodooKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("KILL: random preset (lock = current category). Right-click: Mutate (+-15 %).");
}

void KillButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    if (pressed) r = r.translated (1.0f, 1.0f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (r);
    g.setColour (isMouseOver() ? Colours::red.brighter (0.2f) : Colours::red);
    g.drawRect (r, 2.5f);

    auto content = r.reduced (8.0f, 6.0f);
    const auto diceArea = content.removeFromRight (content.getHeight() * 0.7f).withSizeKeepingCentre (content.getHeight() * 0.62f, content.getHeight() * 0.62f);
    g.setFont (Theme::marker (content.getHeight() * 0.9f));
    g.drawFittedText ("KILL", content.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    Theme::drawDice (g, diceArea, Colours::red, juce::Colour (0xff0a0a0a));
}

void KillButton::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    repaint();
}

void KillButton::mouseUp (const juce::MouseEvent& e)
{
    pressed = false;
    repaint();
    if (getLocalBounds().contains (e.getPosition()))
        proc.kill (e.mods.isPopupMenu());
}

// ===========================================================================
IconButton::IconButton (Kind k, std::function<bool()> on, std::function<void (IconButton&)> onClick)
    : kind (k), isOn (std::move (on)), click (std::move (onClick))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paint (juce::Graphics& g)
{
    const bool on = isOn && isOn();
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (on ? Colours::red : (isMouseOver() ? Colours::paper : Colours::paper.withAlpha (0.75f)));
    g.fillPath (kind == Kind::lock ? Theme::lockIcon (r, on) : Theme::gearIcon (r));
}

void IconButton::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && click)
    {
        click (*this);
        repaint();
    }
}
} // namespace vk::ui
