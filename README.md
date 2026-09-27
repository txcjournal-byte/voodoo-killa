# Voodoo Killa

*Cast a preset. Kill the melody.* – melodický time/FX plugin ze série Killa (VST3 + Standalone, JUCE 8).

## Build (Windows, Visual Studio 2022)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

JUCE 8.0.15 se stáhne automaticky (`FetchContent`). Výstupy:

- `build\VoodooKilla_artefacts\Release\VST3\Voodoo Killa.vst3`
- `build\VoodooKilla_artefacts\Release\Standalone\Voodoo Killa.exe`

Instalace: zkopíruj celou složku `Voodoo Killa.vst3` do `C:\Program Files\Common Files\VST3`.

Testy: `build\VoodooKillaTests_artefacts\Release\VoodooKillaTests.exe` (unit testy, offline render všech
96 presetů, CPU benchmark). Na macOS/Linuxu stejně s Ninja/Xcode; na macOS se navíc staví AU.

## MIDI

| Nota (JUCE/Ableton: C3 = 60) | Nota ve FL Studiu (C5 = 60) | Funkce |
|---|---|---|
| C2 – B2 (48–59) | C4 – B4 | výběr kategorie 1–12 |
| C3 – G3 (60–67) | C5 – G5 | karty 1–8 aktuální kategorie (velocity = AMOUNT, držení = Hold) |
| pod C2 (< 48) | pod C4 | spouští 808 DUCK (když není sidechain) |

## Ovládání

- **Klik na kartu** = preset A (B se zruší). **Shift/pravý klik** = preset B → Morph A/B.
- **A|B** v liště = kam jde další obyčejný klik; pravý klik na A|B = zrušit B.
- **KILL** = náhodný preset (🔒 = jen aktuální kategorie), **pravý klik** = Mutate (±15 %). 🎲 v liště = náhodná dvojice A/B.
- **KILL PAD**: X = MORPH (s B) nebo AMOUNT (jen A), Y = TONE, dvojklik = střed.
- Knoby: tah nahoru/dolů, Ctrl = jemně, dvojklik = výchozí, kolečko.
- ⚙ = velikost okna 100/125/150 %, složka uživatelských presetů
  (`Documents/Killa/Voodoo Killa/Presets`).

## Grafika (výměna bez změny kódu)

| Soubor | Použití |
|---|---|
| `Resources/Logo/logo.png`, `logo@2x.png` | logo |
| `Resources/Textures/background.png`, `paper.png` | dlaždice pozadí / papíru |
| `Resources/Textures/tape_1.png` … `tape_3.png` | kousky pásky |
| `Resources/Textures/pin.png` | červený špendlík |
| `Resources/Photos/<kategorie>_<číslo>.jpg` | 96 fotek (prompty v `Resources/Photos/prompts.md`) |

Chybějící soubor = automaticky placeholder. Po přidání souborů znovu spusť `cmake` (glob) a build.
Presety se generují z `Tools/gen_factory.py` → `Resources/Presets/factory.json`.

## Test ve FL Studiu

1. Options → Manage plugins → přidej `C:\Program Files\Common Files\VST3` → *Find more plugins*.
2. Dej **Voodoo Killa** do mixer insertu s melodií (sample/keys), pusť pattern ve smyčce (např. 140 BPM).
3. Klikni na **HALFCUT → Halfcut**: melodie hraje v půlce tempa a na každém taktu se vrací do synchronu.
4. **DEAD TAPE → Flatline** (Every 4): tape stop jen v posledním taktu každé 4taktové fráze.
5. **GLITCH RITVAL → Spiral** (Last Beat): zrychlující roll na poslední době taktu.
6. Klik na *Halfcut*, Shift+klik na *Slow Grave* → táhni tečkou v KILL PADu zleva doprava:
   plynulý přechod, bez lupanců; v polovině se přepne trigger.
7. Trigger **Hold** → drž myš na kartě: efekt běží jen po dobu držení.
8. Piano roll na kanálu posílajícím MIDI do pluginu (Channel settings → MIDI out / *Patcher*):
   C4 = kategorie 1, C5–G5 = karty; slabší velocity = menší AMOUNT.
9. Změna tempa, skok v playlistu, loop a stop: efekt se znovu srovná (bez výpadků a lupanců).
10. Ulož projekt, zavři a otevři: vrátí se A, B, morph, makra, trigger, steps i velikost okna.
11. **LOCK IN → Mute for 808** + 808 DUCK: pošli do sidechain vstupu pluginu 808 (FL: route kanál
    808 do insertu s Voodoo Killa jako sidechain) nebo hraj MIDI noty pod C4.

## Licence třetích stran

Fonty *Special Elite* a *Permanent Marker* (Google Fonts) jsou pod licencí Apache 2.0,
viz `Resources/Fonts/LICENSE-Apache-2.0.txt`.
