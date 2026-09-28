#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int kMaxUndo = 64;
constexpr int kFirstCategoryNote = 48;   // C2 (C3 = 60 convention) .. B2 -> categories 1-12
constexpr int kFirstCardNote = 60;       // C3 .. G3 -> cards 1-8
constexpr int kDuckNoteBelow = 48;       // notes below C2 trigger the 808 duck

juce::File favouritesFile()
{
    return vk::PresetLibrary::getUserPresetFolder().getParentDirectory().getChildFile ("favourites.json");
}
}

// ===========================================================================
const juce::StringArray& VoodooKillaAudioProcessor::speedChoices()
{
    static const juce::StringArray c { "1/4", "1/2", "1", "2", "4" };
    return c;
}

float VoodooKillaAudioProcessor::speedMultiplier (int index) noexcept
{
    static const float m[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
    return m[juce::jlimit (0, 4, index)];
}

juce::AudioProcessorValueTreeState::ParameterLayout VoodooKillaAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })
                                              .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::amount, 1 }, "Amount", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::speed, 1 }, "Speed", speedChoices(), 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tone, 1 }, "Tone", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::space, 1 }, "Space", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::out, 1 }, "Out", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::duck, 1 }, "808 Duck", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::morph, 1 }, "Morph", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    StringArray triggers;
    for (int i = 0; i < (int) vk::TriggerMode::count; ++i)
        triggers.add (vk::triggerLabel ((vk::TriggerMode) i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::trigger, 1 }, "Trigger", triggers, 0));

    const bool defaultSteps[vk::kNumSteps] = { true, false, true, true, false, false, false, true,
                                               true, false, true, true, false, false, true, false };
    for (int i = 0; i < vk::kNumSteps; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::step (i), 1 }, "Step " + String (i + 1), defaultSteps[i]));

    return layout;
}

// ===========================================================================
VoodooKillaAudioProcessor::VoodooKillaAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VoodooKilla", createLayout())
{
    pAmount  = apvts.getRawParameterValue (ParamIDs::amount);
    pSpeed   = apvts.getRawParameterValue (ParamIDs::speed);
    pTone    = apvts.getRawParameterValue (ParamIDs::tone);
    pSpace   = apvts.getRawParameterValue (ParamIDs::space);
    pMix     = apvts.getRawParameterValue (ParamIDs::mix);
    pOut     = apvts.getRawParameterValue (ParamIDs::out);
    pDuck    = apvts.getRawParameterValue (ParamIDs::duck);
    pMorph   = apvts.getRawParameterValue (ParamIDs::morph);
    pTrigger = apvts.getRawParameterValue (ParamIDs::trigger);
    for (int i = 0; i < vk::kNumSteps; ++i)
        pSteps[(size_t) i] = apvts.getRawParameterValue (ParamIDs::step (i));

    library.scanUserPresets();
    loadFavourites();

    // start with the first factory preset (Halfcut)
    loadPreset (0, Slot::A, false);
    pushUndoSnapshot();

    startTimerHz (30);
}

VoodooKillaAudioProcessor::~VoodooKillaAudioProcessor()
{
    stopTimer();
}

// ===========================================================================
void VoodooKillaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
    heldCardNotes = 0;

    // the engine was reset: re-send the current slots
    const juce::SpinLock::ScopedLockType sl (producerLock);
    if (slotA.valid) engine.setSlotA (slotA.info.data);
    engine.setSlotB (slotB.valid ? &slotB.info.data : nullptr);
}

bool VoodooKillaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

// ===========================================================================
void VoodooKillaAudioProcessor::pushSlotCommand (const SlotCommand& c)
{
    const juce::SpinLock::ScopedLockType sl (producerLock);
    const auto scope = commandFifo.write (1);
    if (scope.blockSize1 > 0)
        commands[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0)
        commands[(size_t) scope.startIndex2] = c;
}

void VoodooKillaAudioProcessor::sendSlotsToAudio()
{
    SlotCommand a;
    a.slot = 0;
    a.clear = ! slotA.valid;
    if (slotA.valid) a.data = slotA.info.data;
    pushSlotCommand (a);

    SlotCommand b;
    b.slot = 1;
    b.clear = ! slotB.valid;
    b.resetVelocity = false;
    if (slotB.valid) b.data = slotB.info.data;
    pushSlotCommand (b);
}

void VoodooKillaAudioProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        if (note >= kFirstCategoryNote && note < kFirstCategoryNote + vk::kNumCategories)
        {
            const int cat = note - kFirstCategoryNote;
            audioCategory.store (cat);
            midiCategoryEvent.store (cat);
        }
        else if (note >= kFirstCardNote && note < kFirstCardNote + vk::kPresetsPerCategory)
        {
            const int idx = library.factoryIndex (audioCategory.load(), note - kFirstCardNote);
            const auto& factory = library.getFactory();
            if (idx >= 0 && idx < (int) factory.size())
            {
                engine.setSlotA (factory[(size_t) idx].data);
                engine.setSlotB (nullptr);
                engine.setVelocity (juce::jlimit (0.05f, 1.0f, m.getFloatVelocity()));
                midiCardEvent.store (idx);
            }
            ++heldCardNotes;
        }
        else if (note < kDuckNoteBelow)
        {
            engine.trigger808();
        }
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        if (note >= kFirstCardNote && note < kFirstCardNote + vk::kPresetsPerCategory)
            heldCardNotes = std::max (0, heldCardNotes - 1);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldCardNotes = 0;
    }
    engine.setHold (uiHold.load() || heldCardNotes > 0);
}

void VoodooKillaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // ---- slot updates from the message thread (lock-free)
    {
        const auto scope = commandFifo.read (commandFifo.getNumReady());
        auto apply = [this] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& c = commands[(size_t) i];
                if (c.slot == 0)
                {
                    if (! c.clear) engine.setSlotA (c.data);
                    if (c.resetVelocity) engine.setVelocity (1.0f);
                }
                else
                {
                    engine.setSlotB (c.clear ? nullptr : &c.data);
                }
            }
        };
        apply (scope.startIndex1, scope.blockSize1);
        apply (scope.startIndex2, scope.blockSize2);
    }

    // ---- transport
    vk::TransportInfo t;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            t.hostTransport = true;
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { t.hasPpq = true; t.ppq = *ppq; }
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto sig = pos->getTimeSignature()) { t.timeSigNum = sig->numerator; t.timeSigDen = sig->denominator; }
            if (auto bar = pos->getPpqPositionOfLastBarStart()) { t.hasBarStart = true; t.barStartPpq = *bar; }
        }
    }

    // ---- controls
    vk::EngineControls c;
    c.amount = pAmount->load();
    c.speed = speedMultiplier ((int) pSpeed->load());
    c.tone = pTone->load();
    c.space = pSpace->load();
    c.mix = pMix->load();
    c.outDb = pOut->load();
    c.duck = pDuck->load() > 0.5f;
    c.morph = pMorph->load();
    c.trigger = (vk::TriggerMode) juce::jlimit (0, (int) vk::TriggerMode::count - 1, (int) pTrigger->load());
    for (int i = 0; i < vk::kNumSteps; ++i)
        c.steps[(size_t) i] = pSteps[(size_t) i]->load() > 0.5f;

    auto main = getBusBuffer (buffer, true, 0);
    juce::AudioBuffer<float> sidechain;
    const juce::AudioBuffer<float>* sc = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        sidechain = getBusBuffer (buffer, true, 1);
        if (sidechain.getNumChannels() > 0)
            sc = &sidechain;
    }

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n = main.getNumSamples();
    engine.setHold (uiHold.load() || heldCardNotes > 0);
    engine.beginBlock (t, n);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, n, meta.samplePosition);
        if (s > pos)
        {
            engine.processRange (main, pos, s - pos, sc, c);
            pos = s;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
        engine.processRange (main, pos, n - pos, sc, c);
}

// ===========================================================================
void VoodooKillaAudioProcessor::setParamValue (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - norm) > 1.0e-6f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    }
}

void VoodooKillaAudioProcessor::applyPresetDefaults (const vk::PresetInfo& p)
{
    // The trigger belongs to the user: it starts on Always and only changes when clicked.
    if (p.data.duck.amount > 0.0f)
        setParamValue (ParamIDs::duck, 1.0f);
}

void VoodooKillaAudioProcessor::loadPreset (int libraryIndex, Slot slot, bool recordUndo)
{
    const auto* p = library.get (libraryIndex);
    if (p == nullptr)
        return;

    if (slot == Slot::B && slotA.valid && libraryIndex != slotA.libraryIndex)
    {
        slotB.valid = true;
        slotB.libraryIndex = libraryIndex;
        slotB.info = *p;
        lastMorphSideB = pMorph->load() >= 0.5f;
    }
    else
    {
        slotA.valid = true;
        slotA.libraryIndex = libraryIndex;
        slotA.info = *p;
        slotB = SlotState {};
        setParamValue (ParamIDs::morph, 0.0f);
        lastMorphSideB = false;
        applyPresetDefaults (*p);

        // user presets can carry an A+B pair and a morph position
        if (const auto* extra = library.getUserExtra (libraryIndex); extra != nullptr && extra->hasB)
        {
            slotB.valid = true;
            slotB.libraryIndex = library.findById (extra->bId);
            slotB.info = extra->b;
            setParamValue (ParamIDs::morph, extra->morph);
            lastMorphSideB = extra->morph >= 0.5f;
        }

        if (p->categoryIndex >= 0)
            setCurrentCategory (p->categoryIndex);
    }

    sendSlotsToAudio();
    ++selectionVersion;
    if (recordUndo)
        pushUndoSnapshot();
}

