# VOODOO KILLA – melodický time/FX plugin (zadání pro Claude Code)

Tento soubor je zadání projektu. Postupuj po milestonech (sekce 13). Po každém milestonu projekt
zkompiluj, spusť testy a krátce shrň, co je hotové a co mám vyzkoušet ve FL Studiu.

Vizuální předloha UI: `Design/ui-reference.png` (finální design – drž se ho).

---

## 1. Produkt

- **Název:** Voodoo Killa, slogan „Cast a preset. Kill the melody.“
- **Série:** Killa pluginy (808 Killa, Keys Killa, Rolls Killa).
- **Co to je:** audio efekt, který se dá na **melodii** (keys, kytara, pluck, vokál, sample). Mění ji v čase
  a hlasitosti (halftime, tape stop, stutter, reverse, chop…) + pitch, filtr, lo-fi, distorze, prostor.
- **Workflow uživatele:** dá plugin na kanál s melodií → klikne na kategorii → klikne na kartu (preset) →
  melodie hraje upravená. Nic dalšího nastavovat nemusí.
- **Pro koho:** trap / rage / plugg / underground producenti. Priorita: **vybírat, ne nastavovat.**
- **Obsah:** 12 kategorií × 8 presetů = **96 presetů**.
- **Hlavní DAW:** FL Studio (Windows). Musí fungovat i v Ableton Live a dalších VST3 hostech.
- **Konkurence:** Gross Beat (Image-Line), HalfTime (Cableguys). **Nekopíruj jejich názvy, UI, presety
  ani grafiku.** Princip (time/volume manipulace) je volný.

## 2. Technologie

- **C++17, JUCE 8, CMake** (JUCE přes `FetchContent`, zafixovaný tag).
- Formáty: **VST3 + Standalone** (AU volitelně při buildu na macOS).
- Typ: **audio efekt s MIDI vstupem** (`IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT TRUE`) – MIDI klávesy spouští karty.
- `PRODUCT_NAME "Voodoo Killa"`, `COMPANY_NAME "Killa"`, vlastní 4znakové plugin code.
- Build: Windows, Visual Studio 2022+ (nebo Ninja). Bez externích knihoven kromě JUCE.
- Stereo in/out, 44.1–192 kHz, libovolná velikost bufferu.

### Struktura

```
VoodooKilla/
  CMakeLists.txt
  Design/ui-reference.png          // vizuální předloha (není součást buildu)
  Source/
    PluginProcessor.*              // APVTS, host sync, trigger logika, routing modulů, morph A/B
    PluginEditor.*
    engine/TimeBuffer.*            // kruhový buffer + čtecí hlava (halftime, stop, reverse, repeat)
    engine/Modules/*.h/.cpp        // Pitch, Gate, Filter, LoFi, Drive, Space, Width, Duck
    engine/Preset.*                // datový model presetu + (de)serializace JSON
    engine/Morph.*                 // interpolace preset A ↔ B
    engine/Trigger.*               // Always / Hold / Every N / Last Beat / Steps / MIDI
    ui/Theme.*                     // barvy, fonty, textury, LookAndFeel
    ui/CategoryList.*  ui/PolaroidGrid.*  ui/Thread.*  ui/KillPad.*
    ui/Waveform.*  ui/Steps.*  ui/TriggerBar.*  ui/Knob.*  ui/MorphFader.*  ui/TopBar.*
  Resources/Presets/factory.json   // všech 96 presetů (generuj z tabulky v sekci 10)
  Resources/Photos/                // 96 fotek do polaroidů (sekce 9) + placeholder
  Resources/Textures/  Resources/Fonts/  Resources/Logo/
  Tests/
```

## 3. Signálový řetězec

```
IN ─► TIME (buffer/čtecí hlava) ─► PITCH ─► GATE ─► FILTER ─► LOFI ─► DRIVE ─► SPACE ─► WIDTH ─► DUCK ─► MIX(dry/wet) ─► OUT
```

- Každý modul má **bypass**, když ho preset nepoužívá (nulová CPU zátěž).
- **Efekt je „aktivní“ jen když trigger dovolí** (sekce 5). Mimo aktivní úseky plugin propouští dry signál,
  přechody mají crossfade 5–20 ms (žádné lupance).
