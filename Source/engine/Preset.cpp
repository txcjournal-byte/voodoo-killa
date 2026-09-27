#include "Preset.h"

namespace vk
{
namespace
{
    const char* const timeModeNames[]   = { "none", "half", "quarter", "double", "stop", "start", "stopstart", "rewind", "reverse", "repeat", "scatter" };
    const char* const filterTypeNames[] = { "off", "lp", "hp", "bp", "notch", "band" };
    const char* const driveTypeNames[]  = { "off", "tape", "fuzz", "clip", "fold", "bit" };
    const char* const triggerNames[]    = { "always", "hold", "every4", "every8", "lastbeat", "steps" };
    const char* const triggerLabels[]   = { "Always", "Hold", "Every 4", "Every 8", "Last Beat", "Steps" };

    template <typename Enum, size_t N>
    Enum enumFromName (const char* const (&names)[N], const juce::String& s, Enum fallback)
    {
        for (size_t i = 0; i < N; ++i)
            if (s.equalsIgnoreCase (names[i]))
                return (Enum) (int) i;
        return fallback;
    }

    float getFloat (const juce::var& obj, const char* key, float fallback)
    {
        const auto& v = obj[key];
        if (v.isVoid() || v.isUndefined())
            return fallback;
        if (v.isBool())
            return (bool) v ? 1.0f : 0.0f;
        return (float) (double) v;
    }

    bool getBool (const juce::var& obj, const char* key, bool fallback)
    {
        const auto& v = obj[key];
        if (v.isVoid() || v.isUndefined())
            return fallback;
        return (bool) v;
    }

    float getRate (const juce::var& obj, const char* key, float fallback)
    {
        const auto& v = obj[key];
        if (v.isVoid() || v.isUndefined())
            return fallback;
        if (v.isString())
            return parseRate (v.toString(), fallback);
        return (float) (double) v;
    }

