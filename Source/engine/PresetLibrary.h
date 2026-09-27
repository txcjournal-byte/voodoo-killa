#pragma once

#include "Preset.h"
#include <vector>

namespace vk
{
/**
    Factory presets (read-only, loaded once from BinaryData) + user presets (JSON files in
    Documents/Killa/Voodoo Killa/Presets).

    The factory vector never changes after construction, so the audio thread may read
    getFactory()[i].data directly (used for MIDI card selection).
    User presets are only touched on the message thread.
*/
class PresetLibrary
{
public:
    PresetLibrary();

    bool loadFactoryJson (const juce::String& json);

    const std::vector<CategoryInfo>& getCategories() const noexcept { return categories; }
    const std::vector<PresetInfo>& getFactory() const noexcept { return factory; }
    const std::vector<PresetInfo>& getUser() const noexcept { return user; }

    /** Combined index space: [0, factory) = factory, then user presets. */
    int size() const noexcept { return (int) (factory.size() + user.size()); }
    const PresetInfo* get (int index) const noexcept;
    int findById (const juce::String& id) const;
    int factoryIndex (int category, int card) const noexcept { return category * kPresetsPerCategory + card; }
    int categoryIndexOf (const juce::String& categoryId) const;

    /** Deterministic random pick. category < 0 = any factory preset. `exclude` is never returned (if possible). */
    int randomIndex (uint32_t seed, int category, int exclude) const noexcept;

    // ---- user presets (message thread)
    static juce::File getUserPresetFolder();
    void scanUserPresets();
    /** Saves a user preset (optionally with B + morph) and returns its library index, or -1. */
    int saveUserPreset (const juce::String& name, const PresetInfo& a, const PresetInfo* b, float morph);
    bool deleteUserPreset (int index);

    /** Extra data stored with a user preset (A+B pair). */
    struct UserExtra { juce::String bId; bool hasB = false; PresetInfo b; float morph = 0.0f; juce::File file; };
    const UserExtra* getUserExtra (int index) const noexcept;

private:
    std::vector<CategoryInfo> categories;
    std::vector<PresetInfo> factory;
    std::vector<PresetInfo> user;
    std::vector<UserExtra> userExtra;
};
} // namespace vk