- Latence: nahlas ji hostu (`setLatencySamples`), pokud ji nějaký modul potřebuje (pitch).

## 4. Moduly (parametry, se kterými pracují presety)

**TIME** – kruhový buffer min. 8 taktů při 60 BPM (stereo), čtecí hlava synchronní s hostem.
- `mode`: `none | half | quarter | double | stop | start | stopstart | rewind | reverse | repeat | scatter`
- `rate`: délka opakování/segmentu (`1/4, 1/8, 1/16, 1/32, 1/64, 1/8T, 1/16T`)
- `length`: délka úseku v dobách (0.25–4)
- `curve`: tvar zpomalení u stop/start (0 = lineární, 1 = exponenciální „tape“)
- `ramp`: u repeat zrychlování (0–1) – „ramp roll“
- `pitchFollow`: u half/stop/rewind pitch klesá s rychlostí (true = páskový efekt, false = time-stretch bez změny výšky)
- Half/quarter musí čerpat z bufferu tak, aby se na konci úseku vrátil do synchronu (reset na začátku každého úseku).

**PITCH** – `semis` (−24..+24), `fine`, `endDrop` (pád o X půltónů na poslední dobu úseku), `wobble` (LFO depth v centech + rate), `formant` (−12..+12). Implementace: granulární / PSOLA pitch shifter s nízkou latencí.

**GATE** – `pattern` (16 kroků, řetězec `x`/`-`/čísla 0–9 pro hlasitost), `rate` (1/16 nebo 1/16T), `depth`, `smooth` (ms).

**FILTER** – `type` LP/HP/BP, `cutoff` (Hz), `reso`, `lfoRate` (sync), `lfoDepth`, `sweep` (od→do přes délku úseku).

**LOFI** – `bits` (4–16), `srate` (2–44 kHz), `wow`, `flutter`, `noise` (vinyl/tape šum), `tone` (LP).

**DRIVE** – `type` tape/fuzz/clip/fold/bit, `amount`, `postLP`.

**SPACE** – `reverbSize`, `reverbMix`, `shimmer` (+12 st), `freeze` (bool), `delayTime` (sync), `delayFb`, `pingPong`, `delayMix`.

**WIDTH** – `width` 0–200 %.

**DUCK (808 Duck)** – sidechain: vstup buď z host sidechain busu, nebo z MIDI noty 808 (když není sidechain). `amount`, `release`.

## 5. Trigger (kdy efekt běží)

- **Always** – pořád.
- **Hold** – jen při držení karty (myš) nebo MIDI klávesy.
- **Every 4 / Every 8** – automaticky na posledním úseku 4/8taktové fráze (délka = `length`).
- **Last Beat** – poslední doba každého taktu.
- **Steps** – 16kroková lišta (1 takt), efekt běží na zapnutých krocích.
- Každý preset má výchozí trigger (sekce 10), uživatel ho může přepnout.

**MIDI mapování:** C3–G3 = karty 1–8 aktuální kategorie. C2–B2 = výběr kategorie 1–12.
Velocity škáluje AMOUNT.

## 6. Makra, Morph A/B, KILL PAD

- **AMOUNT** – síla efektu (škáluje hlavní parametry presetu: hloubky, mixy, drive…)
- **SPEED** – posouvá `rate` / `length` o stupeň rychleji/pomaleji (zobrazení např. `1/2`, `1`, `2`)
- **TONE** – tilt: tmavší (LP) ↔ jasnější (HP/shelf), zobrazení −12…+12
- **SPACE** – reverb/delay mix
- **MIX** (dry/wet), **OUT** (dB), **808 DUCK** on/off
- Všechno jsou APVTS parametry, automatizovatelné, s `SmoothedValue` (30 ms).

### Morph A/B (výběr dvou karet)

- **Klik na kartu** = preset se nahraje jako **A** (a B se zruší → čistý preset, morph vypnutý).
- **Shift+klik nebo pravý klik** na jinou kartu = preset **B**. B může být i z jiné kategorie.
- Tlačítko **A | B** v horní liště určuje, do kterého slotu jde další obyčejný klik (alternativa ke Shift).
- Vybrané karty mají červený rámeček a **červený špendlík se štítkem „A“ / „B“**.
- **Červená nit** (kreslená kódem, kubická Bézierova křivka s jemným průvěsem) vede ze špendlíku A → špendlík B →
  do tečky v KILL PADu. Vede **obloukem nad kartami**, nepřekrývá fotky. Při tažení tečky se nit plynule hýbe.