void VoodooKillaAudioProcessor::clearSlotB (bool recordUndo)
{
    if (! slotB.valid)
        return;
    slotB = SlotState {};
    setParamValue (ParamIDs::morph, 0.0f);
    lastMorphSideB = false;
    sendSlotsToAudio();
    ++selectionVersion;
    if (recordUndo)
        pushUndoSnapshot();
}

void VoodooKillaAudioProcessor::stepPreset (int delta)
{
    const int count = library.size();
    if (count <= 0)
        return;
    int idx = slotA.libraryIndex >= 0 ? slotA.libraryIndex : library.factoryIndex (currentCategory, 0);
    idx = ((idx + delta) % count + count) % count;
    loadPreset (idx, Slot::A);
}

void VoodooKillaAudioProcessor::kill (bool mutate, uint32_t seed)
{
    if (seed == 0)
        seed = (uint32_t) juce::Random::getSystemRandom().nextInt() | 1u;

    if (mutate)
    {
        if (! slotA.valid)
            return;
        slotA.info.data = vk::mutatePreset (slotA.info.data, seed);
        if (! slotA.info.name.endsWithChar ('*'))
            slotA.info.name << "*";
        slotA.info.id = "mutated";
        slotA.libraryIndex = -1;
        sendSlotsToAudio();
        ++selectionVersion;
        pushUndoSnapshot();
        return;
    }

    const int idx = library.randomIndex (seed, categoryLock ? currentCategory : -1, slotA.libraryIndex);
    if (idx >= 0)
        loadPreset (idx, Slot::A);
}

void VoodooKillaAudioProcessor::setCurrentCategory (int category)
{
    category = juce::jlimit (0, vk::kNumCategories - 1, category);
    if (category != currentCategory)
    {
        currentCategory = category;
        ++selectionVersion;
    }
    audioCategory.store (category);
}

juce::String VoodooKillaAudioProcessor::getDisplayName() const
{
    return slotA.valid ? slotA.info.name : juce::String ("—");
}

// ===========================================================================
void VoodooKillaAudioProcessor::timerCallback()
{
    const int cat = midiCategoryEvent.exchange (-1);
    if (cat >= 0 && cat != currentCategory)
    {
        currentCategory = cat;
        ++selectionVersion;
    }

    const int card = midiCardEvent.exchange (-1);
    if (card >= 0)
    {
        if (const auto* p = library.get (card))
        {
            slotA.valid = true;
            slotA.libraryIndex = card;
            slotA.info = *p;
            slotB = SlotState {};
            lastMorphSideB = false;
            applyPresetDefaults (*p);
            ++selectionVersion;
        }
    }

}

bool VoodooKillaAudioProcessor::ensureSlotB()
{
    if (slotB.valid || ! slotA.valid)
        return slotB.valid;

    // no B chosen yet: take the neighbouring card of the same category
    int idx = slotA.libraryIndex;
    if (idx < 0 || idx >= (int) library.getFactory().size())
        idx = library.factoryIndex (currentCategory, 0);
    const int cat = idx / vk::kPresetsPerCategory;
    const int next = library.factoryIndex (cat, (idx % vk::kPresetsPerCategory + 1) % vk::kPresetsPerCategory);
    if (const auto* p = library.get (next))
    {
        slotB.valid = true;
        slotB.libraryIndex = next;
        slotB.info = *p;
        sendSlotsToAudio();
        ++selectionVersion;
    }
    return slotB.valid;
}

// ===========================================================================
void VoodooKillaAudioProcessor::toggleFavourite (const juce::String& presetId)
{
    if (presetId.isEmpty() || presetId == "mutated")
        return;
    if (favourites.contains (presetId))
        favourites.removeString (presetId);
    else
        favourites.add (presetId);
    saveFavourites();
    ++selectionVersion;
}

void VoodooKillaAudioProcessor::loadFavourites()
{
    favourites.clear();
    const auto v = juce::JSON::parse (favouritesFile().loadFileAsString());
    if (auto* arr = v.getArray())
        for (auto& id : *arr)
            favourites.add (id.toString());
}

void VoodooKillaAudioProcessor::saveFavourites()
{
    juce::Array<juce::var> arr;
    for (auto& f : favourites)
        arr.add (f);
    auto file = favouritesFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (arr)));
}

int VoodooKillaAudioProcessor::saveUserPreset (const juce::String& name)
{
    if (! slotA.valid)
        return -1;
    auto a = slotA.info;
    const int idx = library.saveUserPreset (name, a, slotB.valid ? &slotB.info : nullptr, pMorph->load());
    if (idx >= 0)
    {
        slotA.libraryIndex = idx;
        slotA.info.name = library.get (idx)->name;
        slotA.info.id = library.get (idx)->id;
        ++selectionVersion;
    }
    return idx;
}

