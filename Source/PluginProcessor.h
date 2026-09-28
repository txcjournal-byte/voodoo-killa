#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "engine/Engine.h"
#include "engine/PresetLibrary.h"

namespace ParamIDs
{
    inline constexpr const char* amount  = "amount";
    inline constexpr const char* speed   = "speed";
    inline constexpr const char* tone    = "tone";
    inline constexpr const char* space   = "space";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* out     = "out";
    inline constexpr const char* duck    = "duck";
    inline constexpr const char* morph   = "morph";
    inline constexpr const char* trigger = "trigger";
    inline juce::String step (int i) { return "step" + juce::String (i + 1); }
}

/**
    Voodoo Killa processor.

    Threading model
    - Message thread owns the selection (slot A / slot B PresetInfo, category, undo history).
    - Preset data reaches the audio thread through a lock-free FIFO of POD commands.
    - MIDI card/category selection happens on the audio thread (reads the immutable factory list)
      and is reported back to the message thread through atomics.
*/
class VoodooKillaAudioProcessor : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    enum class Slot { A, B };

    struct SlotState
    {
        bool valid = false;
        int libraryIndex = -1;     // -1 = not in library (mutated)
        vk::PresetInfo info;
    };

    VoodooKillaAudioProcessor();
    ~VoodooKillaAudioProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- model API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    vk::PresetLibrary library;

    const SlotState& getSlotA() const noexcept { return slotA; }
    const SlotState& getSlotB() const noexcept { return slotB; }

    /** Card click: slot A clears B (clean preset, morph off). Slot B keeps A. */
    void loadPreset (int libraryIndex, Slot slot, bool recordUndo = true);
    void clearSlotB (bool recordUndo = true);
    /** Makes sure a B slot exists (picks the neighbouring card) so MORPH always does something. */
    bool ensureSlotB();
    void stepPreset (int delta);
    /** KILL: random preset (respecting the category lock). Mutate: +-15 % of the current A. */
    void kill (bool mutate, uint32_t seed = 0);

    int getCurrentCategory() const noexcept { return currentCategory; }
    void setCurrentCategory (int category);
    bool getCategoryLock() const noexcept { return categoryLock; }
    void setCategoryLock (bool locked) { categoryLock = locked; }

    Slot getClickSlot() const noexcept { return clickSlot; }
    void setClickSlot (Slot s) { clickSlot = s; ++selectionVersion; }

    void setHoldFromUI (bool down) noexcept { uiHold.store (down); }

    float getUiScale() const noexcept { return uiScale; }
    void setUiScale (float s) { uiScale = s; }

    uint32_t getSelectionVersion() const noexcept { return selectionVersion; }
    vk::EngineMeters& getMeters() noexcept { return engine.getMeters(); }
    bool isEffectActive() noexcept { return engine.getMeters().effectActive.load (std::memory_order_relaxed); }

    // ---- favourites (global, stored next to user presets)
    bool isFavourite (const juce::String& presetId) const { return favourites.contains (presetId); }
    void toggleFavourite (const juce::String& presetId);

    // ---- undo / redo (whole state snapshots)
    void pushUndoSnapshot();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return undoIndex > 0; }
    bool canRedo() const noexcept { return undoIndex + 1 < (int) undoStack.size(); }

    // ---- user presets
    int saveUserPreset (const juce::String& name);

    // ---- helpers for the editor
    juce::String getDisplayName() const;
    static const juce::StringArray& speedChoices();
    static float speedMultiplier (int index) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct SlotCommand
    {
        int slot = 0;           // 0 = A, 1 = B
        bool clear = false;
        bool resetVelocity = true;
        vk::PresetData data;
    };

    void pushSlotCommand (const SlotCommand& c);
    void sendSlotsToAudio();
    void setParamValue (const char* id, float plainValue);
    void applyPresetDefaults (const vk::PresetInfo& p);
    void handleMidi (const juce::MidiMessage& m) noexcept;
    void timerCallback() override;

    juce::ValueTree createStateTree();
    void applyStateTree (const juce::ValueTree& tree);
    void restoreSlotFromTree (const juce::ValueTree& t, const char* idKey, const char* dataKey, SlotState& slot);

    void loadFavourites();
    void saveFavourites();

    // engine / audio side
    vk::Engine engine;
    juce::AbstractFifo commandFifo { 64 };
    std::array<SlotCommand, 64> commands;
    juce::SpinLock producerLock;

    std::atomic<int> audioCategory { 0 };
    std::atomic<int> midiCardEvent { -1 };
    std::atomic<int> midiCategoryEvent { -1 };
    std::atomic<bool> uiHold { false };
    int heldCardNotes = 0;

    // parameter pointers (audio thread reads)
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pTrigger = nullptr;
    std::array<std::atomic<float>*, vk::kNumSteps> pSteps {};

    // message-thread model
    SlotState slotA, slotB;
    int currentCategory = 0;
    bool categoryLock = false;
    Slot clickSlot = Slot::A;
    float uiScale = 0.0f;   // 0 = choose automatically on first open
    uint32_t selectionVersion = 1;
    bool lastMorphSideB = false;
    juce::StringArray favourites;

    std::vector<juce::ValueTree> undoStack;
    int undoIndex = -1;
    bool restoringUndo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoodooKillaAudioProcessor)
};