- **MORPH** parametr (0–1) = interpolace všech spojitých parametrů A → B; diskrétní (mode, type, trigger, pattern)
  se přepnou v 50 %. Přechod bez lupanců (u TIME modulu crossfade mezi dvěma čtecími hlavami).
- Interpolovaný preset se počítá mimo audio vlákno a předává lock-free (viz sekce 12).

### KILL PAD (XY)

- **Když je vybrané A i B:** X = **MORPH** (A vlevo, B vpravo – štítky „A“/„B“ na okrajích), Y = **TONE**.
- **Když je jen A:** X = **AMOUNT** (štítek „AMOUNT“ dole), Y = **TONE**.
- Tečka a MORPH fader / knoby jsou obousměrně provázané (hýbe se jedno → hýbe se druhé).
- Dvojklik do padu = návrat do středu. Automatizovatelné, hladké (smoothing 30 ms).

## 7. UI – podle `Design/ui-reference.png`

**Styl:** syrový underground trap / mixtape zine. Téměř monochrom: černá, špinavě bílý papír, šedá.
**Jediný akcent je červená `#FF2E3E`** (aktivní kategorie, aktivní karty, špendlíky, nit, zapnuté steps, trigger, KILL).
Klidné černé pozadí s jemným zrnem papíru, hodně prázdného místa. **Žádné dekorativní nápisy, čmáranice,
maskoti ani panenky.** Všechno zarovnané v mřížce (8 px), pásky a natržené okraje jen decentně.

**Velikost:** 1000×625 px, škálovatelné 100 / 125 / 150 % (volba v nastavení, uložit do stavu).

**Rozložení:**

```
┌───────────────────────────────────────────────────────────────────────────┐
│ VOODOO KILLA (logo)          [◀ SLOW GRAVE ▶ | ★ | A|B | ↶ ↷ | 💾 | 🎲]  [KILL 🎲] 🔒 │
│ MELODY FX · HALFTIME · TAPE STOP · STUTTER · REVERSE · PITCH · LO-FI              │
├──────────┬────────────────────────────────────────┬───────────────────────┤
│ HALFCUT  │  vlna (páska papíru, dry šedá / wet    │  KILL PAD (milimetrový│
│ DEAD TAPE│  červená, červená přehrávací hlava)    │  papír, červená tečka)│
│ …12 štítků├────────────────────────────────────────┤  A ◄──────► B        │
│ (masking │  8 polaroidů 4×2                        ├───────────────────────┤
│  tape)   │  fotka + název (marker) + popisek       │  STEPS 1–16           │
│          │  (psací stroj)                          │  TRIGGER [Always|Hold|…]│
├──────────┴───────────────┬────────────────────────┴──────────┬────────────┤
│ AMOUNT SPEED TONE SPACE MIX OUT (černé knoby, červená ryska) │ MORPH A═●═B │ 808 DUCK ● │
└──────────────────────────────────────────────────────────────┴─────────────┴────────────┘
```

**Prvky:**
- **Kategorie** – 12 štítků z masking tape pod sebou, číslo 1–12 malým písmem vlevo (odpovídá MIDI C2–B2).
  Aktivní štítek červený. Po najetí tooltip s popiskem kategorie (např. „Halftime“).
- **Polaroidy (karty)** – 4×2, černobílá zrnitá fotka (sekce 9), pod ní název presetu marker fontem
  a malý popisek psacím strojem. Aktivní (A/B) = červený rámeček + špendlík. Když efekt právě běží (trigger),
  karta jemně pulzuje (světlejší rámeček). Držení karty v režimu Hold = spuštění.
- **Vlna** – na proužku papíru, dry šedě, wet červeně přes něj, svislá červená přehrávací hlava, jemná mřížka dob.
- **Knoby** – černé bakelitové, červená ryska, hodnota ručním písmem nad názvem. Tah myší nahoru/dolů,
  dvojklik = výchozí hodnota, Ctrl+tah = jemně.
