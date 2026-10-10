# TrapWin Timer

Halftime, tape a time FX pro melodie od TrapWin (VST3 + Standalone, JUCE 8).

## Build (Windows, Visual Studio 2022)

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

JUCE 8.0.15 se stáhne automaticky (`FetchContent`). Výstupy:

- `build\VoodooKilla_artefacts\Release\VST3\TrapWin Timer.vst3`
- `build\VoodooKilla_artefacts\Release\Standalone\TrapWin Timer.exe`

Instalace: zkopíruj celou složku `TrapWin Timer.vst3` do `C:\Program Files\Common Files\VST3`.

Testy: `build\VoodooKillaTests_artefacts\Release\VoodooKillaTests.exe` (unit testy, offline render všech
96 presetů, CPU benchmark). Na macOS/Linuxu stejně s Ninja/Xcode; na macOS se navíc staví AU.

## MIDI

| Nota (JUCE/Ableton: C3 = 60) | Nota ve FL Studiu (C5 = 60) | Funkce |
|---|---|---|
| C2 – B2 (48–59) | C4 – B4 | výběr kategorie 1–12 |
| C3 – G3 (60–67) | C5 – G5 | karty 1–8 aktuální kategorie (velocity = AMOUNT, držení = Hold) |
| pod C2 (< 48) | pod C4 | spouští DUCK FOR 808 (když není sidechain) |

## Ovládání

- **Klik na kartu** = preset A (B se zruší). **Shift/pravý klik** = preset B → Morph A/B.
- **A|B** v liště = kam jde další obyčejný klik; pravý klik na A|B = zrušit B.
- **KILL** = náhodný preset (zámek = jen aktuální kategorie), **pravý klik** = Mutate (±15 %).
- **KILL PAD**: X = MORPH (s B) nebo AMOUNT (jen A), Y = TONE (BRIGHT/DARK), dvojklik = střed.
- **HALFTIME ENGINE** (upravuje preset A, u presetu bez halftime ho přidá):
  - SPEED 1.5× / 2× / 3× / 4×
  - LOOP ¼ taktu … 4 takty, T = triola
  - MODE: TAPE = pitch klesá, KEY = zůstane v tónině (oktáva), HOLD = pitch se nemění
  - REV = pozpátku
  - SMOOTH = crossfade na švu smyčky, BRAKE = rozjezd z normální rychlosti do halftime, FADE = postupný nástup
- **WHEN** = kdy efekt běží (Always, Hold, konec 4/8 taktů, poslední doba, Pattern + editor patternu).
- Knoby: tah nahoru/dolů, Ctrl = jemně, dvojklik = výchozí, kolečko.
- ⚙ = velikost okna, složka uživatelských presetů (`Documents/TrapWin/TrapWin Timer/Presets`).

## Grafika (výměna bez změny kódu)

| Soubor | Použití |
|---|---|
| `Resources/Logo/logo.png`, `logo@2x.png` | logo TrapWin |
| `Resources/Photos/<kategorie>_<číslo>.jpg` | 96 fotek (prompty v `Resources/Photos/prompts.md`) |

Chybějící soubor = automaticky placeholder. Po přidání souborů znovu spusť `cmake` (glob) a build.
Presety se generují z `Tools/gen_factory.py` → `Resources/Presets/factory.json`.

## Test ve FL Studiu

1. Options → Manage plugins → přidej `C:\Program Files\Common Files\VST3` → *Find more plugins*.
2. Dej **TrapWin Timer** do mixer insertu s melodií, pusť pattern ve smyčce (např. 140 BPM).
3. **HALFCUT → Halfcut**: melodie hraje v půlce tempa a na konci smyčky se vrací do synchronu.
4. V HALFTIME ENGINE přepínej SPEED, LOOP, MODE a REV: změna je slyšet hned, karta dostane `*`.
5. Klik na *Halfcut*, Shift+klik na *Slow Grave* → táhni tečkou v KILL PADu nebo MORPH fader: plynulý přechod.
6. WHEN → **Hold** → drž myš na kartě: efekt běží jen po dobu držení.
7. Zastav transport: žádné praskání ani dozvuky, vlna i playhead stojí.
8. Ulož projekt, zavři a otevři: vrátí se A, B, morph, makra, WHEN, pattern i velikost okna.
9. Zavři FL Studio s otevřeným pluginem: nesmí zamrznout.

## Licence třetích stran

Fonty *Archivo* a *IBM Plex Mono* jsou pod licencí SIL Open Font License 1.1,
viz `Resources/Fonts/OFL-Archivo.txt` a `Resources/Fonts/OFL-IBMPlexMono.txt`.