    juce::DynamicObject::Ptr obj() { return new juce::DynamicObject(); }
}

// ---------------------------------------------------------------------------
int parseGatePattern (const juce::String& text, GateParams& gate)
{
    int n = 0;
    for (auto c : text)
    {
        if (n >= kMaxGateSteps)
            break;
        if (c == 'x' || c == 'X')
            gate.pattern[(size_t) n++] = 1.0f;
        else if (c == '-' || c == '.' || c == '_')
            gate.pattern[(size_t) n++] = 0.0f;
        else if (c >= '0' && c <= '9')
            gate.pattern[(size_t) n++] = (float) (c - '0') / 9.0f;
    }
    for (int i = n; i < kMaxGateSteps; ++i)
        gate.pattern[(size_t) i] = 1.0f;
    gate.numSteps = n;
    return n;
}

juce::String gatePatternToString (const GateParams& gate)
{
    juce::String s;
    for (int i = 0; i < gate.numSteps; ++i)
    {
        const float v = gate.pattern[(size_t) i];
        if (v >= 0.999f)       s << 'x';
        else if (v <= 0.001f)  s << '-';
        else                   s << juce::String (juce::jlimit (0, 9, juce::roundToInt (v * 9.0f)));
    }
    return s;
}

float parseRate (const juce::String& textIn, float fallback)
{
    auto text = textIn.trim().toUpperCase();
    if (text.isEmpty())
        return fallback;

    float mult = 1.0f;
    if (text.endsWithChar ('T')) { mult = 2.0f / 3.0f; text = text.dropLastCharacters (1); }
    else if (text.endsWithChar ('D')) { mult = 1.5f; text = text.dropLastCharacters (1); }

    if (text.containsChar ('/'))
    {
        const float num = text.upToFirstOccurrenceOf ("/", false, false).getFloatValue();
        const float den = text.fromFirstOccurrenceOf ("/", false, false).getFloatValue();
        if (num <= 0.0f || den <= 0.0f)
            return fallback;
        return 4.0f * num / den * mult;      // 1/4 = 1 beat
    }

    const float v = text.getFloatValue();
    return v > 0.0f ? v * mult : fallback;
}

juce::String rateToString (float beats)
{
    struct R { float b; const char* s; };
    static const R table[] = { { 4.0f, "1/1" }, { 2.0f, "1/2" }, { 1.5f, "1/4D" }, { 1.0f, "1/4" }, { 0.75f, "1/8D" },
                               { 2.0f / 3.0f, "1/4T" }, { 0.5f, "1/8" }, { 1.0f / 3.0f, "1/8T" }, { 0.25f, "1/16" },
                               { 1.0f / 6.0f, "1/16T" }, { 0.125f, "1/32" }, { 1.0f / 12.0f, "1/32T" }, { 0.0625f, "1/64" } };
    for (auto& r : table)
        if (std::abs (r.b - beats) < 1.0e-4f)
            return r.s;
    return juce::String (beats, 4);
}

const char* timeModeName (TimeMode m)     { return timeModeNames[juce::jlimit (0, (int) TimeMode::count - 1, (int) m)]; }
TimeMode timeModeFromName (const juce::String& s) { return enumFromName (timeModeNames, s, TimeMode::none); }
const char* filterTypeName (FilterType t) { return filterTypeNames[juce::jlimit (0, (int) FilterType::count - 1, (int) t)]; }
FilterType filterTypeFromName (const juce::String& s) { return enumFromName (filterTypeNames, s, FilterType::off); }
const char* driveTypeName (DriveType t)   { return driveTypeNames[juce::jlimit (0, (int) DriveType::count - 1, (int) t)]; }
DriveType driveTypeFromName (const juce::String& s) { return enumFromName (driveTypeNames, s, DriveType::off); }
const char* triggerName (TriggerMode t)   { return triggerNames[juce::jlimit (0, (int) TriggerMode::count - 1, (int) t)]; }
const char* triggerLabel (TriggerMode t)  { return triggerLabels[juce::jlimit (0, (int) TriggerMode::count - 1, (int) t)]; }

TriggerMode triggerFromName (const juce::String& s)
{
    auto t = enumFromName (triggerNames, s.removeCharacters (" _-"), TriggerMode::always);
    return t;
}

bool timeModePitchFollowDefault (TimeMode m) noexcept
{
    switch (m)
    {
        case TimeMode::stop:
        case TimeMode::start:
        case TimeMode::stopstart:
        case TimeMode::rewind:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
bool timeActive (const PresetData& p) noexcept   { return p.time.mode != TimeMode::none && p.time.mix > 0.0f; }
bool pitchActive (const PresetData& p) noexcept
{
    const auto& q = p.pitch;
    return q.semis != 0.0f || q.fine != 0.0f || q.endDrop != 0.0f || q.ramp != 0.0f
        || q.wobbleCents != 0.0f || q.formant != 0.0f || q.detuneCents != 0.0f;
}
bool filterActive (const PresetData& p) noexcept { return p.filter.type != FilterType::off && p.filter.mix > 0.0f; }
bool lofiActive (const PresetData& p) noexcept
{
    const auto& l = p.lofi;
    return l.enabled && (l.bits < 15.99f || l.srate < 40000.0f || l.wow > 0.0f || l.flutter > 0.0f || l.noise > 0.0f || l.tone < 19000.0f);
}
bool driveActive (const PresetData& p) noexcept  { return p.drive.type != DriveType::off && p.drive.amount > 0.0f; }
bool spaceActive (const PresetData& p) noexcept
{
    return p.space.reverbMix > 0.0f || p.space.shimmer > 0.0f || p.space.freeze || p.space.delayMix > 0.0f;
}

// ---------------------------------------------------------------------------
juce::var presetDataToVar (const PresetData& p)
{
    auto root = obj();
    root->setProperty ("trigger", triggerName (p.trigger));
    root->setProperty ("length", p.length);

    if (p.time.mode != TimeMode::none)
    {
        auto t = obj();
        t->setProperty ("mode", timeModeName (p.time.mode));
        t->setProperty ("rate", rateToString (p.time.rate));
        t->setProperty ("curve", p.time.curve);
        t->setProperty ("ramp", p.time.ramp);
        t->setProperty ("speed", p.time.speed);
        if (p.time.pitchFollow >= 0) t->setProperty ("pitchFollow", p.time.pitchFollow == 1);
        if (p.time.alternate)        t->setProperty ("alternate", true);
        if (p.time.endMute > 0.0f)   t->setProperty ("endMute", p.time.endMute);
        if (p.time.mix < 1.0f)       t->setProperty ("mix", p.time.mix);
        root->setProperty ("time", t.get());
    }

    if (pitchActive (p))
    {
        auto q = obj();
        const auto& s = p.pitch;
        if (s.semis != 0.0f)       q->setProperty ("semis", s.semis);
        if (s.fine != 0.0f)        q->setProperty ("fine", s.fine);
        if (s.endDrop != 0.0f)     q->setProperty ("endDrop", s.endDrop);
        if (s.ramp != 0.0f)        q->setProperty ("ramp", s.ramp);
        if (s.wobbleCents != 0.0f) { q->setProperty ("wobble", s.wobbleCents); q->setProperty ("wobbleHz", s.wobbleHz); }
        if (s.formant != 0.0f)     q->setProperty ("formant", s.formant);
        if (s.blend != 1.0f)       q->setProperty ("blend", s.blend);
        if (s.detuneCents != 0.0f) q->setProperty ("detune", s.detuneCents);
        root->setProperty ("pitch", q.get());
    }

    if (p.gate.enabled)
    {
        auto g = obj();
        if (p.gate.numSteps > 0) g->setProperty ("pattern", gatePatternToString (p.gate));
        g->setProperty ("rate", rateToString (p.gate.rate));
        g->setProperty ("depth", p.gate.depth);
        g->setProperty ("smooth", p.gate.smoothMs);
        if (p.gate.pump) g->setProperty ("pump", true);
        root->setProperty ("gate", g.get());
    }

    if (p.filter.type != FilterType::off)
    {
        auto f = obj();
        const auto& s = p.filter;
        f->setProperty ("type", filterTypeName (s.type));
        f->setProperty ("cutoff", s.cutoff);
        if (s.type == FilterType::band) f->setProperty ("cutoff2", s.cutoff2);
        f->setProperty ("reso", s.reso);
        if (s.lfoDepth > 0.0f)
        {
            f->setProperty ("lfoRate", rateToString (s.lfoRate));
            f->setProperty ("lfoDepth", s.lfoDepth);
            if (s.lfoShape == LfoShape::sampleHold) f->setProperty ("lfoShape", "sh");
        }
        if (s.sweepTo > 0.0f) { f->setProperty ("sweepFrom", s.sweepFrom); f->setProperty ("sweepTo", s.sweepTo); }
        if (s.mix < 1.0f) f->setProperty ("mix", s.mix);
        root->setProperty ("filter", f.get());
    }

    if (p.lofi.enabled)
    {
        auto l = obj();
        const auto& s = p.lofi;
        l->setProperty ("bits", s.bits);
        l->setProperty ("srate", s.srate);
        l->setProperty ("wow", s.wow);
        l->setProperty ("flutter", s.flutter);
        l->setProperty ("noise", s.noise);
        l->setProperty ("tone", s.tone);
        root->setProperty ("lofi", l.get());
    }

    if (p.drive.type != DriveType::off)
    {
        auto d = obj();
        d->setProperty ("type", driveTypeName (p.drive.type));
        d->setProperty ("amount", p.drive.amount);
        d->setProperty ("postLP", p.drive.postLP);
        root->setProperty ("drive", d.get());
    }

    if (spaceActive (p))
    {
        auto s = obj();
        const auto& q = p.space;
        s->setProperty ("reverbSize", q.reverbSize);
        s->setProperty ("reverbMix", q.reverbMix);
        if (q.shimmer > 0.0f) s->setProperty ("shimmer", q.shimmer);
        if (q.freeze)         s->setProperty ("freeze", true);
        if (q.delayMix > 0.0f)
        {
            s->setProperty ("delayTime", rateToString (q.delayBeats));
            s->setProperty ("delayFb", q.delayFb);
            s->setProperty ("pingPong", q.pingPong);
            s->setProperty ("delayMix", q.delayMix);
        }
        root->setProperty ("space", s.get());
    }

    if (p.width != 1.0f)
        root->setProperty ("width", p.width);

    if (p.duck.amount > 0.0f)
    {
        auto d = obj();
        d->setProperty ("amount", p.duck.amount);
        d->setProperty ("release", p.duck.releaseMs);
        root->setProperty ("duck", d.get());
    }

    return juce::var (root.get());
}

PresetData presetDataFromVar (const juce::var& v)
{
    PresetData p;
    if (v.hasProperty ("trigger")) p.trigger = triggerFromName (v["trigger"].toString());
    p.length = juce::jlimit (0.125f, 64.0f, getFloat (v, "length", p.length));

    if (auto t = v["time"]; t.isObject())
    {
        p.time.mode  = timeModeFromName (t["mode"].toString());
        p.time.rate  = getRate (t, "rate", p.time.rate);
        p.time.curve = getFloat (t, "curve", p.time.curve);
        p.time.ramp  = getFloat (t, "ramp", p.time.ramp);
        p.time.speed = getFloat (t, "speed", p.time.speed);
        if (t.hasProperty ("pitchFollow")) p.time.pitchFollow = getBool (t, "pitchFollow", false) ? 1 : 0;
        p.time.alternate = getBool (t, "alternate", false);
        p.time.endMute = getFloat (t, "endMute", 0.0f);
        p.time.mix = getFloat (t, "mix", 1.0f);
    }

    if (auto q = v["pitch"]; q.isObject())
    {
        auto& s = p.pitch;
        s.semis       = getFloat (q, "semis", 0.0f);
        s.fine        = getFloat (q, "fine", 0.0f);
        s.endDrop     = getFloat (q, "endDrop", 0.0f);
        s.ramp        = getFloat (q, "ramp", 0.0f);
        s.wobbleCents = getFloat (q, "wobble", 0.0f);
        s.wobbleHz    = getFloat (q, "wobbleHz", 0.5f);
        s.formant     = getFloat (q, "formant", 0.0f);
        s.blend       = getFloat (q, "blend", 1.0f);
        s.detuneCents = getFloat (q, "detune", 0.0f);
    }

    if (auto g = v["gate"]; g.isObject())
    {
        p.gate.enabled = true;
        parseGatePattern (g["pattern"].toString(), p.gate);
        p.gate.rate     = getRate (g, "rate", 0.25f);
        p.gate.depth    = getFloat (g, "depth", 1.0f);
        p.gate.smoothMs = getFloat (g, "smooth", 3.0f);
        p.gate.pump     = getBool (g, "pump", false);
    }

    if (auto f = v["filter"]; f.isObject())
    {
        auto& s = p.filter;
        s.type      = filterTypeFromName (f["type"].toString());
        s.cutoff    = getFloat (f, "cutoff", 1000.0f);
        s.cutoff2   = getFloat (f, "cutoff2", 3000.0f);
        s.reso      = getFloat (f, "reso", 0.1f);
        s.lfoRate   = getRate (f, "lfoRate", 1.0f);
        s.lfoDepth  = getFloat (f, "lfoDepth", 0.0f);
        s.lfoShape  = f["lfoShape"].toString().equalsIgnoreCase ("sh") ? LfoShape::sampleHold : LfoShape::sine;
        s.sweepFrom = getFloat (f, "sweepFrom", 0.0f);
        s.sweepTo   = getFloat (f, "sweepTo", 0.0f);
        s.mix       = getFloat (f, "mix", 1.0f);
    }

    if (auto l = v["lofi"]; l.isObject())
    {
        auto& s = p.lofi;
        s.enabled = true;
        s.bits    = juce::jlimit (2.0f, 16.0f, getFloat (l, "bits", 16.0f));
        s.srate   = juce::jlimit (500.0f, 48000.0f, getFloat (l, "srate", 44100.0f));
        s.wow     = getFloat (l, "wow", 0.0f);
        s.flutter = getFloat (l, "flutter", 0.0f);
        s.noise   = getFloat (l, "noise", 0.0f);
        s.tone    = getFloat (l, "tone", 20000.0f);
    }

    if (auto d = v["drive"]; d.isObject())
    {
        p.drive.type   = driveTypeFromName (d["type"].toString());
        p.drive.amount = getFloat (d, "amount", 0.5f);
        p.drive.postLP = getFloat (d, "postLP", 20000.0f);
    }

    if (auto s = v["space"]; s.isObject())
    {
        auto& q = p.space;
        q.reverbSize = getFloat (s, "reverbSize", 0.5f);
        q.reverbMix  = getFloat (s, "reverbMix", 0.0f);
        q.shimmer    = getFloat (s, "shimmer", 0.0f);
        q.freeze     = getBool (s, "freeze", false);
        q.delayBeats = getRate (s, "delayTime", 0.5f);
        q.delayFb    = getFloat (s, "delayFb", 0.0f);
        q.pingPong   = getBool (s, "pingPong", false);
        q.delayMix   = getFloat (s, "delayMix", 0.0f);
    }

    p.width = juce::jlimit (0.0f, 2.0f, getFloat (v, "width", 1.0f));

    if (auto d = v["duck"]; d.isObject())
    {
        p.duck.amount    = getFloat (d, "amount", 0.0f);
        p.duck.releaseMs = getFloat (d, "release", 150.0f);
    }
    return p;
}

juce::var presetInfoToVar (const PresetInfo& info)
{
    auto v = presetDataToVar (info.data);
    auto* o = v.getDynamicObject();
    o->setProperty ("id", info.id);
    o->setProperty ("name", info.name);
    o->setProperty ("desc", info.desc);
    o->setProperty ("category", info.category);
    o->setProperty ("photo", info.photo);
    return v;
}

PresetInfo presetInfoFromVar (const juce::var& v)
{
    PresetInfo info;
    info.id       = v["id"].toString();
    info.name     = v["name"].toString();
    info.desc     = v["desc"].toString();
    info.category = v["category"].toString();
    info.photo    = v["photo"].toString();
    info.data     = presetDataFromVar (v);
    return info;
}
} // namespace vk
