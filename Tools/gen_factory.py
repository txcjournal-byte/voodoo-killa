#!/usr/bin/env python3
"""
Generates Resources/Presets/factory.json from the preset table (CLAUDE.md, section 10).

Usage:  python3 Tools/gen_factory.py
Each entry: (name, description, trigger, length_beats, modules)
Module keys follow the JSON format read by Source/engine/Preset.cpp.
"""
import json
import os

CATEGORIES = [
    ("halfcut",      "HALFCUT",       "Halftime",            "#8A2BE2"),
    ("deadtape",     "DEAD TAPE",     "Tape Stop",           "#B22222"),
    ("glitchritval", "GLITCH RITVAL", "Stutter / Repeat",    "#00E5FF"),
    ("chopped",      "CHOPPED",       "Chop / Gate",         "#FF6A00"),
    ("backmask",     "BACKMASK",      "Reverse",             "#9400D3"),
    ("diabolvs",     "DIABOLVS",      "Pitch",               "#DC143C"),
    ("cassettecvlt", "CASSETTE CVLT", "Lo-Fi",               "#C8A165"),
    ("villainera",   "VILLAIN ERA",   "Dark",                "#4B0082"),
    ("aura",         "AURA",          "Space / Reverb",      "#40E0D0"),
    ("toxic",        "TOXIC",         "Filter / Sweep",      "#7CFC00"),
    ("crashout",     "CRASH OUT",     "Rage / Distortion",   "#FF0000"),
    ("lockin",       "LOCK IN",       "Transitions / Drops", "#FFD700"),
]

A, H, E4, E8, LB, ST = "always", "hold", "every4", "every8", "lastbeat", "steps"

def time(mode, **kw):  return {"time": dict(mode=mode, **kw)}
def pitch(**kw):       return {"pitch": kw}
def gate(pattern, **kw): return {"gate": dict(pattern=pattern, **kw)}
def filt(type_, cutoff, **kw): return {"filter": dict(type=type_, cutoff=cutoff, **kw)}
def band(lo, hi, **kw): return {"filter": dict(type="band", cutoff=lo, cutoff2=hi, **kw)}
def lofi(**kw):        return {"lofi": kw}
def drive(type_, amount, **kw): return {"drive": dict(type=type_, amount=amount, **kw)}
def space(**kw):       return {"space": kw}
def reverb(size, mix, **kw): return space(reverbSize=size, reverbMix=mix, **kw)
def delay(t, fb, mix=0.35, ping=False): return space(delayTime=t, delayFb=fb, delayMix=mix, pingPong=ping)
def width(w):          return {"width": w}
def duck(amount, release): return {"duck": {"amount": amount, "release": release}}

def m(*parts):
    out = {}
    for p in parts:
        for k, v in p.items():
            if k in out and isinstance(v, dict):
                out[k].update(v)
            else:
                out[k] = v
    return out