- **STEPS** – 16 čtverečků, zapnuté červené, čísla pod nimi.
- **TRIGGER** – 6 tlačítek s rámečkem, aktivní červené.
- **MORPH** – fader na pásce, „A“ vlevo, „B“ vpravo; neaktivní (šedý), když není vybrané B.
- **808 DUCK** – červený kolébkový vypínač.
- **KILL** – obyčejné obdélníkové tlačítko (černé, červený rámeček a text, ikona kostky) = náhodný preset;
  🔒 zámek = náhodně jen v rámci aktuální kategorie. Pravý klik na KILL = **Mutate** (±15 % náhodná odchylka parametrů).
- **Horní lišta** – ◀ ▶ přepínání presetů, ★ oblíbené, A|B slot, undo/redo, 💾 uložit, 🎲 náhodný.
- **Tooltipy** u všech ovládacích prvků (1 krátká věta, anglicky).
- **Draw režim** (verze 1.1): kreslení vlastní křivky času/hlasitosti – teď jen připrav architekturu.
- Animace jemné (pulz karty, pohyb nitě), max 60 fps, nízké CPU, překreslovat jen změněné oblasti.

**Co je obrázek a co kód:**

| Obrázek (PNG, `BinaryData`) | Kreslí kód (JUCE `Graphics`) |
|---|---|
| logo VOODOO KILLA (ve 2 rozlišeních 1× a 2×) | všechen text kromě loga |
| textura pozadí (černé zrno) | knoby, fadery, vypínač |
| textura papíru (štítky, polaroidy, panely) – dlaždice | STEPS, TRIGGER, tlačítka lišty |
| 2–3 varianty kousků pásky | vlna, přehrávací hlava |
| 96 fotek do polaroidů + placeholder | KILL PAD (mřížka, tečka, glow) |
| špendlík (červený) | nit (Bézier), rámečky, zvýraznění |

Dokud grafika není hotová, použij **placeholdery** (šedé plochy s názvem), ať UI jde postavit a testovat hned.
Assety musí jít vyměnit bez změny kódu (pevné názvy souborů v `Resources/`).

**Fonty** (OFL licence, vložit do `BinaryData`): psací stroj – *Special Elite* (štítky, popisky, hodnoty),
marker – *Permanent Marker* (názvy presetů, hodnoty knobů). Jiné fonty jen se souhlasem.

## 8. Barvy kategorií

UI je monochromatické – barvy kategorií se **nezobrazují** v hlavním UI, slouží jen interně (JSON, případně budoucí skiny).
Aktivní stav je vždy červený `#FF2E3E`.

## 9. Fotky do polaroidů (96 ks)

- Formát 512×512 px, černobílé, zrnité, foceno bleskem, noc, underground atmosféra, bez lidí v popředí / bez tváří,
  bez textu a log. Motiv volně ilustruje název presetu.
- Soubory: `Resources/Photos/<kategorie>_<číslo>.jpg` (např. `halfcut_2.jpg`), + `placeholder.jpg`.
- **Úkol pro tebe (M5):** vytvoř `Resources/Photos/prompts.md` – 96 promptů pro generátor obrázků, jeden na preset,
  všechny se stejným stylovým základem:
  `Black and white flash photograph at night, heavy film grain, underground trap mixtape aesthetic, harsh flash, deep shadows, square 1:1, no text, no people's faces. Subject: <motiv>`
  Já je vygeneruju a nahraju do složky.

## 10. Presety (96) – zdroj pro `factory.json`

Formát řádku: `Název | popisek | moduly | trigger`.
Hodnoty jsou výchozí (AMOUNT = 100 %). Doladit poslechem je v pořádku, ale zachovej charakter.
Každý preset v JSON navíc obsahuje `id`, `category`, `photo` (název souboru).

### 1. HALFCUT – *Halftime* (#8A2BE2)
1. Halfcut | halftime | time=half,len=4 | Always
2. Slow Grave | halftime + dark | time=half; filter=LP 1800 | Always
3. Quarter Death | quarter time | time=quarter,len=4 | Every 4
4. NPC Mode | robotic lofi | time=half; lofi bits=8,srate=16k | Always
5. Sleepwalk | halftime + wobble | time=half,pitchFollow; pitch wobble=25c@0.5Hz | Always
6. Slow Bleed | reverb tail | time=half; space reverb=0.8,mix=0.35 | Always
7. Cooked | tape drive | time=half; drive tape 0.5; lofi wow=0.3 | Every 4
8. Drag Me Down | octave down | time=half; pitch -12 | Last Beat