// ===========================================================================
juce::ValueTree VoodooKillaAudioProcessor::createStateTree()
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("SELECTION"); old.isValid())
        state.removeChild (old, nullptr);

    juce::ValueTree sel ("SELECTION");
    if (slotA.valid)
    {
        sel.setProperty ("aId", slotA.info.id, nullptr);
        sel.setProperty ("aData", juce::JSON::toString (vk::presetInfoToVar (slotA.info), true), nullptr);
    }
    if (slotB.valid)
    {
        sel.setProperty ("bId", slotB.info.id, nullptr);
        sel.setProperty ("bData", juce::JSON::toString (vk::presetInfoToVar (slotB.info), true), nullptr);
    }
    sel.setProperty ("category", currentCategory, nullptr);
    sel.setProperty ("lock", categoryLock, nullptr);
    sel.setProperty ("uiScale", uiScale, nullptr);
    sel.setProperty ("clickSlot", clickSlot == Slot::B ? 1 : 0, nullptr);
    state.appendChild (sel, nullptr);
    return state;
}

void VoodooKillaAudioProcessor::restoreSlotFromTree (const juce::ValueTree& sel, const char* idKey, const char* dataKey, SlotState& slot)
{
    slot = SlotState {};
    const auto id = sel.getProperty (idKey).toString();
    const auto data = sel.getProperty (dataKey).toString();
    if (id.isEmpty() && data.isEmpty())
        return;

    const int idx = library.findById (id);
    if (data.isNotEmpty())
    {
        const auto v = juce::JSON::parse (data);
        if (v.isObject())
        {
            slot.valid = true;
            slot.info = vk::presetInfoFromVar (v);
            slot.info.categoryIndex = library.categoryIndexOf (slot.info.category);
            slot.info.factory = idx >= 0 && idx < (int) library.getFactory().size();
            slot.libraryIndex = idx;
            return;
        }
    }
    if (const auto* p = library.get (idx))
    {
        slot.valid = true;
        slot.info = *p;
        slot.libraryIndex = idx;
    }
}

void VoodooKillaAudioProcessor::applyStateTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return;

    auto copy = tree.createCopy();
    const auto sel = copy.getChildWithName ("SELECTION");
    if (sel.isValid())
        copy.removeChild (sel, nullptr);
    apvts.replaceState (copy);

    if (sel.isValid())
    {
        restoreSlotFromTree (sel, "aId", "aData", slotA);
        restoreSlotFromTree (sel, "bId", "bData", slotB);
        if (! slotA.valid)
            slotB = SlotState {};
        setCurrentCategory ((int) sel.getProperty ("category", currentCategory));
        categoryLock = (bool) sel.getProperty ("lock", false);
        uiScale = (float) sel.getProperty ("uiScale", uiScale);
        clickSlot = (int) sel.getProperty ("clickSlot", 0) == 1 ? Slot::B : Slot::A;
    }
    lastMorphSideB = pMorph->load() >= 0.5f;
    sendSlotsToAudio();
    ++selectionVersion;
}

void VoodooKillaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = createStateTree();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoodooKillaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        applyStateTree (juce::ValueTree::fromXml (*xml));
        undoStack.clear();
        undoIndex = -1;
        pushUndoSnapshot();
    }
}

// ===========================================================================
void VoodooKillaAudioProcessor::pushUndoSnapshot()
{
    if (restoringUndo)
        return;
    auto snap = createStateTree();
    if (undoIndex >= 0 && undoIndex < (int) undoStack.size() && undoStack[(size_t) undoIndex].isEquivalentTo (snap))
        return;

    undoStack.resize ((size_t) (undoIndex + 1));
    undoStack.push_back (snap);
    if ((int) undoStack.size() > kMaxUndo)
        undoStack.erase (undoStack.begin());
    undoIndex = (int) undoStack.size() - 1;
}

bool VoodooKillaAudioProcessor::undo()
{
    if (! canUndo())
        return false;
    --undoIndex;
    const juce::ScopedValueSetter<bool> svs (restoringUndo, true);
    const float scale = uiScale;
    applyStateTree (undoStack[(size_t) undoIndex]);
    uiScale = scale;
    return true;
}

bool VoodooKillaAudioProcessor::redo()
{
    if (! canRedo())
        return false;
    ++undoIndex;
    const juce::ScopedValueSetter<bool> svs (restoringUndo, true);
    const float scale = uiScale;
    applyStateTree (undoStack[(size_t) undoIndex]);
    uiScale = scale;
    return true;
}

// ===========================================================================
juce::AudioProcessorEditor* VoodooKillaAudioProcessor::createEditor()
{
    return new VoodooKillaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoodooKillaAudioProcessor();
}