PRESETS = {
"halfcut": [
    ("Halfcut",       "halftime",          A,  4, m(time("half"))),
    ("Slow Grave",    "halftime + dark",   A,  4, m(time("half"), filt("lp", 1800))),
    ("Quarter Death", "quarter time",      E4, 4, m(time("quarter"))),
    ("NPC Mode",      "robotic lofi",      A,  4, m(time("half"), lofi(bits=8, srate=16000))),
    ("Sleepwalk",     "halftime + wobble", A,  4, m(time("half", pitchFollow=True), pitch(wobble=25, wobbleHz=0.5))),
    ("Slow Bleed",    "reverb tail",       A,  4, m(time("half"), reverb(0.8, 0.35))),
    ("Cooked",        "tape drive",        E4, 4, m(time("half"), drive("tape", 0.5), lofi(wow=0.3))),
    ("Drag Me Down",  "octave down",       LB, 1, m(time("half"), pitch(semis=-12))),
],
"deadtape": [
    ("Flatline",      "stop 1 bar",          E4, 4,   m(time("stop", curve=0.7))),
    ("Power Cut",     "stop 1/4",            LB, 1,   m(time("stop", curve=0.9))),
    ("Last Breath",   "slow stop + reverb",  E8, 2,   m(time("stop"), reverb(0.9, 0.4))),
    ("Brake Check",   "stop then start",     LB, 1,   m(time("stopstart"))),
    ("Rewind Demon",  "tape rewind",         E4, 1,   m(time("rewind", pitchFollow=True))),
    ("It's Over",     "stop + fade + lofi",  E8, 4,   m(time("stop"), lofi(noise=0.3))),
    ("Dead Battery",  "stop + gate stutter", E4, 2,   m(time("stop"), gate("x-x-x-x--x--x---"))),
    ("Unplugged",     "vinyl power-down",    LB, 0.5, m(time("stop", curve=1.0), lofi(noise=0.2))),
],
"glitchritval": [
    ("Ritval",        "repeat 1/8",             LB, 1,   m(time("repeat", rate="1/8"))),
    ("Brainrot",      "random scatter",         E4, 2,   m(time("scatter", rate="1/16"))),
    ("Possessed",     "repeat 1/16 + pitch up", LB, 1,   m(time("repeat", rate="1/16"), pitch(ramp=12))),
    ("Machine Gun",   "repeat 1/32",            LB, 0.5, m(time("repeat", rate="1/32"))),
    ("Triplet Curse", "triplet repeat",         E4, 1,   m(time("repeat", rate="1/8T"))),
    ("Spiral",        "accelerating roll",      LB, 1,   m(time("repeat", rate="1/8", ramp=1.0))),
    ("Broken Prayer", "repeat + reverse",       E4, 1,   m(time("repeat", rate="1/8", alternate=True))),
    ("Glitch Mass",   "scatter + bitcrush",     E8, 4,   m(time("scatter", rate="1/32"), lofi(bits=6))),
],
"chopped": [
    ("Chopped",       "1/16 gate",       A,  4, m(gate("x-x-x-x-x-x-x-x-"))),
    ("Cleaver",       "hard 1/8 chop",   A,  4, m(gate("xx--xx--xx--xx--", smooth=1))),
    ("Rage Chop",     "rage gate + drive", A, 4, m(gate("xxx-xx-xxx-x-xx-"), drive("clip", 0.4))),
    ("Drill Skip",    "drill pattern",   A,  4, m(gate("x--x--x---x--x--"))),
    ("Bounce Hex",    "plugg bounce",    A,  4, m(gate("x-xx-x-xx-x-xx-x", depth=0.8))),
    ("Triplet Slice", "triplet gate",    A,  4, m(gate("x-x", rate="1/16T"))),
    ("Offbeat Knife", "offbeat gate",    A,  4, m(gate("-x-x-x-x-x-x-x-x"))),
    ("Chainsaw",      "1/32 gate + fuzz", LB, 1, m(gate("x-", rate="1/32"), drive("fuzz", 0.5))),
],
"backmask": [
    ("Backmask",       "reverse 1/4",            LB, 1, m(time("reverse"))),
    ("Hidden Message", "reverse 1 bar",          E4, 4, m(time("reverse"))),
    ("Satan Spin",     "reverse + octave down",  LB, 1, m(time("reverse"), pitch(semis=-12))),
    ("Reverse Swell",  "reverse + reverb swell", E4, 2, m(time("reverse"), reverb(0.9, 0.5))),
    ("Delulu",         "reverse + dreamy shimmer", E4, 1, m(time("reverse"), space(reverbSize=0.8, reverbMix=0.2, shimmer=0.4))),
    ("Mirror",         "alternating fwd/rev 1/8", A, 4, m(time("reverse", rate="1/8", alternate=True))),
    ("Rewind Soul",    "reverse echo",           E4, 1, m(time("reverse"), delay("1/8", 0.5))),
    ("Inverse Cross",  "reverse + halftime",     E8, 2, m(time("reverse", speed=0.5))),
],
"diabolvs": [
    ("Diabolvs",        "tritone down",        A,  4, m(pitch(semis=-6))),
    ("Lucifer Low",     "octave down",         A,  4, m(pitch(semis=-12))),
    ("Angel High",      "octave up + air",     A,  4, m(pitch(semis=12), filt("hp", 300))),
    ("Tritone",         "tritone wobble",      A,  4, m(pitch(semis=-6, wobble=30, wobbleHz=2.0))),
    ("Fall From Grace", "end-bar pitch drop",  LB, 1, m(pitch(endDrop=-12))),
    ("Demon Voice",     "down + formant down", A,  4, m(pitch(semis=-7, formant=-6), drive("tape", 0.3))),
    ("Chipmunk Cult",   "up + formant up",     A,  4, m(pitch(semis=7, formant=5))),
    ("Glazed",          "detune wide chorus",  A,  4, m(pitch(detune=12), width(1.7))),
],
"cassettecvlt": [
    ("Cvlt Tape",    "cassette",         A, 4, m(lofi(wow=0.3, flutter=0.2, noise=0.15, tone=9000))),
    ("Basement",     "muffled room",     A, 4, m(filt("lp", 1200), reverb(0.4, 0.2))),
    ("Memphis Mist", "dark tape + crush", A, 4, m(lofi(bits=10, srate=22000, wow=0.2), filt("lp", 4000))),
    ("Warped",       "heavy wow",        A, 4, m(lofi(wow=0.8))),
    ("8-Bit Soul",   "8-bit",            A, 4, m(lofi(bits=6, srate=11000))),
    ("Payphone",     "telephone",        A, 4, m(band(400, 3000), drive("clip", 0.3))),
    ("Pirate Radio", "radio + noise",    A, 4, m(band(600, 4000), lofi(noise=0.4))),
    ("Mid",          "subtle lofi",      A, 4, m(lofi(bits=12, wow=0.1, noise=0.05))),
],
"villainera": [
    ("Villain Era",     "dark filter + verb",   A,  4, m(filt("lp", 1500), reverb(0.6, 0.25))),
    ("Midnight Mass",   "dark + choir shimmer", A,  4, m(filt("lp", 2500), space(reverbSize=0.85, reverbMix=0.15, shimmer=0.3))),
    ("Nox",             "pitch down + dark",    A,  4, m(pitch(semis=-5), filt("lp", 2000))),
    ("Shadow Walk",     "dark + ping-pong",     A,  4, m(filt("lp", 2000), delay("1/8", 0.45, ping=True))),
    ("Hollow",          "band-pass hollow",     A,  4, m(filt("bp", 800, reso=0.6))),
    ("Graveyard Shift", "dark halftime",        E4, 4, m(time("half"), filt("lp", 1200))),
    ("Black Candle",    "dark + tape drive",    A,  4, m(filt("lp", 1800), drive("tape", 0.5))),
    ("Void",            "freeze pad",           H,  4, m(space(reverbSize=0.9, reverbMix=0.5, freeze=True), filt("lp", 3000))),
],
"aura": [
    ("Aura",            "big reverb",          A,  4, m(reverb(0.8, 0.35))),
    ("Aura Farming",    "shimmer + wide",      A,  4, m(space(reverbSize=0.8, reverbMix=0.35, shimmer=0.5), width(1.8))),
    ("Heaven Gate",     "shimmer + HP",        A,  4, m(space(reverbSize=0.85, reverbMix=0.3, shimmer=0.6), filt("hp", 400))),
    ("Frozen Soul",     "freeze on hold",      H,  4, m(space(reverbSize=0.9, reverbMix=0.5, freeze=True))),
    ("Main Character",  "wide ping-pong",      A,  4, m(delay("1/4D", 0.4, ping=True), width(1.7))),
    ("Underwater Tomb", "LP + chorus + verb",  A,  4, m(filt("lp", 900), pitch(wobble=15, wobbleHz=0.3), reverb(0.7, 0.3))),
    ("Cathedral",       "huge hall",           A,  4, m(reverb(1.0, 0.45))),
    ("Echo Chamber",    "echo throw",          LB, 1, m(delay("1/4", 0.6, mix=0.4))),
],
"toxic": [
    ("Toxic",       "LP sweep in",           E4, 16, m(filt("lp", 300, sweepFrom=300, sweepTo=18000))),
    ("Slime Sweep", "resonant LFO",          A,  4,  m(filt("lp", 1500, reso=0.7, lfoRate="1/4", lfoDepth=0.6))),
    ("Radioactive", "HP build",              E8, 16, m(filt("hp", 20, sweepFrom=20, sweepTo=2000))),
    ("Gas Mask",    "muffled + breath noise", A, 4,  m(filt("lp", 700), lofi(noise=0.2))),
    ("Phone Tap",   "telephone intro",       A,  4,  m(band(500, 2500))),
    ("Pump Poison", "sidechain pump",        A,  4,  m({"gate": dict(pump=True, rate="1/4", depth=0.7)})),
    ("The Ick",     "notch wobble",          A,  4,  m(filt("notch", 1200, reso=0.5, lfoRate="1/8", lfoDepth=0.5))),
    ("Acid Bounce", "resonant 1/16 steps",   A,  4,  m(filt("lp", 1200, reso=0.8, lfoRate="1/16", lfoDepth=0.5, lfoShape="sh"))),
],
"crashout": [
    ("Crash Out",    "rage drive",              A,  4, m(drive("clip", 0.7), filt("hp", 120))),
    ("Hellfire",     "fuzz + octave",           A,  4, m(drive("fuzz", 0.6), pitch(semis=12, blend=0.3))),
    ("Blown Out",    "blown speaker",           A,  4, m(drive("clip", 1.0), band(300, 5000))),
    ("Molten",       "tape saturation + wobble", A, 4, m(drive("tape", 0.8), pitch(wobble=10, wobbleHz=1.5))),
    ("Tweaking",     "bitcrush drive",          A,  4, m(drive("bit", 0.6), lofi(srate=8000))),
    ("Clipper Cult", "hard clipper",            A,  4, m(drive("clip", 0.9))),
    ("Screamer",     "fold distortion + HP",    A,  4, m(drive("fold", 0.7), filt("hp", 400))),
    ("Burn It Down", "distortion + halftime",   E4, 4, m(drive("fuzz", 0.6), time("half"))),
],
"lockin": [
    ("Lock In",       "pre-hook stop",      E8, 1, m(time("stop"))),
    ("Pre-Drop Stop", "2-beat stop + mute", E8, 2, m(time("stop", endMute=0.5))),
    ("Intro Crypt",   "filtered intro",     A,  4, m(filt("lp", 800))),
    ("Rise Up",       "riser pitch + HP",   E8, 4, m(filt("hp", 20, sweepFrom=20, sweepTo=1500), pitch(ramp=7))),
    ("Drop Kill",     "full mute last beat", E4, 1, m(gate("-", depth=1.0))),
    ("Mute for 808",  "strong 808 duck",    A,  4, m(duck(0.9, 120))),
    ("End Bar Spin",  "backspin",           E4, 1, m(time("rewind"))),
    ("No Cap Drop",   "stutter into drop",  E8, 2, m(time("repeat", rate="1/16", ramp=1.0))),
],
}


def build():
    cats, presets = [], []
    for ci, (cid, name, desc, colour) in enumerate(CATEGORIES):
        cats.append({"id": cid, "name": name, "desc": desc, "colour": colour, "midiNote": 48 + ci})
        items = PRESETS[cid]
        assert len(items) == 8, cid
        for i, (pname, pdesc, trig, length, mods) in enumerate(items, start=1):
            p = {
                "id": f"{cid}_{i}",
                "name": pname,
                "desc": pdesc,
                "category": cid,
                "photo": f"{cid}_{i}.jpg",
                "trigger": trig,
                "length": length,
            }
            p.update(mods)
            presets.append(p)
    return {"version": 1, "product": "Voodoo Killa", "categories": cats, "presets": presets}


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "Resources", "Presets", "factory.json")
    data = build()
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {len(data['presets'])} presets in {len(data['categories'])} categories -> {out}")