### 2. DEAD TAPE – *Tape Stop* (#B22222)
1. Flatline | stop 1 bar | time=stop,len=4,curve=0.7 | Every 4
2. Power Cut | stop 1/4 | time=stop,len=1,curve=0.9 | Last Beat
3. Last Breath | slow stop + reverb | time=stop,len=2; space reverb=0.9,mix=0.4 | Every 8
4. Brake Check | stop then start | time=stopstart,len=1 | Last Beat
5. Rewind Demon | tape rewind | time=rewind,len=1,pitchFollow | Every 4
6. It's Over | stop + fade + lofi | time=stop,len=4; lofi noise=0.3 | Every 8
7. Dead Battery | stop + gate stutter | time=stop,len=2; gate x-x-x-x--x--x--- | Every 4
8. Unplugged | vinyl power-down | time=stop,len=0.5,curve=1; lofi noise=0.2 | Last Beat

### 3. GLITCH RITVAL – *Stutter / Repeat* (#00E5FF)
1. Ritval | repeat 1/8 | time=repeat,rate=1/8,len=1 | Last Beat
2. Brainrot | random scatter | time=scatter,rate=1/16,len=2 | Every 4
3. Possessed | repeat 1/16 + pitch up | time=repeat,rate=1/16; pitch ramp +12 over len | Last Beat
4. Machine Gun | repeat 1/32 | time=repeat,rate=1/32,len=0.5 | Last Beat
5. Triplet Curse | triplet repeat | time=repeat,rate=1/8T,len=1 | Every 4
6. Spiral | accelerating roll | time=repeat,rate=1/8,ramp=1,len=1 | Last Beat
7. Broken Prayer | repeat + reverse | time=repeat,rate=1/8; odd repeats reversed | Every 4
8. Glitch Mass | scatter + bitcrush | time=scatter,rate=1/32; lofi bits=6 | Every 8

### 4. CHOPPED – *Chop / Gate* (#FF6A00)
1. Chopped | 1/16 gate | gate x-x-x-x-x-x-x-x- | Always
2. Cleaver | hard 1/8 chop | gate xx--xx--xx--xx-- smooth=1 | Always
3. Rage Chop | rage gate + drive | gate xxx-xx-xxx-x-xx-; drive clip 0.4 | Always
4. Drill Skip | drill pattern | gate x--x--x---x--x-- | Always
5. Bounce Hex | plugg bounce | gate x-xx-x-xx-x-xx-x, depth 0.8 | Always
6. Triplet Slice | triplet gate | gate rate=1/16T x-x | Always
7. Offbeat Knife | offbeat gate | gate -x-x-x-x-x-x-x-x | Always
8. Chainsaw | 1/32 gate + fuzz | gate 1/32 x-; drive fuzz 0.5 | Last Beat

### 5. BACKMASK – *Reverse* (#9400D3)
1. Backmask | reverse 1/4 | time=reverse,len=1 | Last Beat
2. Hidden Message | reverse 1 bar | time=reverse,len=4 | Every 4
3. Satan Spin | reverse + octave down | time=reverse,len=1; pitch -12 | Last Beat
4. Reverse Swell | reverse + reverb swell | time=reverse,len=2; space reverb=0.9,mix=0.5 | Every 4
5. Delulu | reverse + dreamy shimmer | time=reverse,len=1; space shimmer=0.4 | Every 4
6. Mirror | alternating fwd/rev 1/8 | time=reverse,rate=1/8 alternate | Always
7. Rewind Soul | reverse echo | time=reverse,len=1; delay 1/8 fb=0.5 | Every 4
8. Inverse Cross | reverse + halftime | time=reverse+half,len=2 | Every 8

### 6. DIABOLVS – *Pitch* (#DC143C)
1. Diabolvs | tritone down | pitch -6 | Always
2. Lucifer Low | octave down | pitch -12 | Always
3. Angel High | octave up + air | pitch +12; filter HP 300 | Always
4. Tritone | tritone wobble | pitch -6, wobble 30c@2Hz | Always
5. Fall From Grace | end-bar pitch drop | pitch endDrop=-12 | Last Beat
6. Demon Voice | down + formant down | pitch -7, formant -6; drive tape 0.3 | Always
7. Chipmunk Cult | up + formant up | pitch +7, formant +5 | Always
8. Glazed | detune wide chorus | pitch ±12c two voices; width 170 % | Always

