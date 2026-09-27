#!/usr/bin/env python3
"""Writes Resources/Photos/prompts.md: one image prompt per factory preset (CLAUDE.md section 9)."""
import json
import os

BASE = ("Black and white flash photograph at night, heavy film grain, underground trap mixtape aesthetic, "
        "harsh flash, deep shadows, square 1:1, no text, no people's faces. Subject: ")

MOTIFS = {
    "halfcut_1": "a lone wooden cross in tall wet grass, leaning slightly",
    "halfcut_2": "an empty street at night, single streetlight in fog, wet asphalt",
    "halfcut_3": "a concrete highway overpass from below, pillars fading into mist",
    "halfcut_4": "a stack of old CRT televisions showing static in a dark room",
    "halfcut_5": "an unmade bed with crumpled sheets in a dark bedroom, curtains half open",
    "halfcut_6": "a chain-link fence at night with blurred lights behind it, rain",
    "halfcut_7": "a cassette deck with the reels exposed, dust and scratches, close-up",
    "halfcut_8": "a narrow concrete staircase leading down into darkness, one bare bulb",
    "deadtape_1": "a hospital heart monitor on a dark stand, flat line glow, empty room",
    "deadtape_2": "a tripped fuse box with burned wires in a basement",
    "deadtape_3": "breath vapour hanging over a frozen parking lot at night",
    "deadtape_4": "a car's brake light and tire skid marks on wet asphalt",
    "deadtape_5": "a tangled cassette tape unspooled across a car dashboard",
    "deadtape_6": "an abandoned vinyl turntable with the needle lifted, dust in the flash",
    "deadtape_7": "a dying flashlight on a wet floor, weak beam, puddles",
    "deadtape_8": "an unplugged power cord lying on a concrete floor, sparks of dust",
    "glitchritval_1": "a ring of melted candles on a concrete floor, smoke trails",
    "glitchritval_2": "a cracked phone screen glowing on a mattress, shattered glass pattern",
    "glitchritval_3": "a rosary hanging from a rear-view mirror, motion blur",
    "glitchritval_4": "empty brass shell casings scattered on asphalt",
    "glitchritval_5": "three identical doors in a dark hallway, one slightly open",
    "glitchritval_6": "a spiral staircase seen from above, dizzying perspective",
    "glitchritval_7": "a broken church window with shards on the floor",
    "glitchritval_8": "a server rack with blinking lights in a dark room, cables hanging",
    "chopped_1": "a butcher's cleaver stuck in a wooden block",
    "chopped_2": "a meat hook hanging from a ceiling chain in a cold room",
    "chopped_3": "a smashed guitar amp on a club stage floor, cables everywhere",
    "chopped_4": "a hooded jacket hanging on a chain-link fence, alley at night",
    "chopped_5": "a trampoline in an empty backyard at night, frozen motion",
    "chopped_6": "a triangle of three streetlights over an empty intersection",
    "chopped_7": "a kitchen knife on a dark table, reflection of the flash on the blade",
    "chopped_8": "a chainsaw resting on a tree stump in a dark forest",
    "backmask_1": "a vinyl record spinning, reversed label, motion blur",
    "backmask_2": "an old reel-to-reel tape recorder in an attic",
    "backmask_3": "an upside-down cross carved into a wooden door",
    "backmask_4": "water swirling down a drain, long exposure",
    "backmask_5": "a dreamy bedroom with fairy lights out of focus, hazy",
    "backmask_6": "a cracked mirror in an abandoned bathroom",
    "backmask_7": "an empty tunnel with an echoing light at the end",
    "backmask_8": "two crossed wooden beams inverted in a field",
    "diabolvs_1": "goat horns resting on a dark wooden table",
    "diabolvs_2": "a boiler room glowing red-black, pipes and steam",
    "diabolvs_3": "a feather falling in a dark hallway, lit by the flash",
    "diabolvs_4": "a church bell tower against a black sky",
    "diabolvs_5": "a staircase with a single fallen white feather at the bottom",
    "diabolvs_6": "a gas mask lying on wet concrete",
    "diabolvs_7": "a carnival stuffed animal abandoned on a sidewalk",
    "diabolvs_8": "a glazed donut box and a styrofoam cup on a car hood at night",
    "cassettecvlt_1": "a pile of cassette tapes on a carpet, some cases broken",
    "cassettecvlt_2": "a basement with a single mattress and a hanging bulb",
    "cassettecvlt_3": "fog over a river bridge at night, distant lights",
    "cassettecvlt_4": "a warped melted vinyl record on a radiator",
    "cassettecvlt_5": "an old handheld game console on a bedsheet",
    "cassettecvlt_6": "a payphone booth on an empty street corner",
    "cassettecvlt_7": "a car radio antenna against the night sky, power lines",
    "cassettecvlt_8": "a boombox on a stoop, worn stickers, no text",
    "villainera_1": "a black hoodie hanging in an empty doorway",
    "villainera_2": "an empty church with rows of pews, candles far away",
    "villainera_3": "a silhouette of bare trees against a dark cloudy sky",
    "villainera_4": "long shadows of lamp posts across an empty courtyard",
    "villainera_5": "a hollow tree trunk in a dark forest",
    "villainera_6": "a graveyard with tilted headstones in fog",
    "villainera_7": "a single black candle burning on a stone ledge",
    "villainera_8": "an empty void room with one open door and darkness behind it",
    "aura_1": "a large empty parking garage with flash reflections on the floor",
    "aura_2": "a field of tall grass at night under a single light",
    "aura_3": "light beams through a gate in fog",
    "aura_4": "icicles hanging from a gutter at night",
    "aura_5": "an empty stage with one microphone stand",
    "aura_6": "an empty swimming pool at night, drain in the centre",
    "aura_7": "a gothic cathedral interior, high arches, darkness",
    "aura_8": "a long empty corridor with doors on both sides",
    "toxic_1": "a leaking barrel with liquid spilling on concrete",
    "toxic_2": "slime dripping from a rusty pipe",
    "toxic_3": "a hazard sign-shaped fence with no text, factory at night",
    "toxic_4": "a gas mask hanging on a nail",
    "toxic_5": "an old rotary telephone off the hook on a table",
    "toxic_6": "a medicine cabinet with pill bottles, no labels",
    "toxic_7": "a spider web between two railings, lit by the flash",
    "toxic_8": "a bouncing rubber ball frozen mid-air in an alley",
    "crashout_1": "a car wreck on the roadside at night, crushed hood",
    "crashout_2": "a burning oil drum in an empty lot",
    "crashout_3": "a blown speaker cone torn apart, close-up",
    "crashout_4": "molten metal dripping from a steel beam, sparks",
    "crashout_5": "an energy drink can crushed on a keyboard, cables",
    "crashout_6": "a pair of pliers cutting a wire, close-up",
    "crashout_7": "a screaming mouth shape made by torn metal, abstract",
    "crashout_8": "a burning house far away across a field",
    "lockin_1": "a padlock on a rusty chain across a gate",
    "lockin_2": "an emergency stop button on a dirty machine",
    "lockin_3": "an iron crypt door in a cemetery wall",
    "lockin_4": "a staircase going up into bright light from darkness",
    "lockin_5": "a severed microphone cable on a stage floor",
    "lockin_6": "a subwoofer speaker box in the trunk of a car",
    "lockin_7": "a spinning record under a turntable arm, motion blur",
    "lockin_8": "a cap lying on the edge of a rooftop at night",
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data = json.load(open(os.path.join(root, "Resources", "Presets", "factory.json"), encoding="utf-8"))
    cats = {c["id"]: c for c in data["categories"]}
    lines = [
        "# Voodoo Killa – polaroid photo prompts",
        "",
        "96 prompts, one per factory preset. Generate 512 × 512 px, black and white, then save as",
        "`Resources/Photos/<file>` (file name in each heading). Missing photos fall back to `placeholder.jpg`.",
        "",
        "Shared style base:",
        "",
        "> " + BASE + "<motif>",
        "",
    ]
    current = None
    for p in data["presets"]:
        if p["category"] != current:
            current = p["category"]
            c = cats[current]
            lines += [f"## {c['name']} – {c['desc']}", ""]
        motif = MOTIFS[p["id"]]
        lines += [f"### `{p['photo']}` – {p['name']} ({p['desc']})", "", "```", BASE + motif, "```", ""]
    out = os.path.join(root, "Resources", "Photos", "prompts.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"wrote {len(data['presets'])} prompts -> {out}")


if __name__ == "__main__":
    main()
