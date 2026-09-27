#include "PresetLibrary.h"
#include "DspUtil.h"
#include <BinaryData.h>

namespace vk
{
PresetLibrary::PresetLibrary()
{
    int size = 0;
    if (const char* data = BinaryData::getNamedResource ("factory_json", size))
        loadFactoryJson (juce::String::fromUTF8 (data, size));
}

bool PresetLibrary::loadFactoryJson (const juce::String& json)
{
    categories.clear();
    factory.clear();

    const auto root = juce::JSON::parse (json);
    if (! root.isObject())
        return false;

    if (auto* cats = root["categories"].getArray())
    {
        for (auto& c : *cats)
        {
            CategoryInfo info;
            info.id = c["id"].toString();
            info.name = c["name"].toString();
            info.desc = c["desc"].toString();
            info.colour = c["colour"].toString();
            categories.push_back (info);
        }
    }

    if (auto* presets = root["presets"].getArray())
    {
        std::vector<int> counters (categories.size(), 0);
        for (auto& v : *presets)
        {
            auto p = presetInfoFromVar (v);
            p.factory = true;
            p.categoryIndex = categoryIndexOf (p.category);
            if (p.categoryIndex >= 0)
                p.indexInCategory = counters[(size_t) p.categoryIndex]++;
            factory.push_back (p);
        }
    }

    // keep the canonical order: category-major, 8 per category
    std::stable_sort (factory.begin(), factory.end(), [] (const PresetInfo& a, const PresetInfo& b)
    {
        return a.categoryIndex != b.categoryIndex ? a.categoryIndex < b.categoryIndex
                                                  : a.indexInCategory < b.indexInCategory;
    });
    return ! factory.empty();
}

const PresetInfo* PresetLibrary::get (int index) const noexcept
{
    if (index < 0)
        return nullptr;
    if (index < (int) factory.size())
        return &factory[(size_t) index];
    index -= (int) factory.size();
    if (index < (int) user.size())
        return &user[(size_t) index];
    return nullptr;
}

int PresetLibrary::findById (const juce::String& id) const
{
    for (int i = 0; i < size(); ++i)
        if (get (i)->id == id)
            return i;
    return -1;
}

int PresetLibrary::categoryIndexOf (const juce::String& categoryId) const
{
    for (size_t i = 0; i < categories.size(); ++i)
        if (categories[i].id == categoryId)
            return (int) i;
    return -1;
}

int PresetLibrary::randomIndex (uint32_t seed, int category, int exclude) const noexcept
{
    const int first = category >= 0 ? category * kPresetsPerCategory : 0;
    const int count = category >= 0 ? std::min (kPresetsPerCategory, (int) factory.size() - first) : (int) factory.size();
    if (count <= 0)
        return -1;

    Rng rng (hash32 (seed));
    int pick = first + rng.nextInt (count);
    if (pick == exclude && count > 1)
        pick = first + ((pick - first + 1 + rng.nextInt (count - 1)) % count);
    return pick;
}

// ---------------------------------------------------------------------------
juce::File PresetLibrary::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Killa").getChildFile ("Voodoo Killa").getChildFile ("Presets");
}

void PresetLibrary::scanUserPresets()
{
    user.clear();
    userExtra.clear();

    auto folder = getUserPresetFolder();
    if (! folder.isDirectory())
        return;

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        const auto v = juce::JSON::parse (f.loadFileAsString());
        if (! v.isObject())
            continue;

        const auto a = v.hasProperty ("a") ? v["a"] : v;
        auto info = presetInfoFromVar (a);
        info.factory = false;
        info.name = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
        info.id = "user:" + f.getFileName();
        info.categoryIndex = categoryIndexOf (info.category);

        UserExtra extra;
        extra.file = f;
        if (v["b"].isObject())
        {
            extra.hasB = true;
            extra.b = presetInfoFromVar (v["b"]);
            extra.bId = extra.b.id;
            extra.morph = (float) (double) v.getProperty ("morph", 0.0);
        }
        user.push_back (info);
        userExtra.push_back (extra);
    }
}

int PresetLibrary::saveUserPreset (const juce::String& name, const PresetInfo& a, const PresetInfo* b, float morph)
{
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return -1;

    const auto safe = juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("User Preset") : name.trim());
    auto file = folder.getChildFile (safe + ".json");

    auto root = new juce::DynamicObject();
    root->setProperty ("product", "Voodoo Killa");
    root->setProperty ("version", 1);
    root->setProperty ("name", name.trim());
    root->setProperty ("a", presetInfoToVar (a));
    if (b != nullptr)
    {
        root->setProperty ("b", presetInfoToVar (*b));
        root->setProperty ("morph", morph);
    }

    if (! file.replaceWithText (juce::JSON::toString (juce::var (root))))
        return -1;

    scanUserPresets();
    return findById ("user:" + file.getFileName());
}

bool PresetLibrary::deleteUserPreset (int index)
{
    const int u = index - (int) factory.size();
    if (u < 0 || u >= (int) userExtra.size())
        return false;
    const bool ok = userExtra[(size_t) u].file.deleteFile();
    scanUserPresets();
    return ok;
}

const PresetLibrary::UserExtra* PresetLibrary::getUserExtra (int index) const noexcept
{
    const int u = index - (int) factory.size();
    if (u < 0 || u >= (int) userExtra.size())
        return nullptr;
    return &userExtra[(size_t) u];
}
} // namespace vk