### 7. CASSETTE CVLT – *Lo-Fi* (#C8A165)
1. Cvlt Tape | cassette | lofi wow=0.3,flutter=0.2,noise=0.15,tone LP 9k | Always
2. Basement | muffled room | filter LP 1200; space reverb=0.4,mix 0.2 | Always
3. Memphis Mist | dark tape + crush | lofi bits=10,srate=22k,wow=0.2; filter LP 4k | Always
4. Warped | heavy wow | lofi wow=0.8 | Always
5. 8-Bit Soul | 8-bit | lofi bits=6,srate=11k | Always
6. Payphone | telephone | filter BP 400–3000; drive clip 0.3 | Always
7. Pirate Radio | radio + noise | filter BP 600–4000; lofi noise=0.4 | Always
8. Mid | subtle lofi | lofi bits=12,wow=0.1,noise=0.05 | Always

### 8. VILLAIN ERA – *Dark* (#4B0082)
1. Villain Era | dark filter + verb | filter LP 1500; space reverb=0.6,mix 0.25 | Always
2. Midnight Mass | dark + choir shimmer | filter LP 2500; space shimmer=0.3 | Always
3. Nox | pitch down + dark | pitch -5; filter LP 2000 | Always
4. Shadow Walk | dark + ping-pong | filter LP 2000; delay 1/8 pingPong fb=0.45 | Always
5. Hollow | band-pass hollow | filter BP 800 reso 0.6 | Always
6. Graveyard Shift | dark halftime | time=half; filter LP 1200 | Every 4
7. Black Candle | dark + tape drive | filter LP 1800; drive tape 0.5 | Always
8. Void | freeze pad | space freeze=true,mix 0.5; filter LP 3000 | Hold

### 9. AURA – *Space / Reverb* (#40E0D0)
1. Aura | big reverb | space reverb=0.8,mix 0.35 | Always
2. Aura Farming | shimmer + wide | space shimmer=0.5,mix 0.35; width 180 % | Always
3. Heaven Gate | shimmer + HP | space shimmer=0.6; filter HP 400 | Always
4. Frozen Soul | freeze on hold | space freeze=true | Hold
5. Main Character | wide ping-pong | delay 1/4D pingPong fb 0.4; width 170 % | Always
6. Underwater Tomb | LP + chorus + verb | filter LP 900; pitch wobble 15c@0.3Hz; reverb 0.7 | Always
7. Cathedral | huge hall | space reverb=1.0,mix 0.45 | Always
8. Echo Chamber | echo throw | delay 1/4 fb 0.6 mix 0.4 | Last Beat

### 10. TOXIC – *Filter / Sweep* (#7CFC00)
1. Toxic | LP sweep in | filter LP sweep 300→18k over 4 bars | Every 4
2. Slime Sweep | resonant LFO | filter LP 1500 reso 0.7, lfo 1/4 depth 0.6 | Always
3. Radioactive | HP build | filter HP sweep 20→2000 over 4 bars | Every 8
4. Gas Mask | muffled + breath noise | filter LP 700; lofi noise=0.2 | Always
5. Phone Tap | telephone intro | filter BP 500–2500 | Always
6. Pump Poison | sidechain pump | gate smooth pump 1/4 depth 0.7 | Always
7. The Ick | notch wobble | filter BP lfo 1/8 | Always
8. Acid Bounce | resonant 1/16 steps | filter LP reso 0.8, lfo S&H 1/16 | Always

### 11. CRASH OUT – *Rage / Distortion* (#FF0000)
1. Crash Out | rage drive | drive clip 0.7; filter HP 120 | Always
2. Hellfire | fuzz + octave | drive fuzz 0.6; pitch +12 blend 30 % | Always
3. Blown Out | blown speaker | drive clip 1.0; filter BP 300–5000 | Always
4. Molten | tape saturation + wobble | drive tape 0.8; pitch wobble 10c | Always
5. Tweaking | bitcrush drive | drive bit 0.6; lofi srate=8k | Always
6. Clipper Cult | hard clipper | drive clip 0.9 | Always
7. Screamer | fold distortion + HP | drive fold 0.7; filter HP 400 | Always
8. Burn It Down | distortion + halftime | drive fuzz 0.6; time=half | Every 4

