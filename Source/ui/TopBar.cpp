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
    // widths in design units, scaled to the pill
    static const float widths[] = { 20.0f, 132.0f, 20.0f, 34.0f, 60.0f, 28.0f, 26.0f, 28.0f };
    float total = 0.0f;
    for (auto w : widths) total += w;
    total += 2.0f * kSepWidth;
    auto b = getLocalBounds().toFloat().reduced (16.0f, 6.0f);
    const float k = b.getWidth() / total;
    float x = b.getX();
    for (int i = 0; i < (int) r; ++i)
    {
        x += widths[i] * k;
        if (i == (int) next || i == (int) ab) x += kSepWidth * k;
    }
    return { x, b.getY(), widths[(int) r] * k, b.getHeight() };
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
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    {
        juce::Path pill;
        pill.addRoundedRectangle (b, b.getHeight() * 0.5f);
        juce::DropShadow (Colours::glow.withAlpha (0.18f), 18, {}).drawForPath (g, pill);
        g.setGradientFill (juce::ColourGradient (Colours::card2, 0.0f, b.getY(), Colours::well, 0.0f, b.getBottom(), false));
        g.fillPath (pill);
        g.setColour (Colours::line2);
        g.strokePath (pill, juce::PathStrokeType (1.0f));
    }

    auto hl = [&] (Region r, juce::Colour base) { return hover == (int) r ? Colours::accentL : base; };
    auto sepAfter = [&] (Region r)
    {
        const auto rb = regionBounds (r);
        const float x = rb.getRight() + (regionBounds ((Region) (r + 1)).getX() - rb.getRight()) * 0.5f;
        g.setColour (Colours::line2);
        g.drawVerticalLine ((int) x, rb.getY() + 6.0f, rb.getBottom() - 6.0f);
    };

    // ◀ ▶
    for (auto r : { prev, next })
    {
        const auto rb = regionBounds (r).withSizeKeepingCentre (7.0f, 10.0f);
        juce::Path tri;
        if (r == prev) tri.addTriangle (rb.getRight(), rb.getY(), rb.getRight(), rb.getBottom(), rb.getX(), rb.getCentreY());
        else           tri.addTriangle (rb.getX(), rb.getY(), rb.getX(), rb.getBottom(), rb.getRight(), rb.getCentreY());
        g.setColour (hl (r, Colours::text2));
        g.fillPath (tri);
    }

    // name + "CATEGORY · NN / 08"
    {
        const auto rb = regionBounds (name);
        g.setColour (hover == name ? Colours::accentL : Colours::text);
        g.setFont (Theme::bold (15.0f));
        g.drawFittedText (proc.getDisplayName().toUpperCase(), rb.withTrimmedBottom (rb.getHeight() * 0.4f).toNearestInt(),
                          juce::Justification::centredBottom, 1, 0.65f);
        juce::String sub;
        const auto& a = proc.getSlotA();
        if (a.valid)
        {
            if (a.info.factory && a.libraryIndex >= 0)
                sub = a.info.category.toUpperCase() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
                    + juce::String (a.info.indexInCategory + 1).paddedLeft ('0', 2) + " / 08";
            else
                sub = a.libraryIndex >= 0 ? juce::String ("USER PRESET") : juce::String ("MUTATED");
        }
        g.setColour (Colours::muted);
        g.setFont (Theme::monoLight (7.0f, 0.22f));
        g.drawFittedText (sub, rb.withTrimmedTop (rb.getHeight() * 0.62f).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
    }
    sepAfter (next);

    // ★
    const bool fav = proc.getSlotA().valid && proc.isFavourite (proc.getSlotA().info.id);
    const auto sb = regionBounds (star).withSizeKeepingCentre (15.0f, 15.0f);
    g.setColour (fav ? Colours::accentL : hl (star, Colours::text2));
    if (fav) g.fillPath (Theme::starIcon (sb));
    else     g.strokePath (Theme::starIcon (sb), juce::PathStrokeType (1.3f));

    // A | B segmented (chrome = where the next plain click goes)
    {
        const auto rb = regionBounds (ab).withSizeKeepingCentre (regionBounds (ab).getWidth(), 22.0f);
        g.setColour (Colours::well);
        g.fillRoundedRectangle (rb, 11.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (rb, 11.0f, 1.0f);
        const bool bNext = proc.getClickSlot() == VoodooKillaAudioProcessor::Slot::B;
        const auto left = rb.withWidth (rb.getWidth() * 0.5f).reduced (2.0f);
        const auto right = rb.withTrimmedLeft (rb.getWidth() * 0.5f).reduced (2.0f);
        Theme::drawChromePill (g, bNext ? right : left, 0.25f);
        g.setFont (Theme::display (11.0f));
        g.setColour (bNext ? Colours::text2 : Colours::well);
        g.drawText ("A", left, juce::Justification::centred, false);
        g.setColour (bNext ? Colours::well : (proc.getSlotB().valid ? Colours::text2 : Colours::muted));
        g.drawText ("B", right, juce::Justification::centred, false);
    }
    sepAfter (ab);

    // ↶ ↷ 💾
    g.setColour (proc.canUndo() ? hl (undo, Colours::text2) : Colours::muted.withAlpha (0.5f));
    g.fillPath (Theme::undoIcon (regionBounds (undo).withSizeKeepingCentre (15.0f, 15.0f), false));
    g.setColour (proc.canRedo() ? hl (redo, Colours::text2) : Colours::muted.withAlpha (0.5f));
    g.fillPath (Theme::undoIcon (regionBounds (redo).withSizeKeepingCentre (15.0f, 15.0f), true));
    g.setColour (hl (save, Colours::text2));
    g.fillPath (Theme::saveIcon (regionBounds (save).withSizeKeepingCentre (14.0f, 14.0f)));
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
        name.setFont (Theme::bold (16.0f));
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
        g.fillAll (Colours::bg.withAlpha (0.7f));
        const auto box = panel();
        Theme::drawCard (g, box, 16.0f, 0.3f);
        Theme::drawChromeText (g, "SAVE PRESET", box.withHeight (46.0f).reduced (16.0f, 0.0f), Theme::display (20.0f),
                               juce::Justification::centredLeft, false);
        return;
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
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    if (pressed) r = r.reduced (1.0f);
    Theme::drawChromePill (g, r, isMouseOver() ? 0.85f : 0.55f);

    auto content = r.reduced (r.getHeight() * 0.45f, 0.0f);
    const float ds = r.getHeight() * 0.36f;
    const auto diceArea = content.removeFromRight (ds + 4.0f).withSizeKeepingCentre (ds, ds);
    g.setColour (Colours::well);
    g.setFont (Theme::display (r.getHeight() * 0.48f));
    g.drawFittedText ("KILL", content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    Theme::drawDice (g, diceArea, Colours::well, juce::Colour (0xffd8dde7));
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
    g.setColour (Colours::card2);
    g.fillEllipse (r);
    g.setColour (on ? Colours::accentL : (isMouseOver() ? Colours::line2.withAlpha (0.6f) : Colours::line2));
    g.drawEllipse (r.reduced (0.5f), 1.0f);
    g.setColour (on ? Colours::accentL : (isMouseOver() ? Colours::text : Colours::text2));
    g.fillPath (kind == Kind::lock ? Theme::lockIcon (r.reduced (r.getWidth() * 0.32f), on) : Theme::gearIcon (r.reduced (r.getWidth() * 0.3f)));
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