### 12. LOCK IN – *Transitions / Drops* (#FFD700)
1. Lock In | pre-hook stop | time=stop,len=1 | Every 8
2. Pre-Drop Stop | 2-beat stop + mute | time=stop,len=2; then mute last 1/8 | Every 8
3. Intro Crypt | filtered intro | filter LP 800 | Always
4. Rise Up | riser pitch + HP | filter HP sweep 20→1500; pitch ramp +7 over len=4 | Every 8
5. Drop Kill | full mute last beat | gate mute last beat | Every 4
6. Mute for 808 | strong 808 duck | duck amount 0.9, release 120 ms | Always
7. End Bar Spin | backspin | time=rewind,len=1 | Every 4
8. No Cap Drop | stutter into drop | time=repeat,rate=1/16,ramp=1,len=2 | Every 8

## 11. Presety – funkce

- Factory presety read-only, uživatelské ukládání do `Documents/Killa/Voodoo Killa/Presets` (JSON).
  Uživatelský preset může ukládat i dvojici A+B a pozici morphu.
- **Favorites ★**, **Undo/Redo** (min. 30 kroků), celý stav (A, B, morph, makra, trigger, steps, zoom UI)
  se ukládá do projektu DAW.
- ◀ ▶ přepínání presetů bez výpadku zvuku (crossfade).

## 12. Kvalita a testy

- **Real-time safety:** žádné alokace, zámky ani I/O v `processBlock`. Preset/morph se předává lock-free.
- **Host sync:** správně při loopu, změně tempa, skoku pozice, zastavení transportu (buffer se vyčistí/resyncne).
- **Bez lupanců:** crossfade při startu/konci efektu, při změně presetu, při morphu a při bypassu.
- **Unit testy:** TimeBuffer (half/stop/reverse/repeat dávají správnou délku a synchronizaci), Gate patterny,
  načtení všech 96 presetů z JSON, morph interpolace (0 = A, 1 = B, diskrétní přepnutí v 0.5),
  determinismus KILL se seedem.
- **Offline render test:** projeď všechny presety a 10 náhodných dvojic A/B s morphem 0.5 na testovacím signálu
  (sinus + klik na každé době), ověř, že výstup nemá NaN/Inf, nepřebuzí nad +6 dBFS a na konci úseku se vrátí do synchronu.
- **pluginval** strictness 5+ musí projít. CPU cíl: < 5 % jednoho jádra při 44.1 kHz / 256 samplů (i při morphu).
- UI: překreslení < 2 ms na snímek, žádné zasekávání při tažení v KILL PADu.

## 13. Milestony

1. **M1** – JUCE/CMake projekt, prázdný VST3 + Standalone „Voodoo Killa“, načte se ve FL Studiu.
2. **M2** – TimeBuffer + TIME modul (half, quarter, stop, start, rewind, reverse, repeat, scatter) + host sync + testy.
3. **M3** – ostatní moduly (Pitch, Gate, Filter, LoFi, Drive, Space, Width, Duck).
4. **M4** – Preset systém + všech 96 presetů v `factory.json` + trigger režimy + MIDI + Morph A/B engine.
5. **M5** – UI s placeholdery: kategorie, polaroidy, výběr A/B, nit, KILL PAD, vlna, Steps, Trigger, knoby,
   Morph, KILL/Mutate, tooltipy, škálování. + `Resources/Photos/prompts.md`.
6. **M6** – Finální grafika (logo, textury, fotky, fonty) – napojení assetů, doladění podle `ui-reference.png`.
7. **M7** – Favorites, undo/redo, uživatelské presety, ukládání stavu.
8. **M8** – pluginval, CPU optimalizace, Release build, README (instalace: `C:\Program Files\Common Files\VST3`).

## 14. Pravidla

- Před větší změnou krátký plán, pak pokračuj bez čekání.
- Bez nových knihoven/služeb mimo JUCE bez zeptání.
- Commit po každém milestonu (pokud je git).
- Co nejde otestovat automaticky (FL Studio), napiš mi přesný testovací postup.
- Žádná cizí loga ani trademarky, žádné kopírování Gross Beat / HalfTime.
