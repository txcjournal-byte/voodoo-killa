# TrapWin Timer – od buildu do e-shopu

Každý push na GitHub sestaví v **Actions** tyto balíčky:

| Artifact | Obsah |
|---|---|
| `TrapWinTimer-Windows-Installer` | `TrapWin-Timer-<verze>-Windows-Setup.exe` (VST3 + Standalone, odinstalace v Nastavení Windows) |
| `TrapWinTimer-Windows-Plugin` | holé `TrapWin Timer.vst3` + `.exe` (ruční instalace / testování) |
| `TrapWinTimer-macOS-Installer` | `TrapWin-Timer-<verze>-macOS.pkg` + `.dmg` (VST3 + AU + Standalone, Universal: Apple Silicon i Intel) |

Verze se bere z `CMakeLists.txt` (`project(TrapWinTimer VERSION x.y.z ...)`) – před vydáním ji změň tam.

## 1. Testovací verze (teď)

- **Windows:** spusť Setup.exe → plugin je v `C:\Program Files\Common Files\VST3`.
  Bez podpisu ukáže SmartScreen „Neznámý vydavatel“ → *Další informace → Přesto spustit*.
- **Mac (bez podpisu):** pravý klik na `.pkg` → *Otevřít* (případně Nastavení → Soukromí a zabezpečení → *Přesto otevřít*).
  Takhle to posílej jen testerům, zákazníkům až podepsané (krok 2).

## 2. Mac – podpis a notarizace (Apple Developer Program, 99 USD/rok)

1. V developer.apple.com → Certificates vytvoř:
   - **Developer ID Application** (podpis pluginů a aplikace)
   - **Developer ID Installer** (podpis .pkg)
2. Na developer účtu vytvoř **app-specific password** (appleid.apple.com → Sign-in and Security).
3. Varianta A – **podepisování v GitHub Actions** (doporučuju, pak každý build vyjde hotový):
   v repozitáři Settings → Secrets and variables → Actions přidej:

   | Secret | Hodnota |
   |---|---|
   | `MAC_CERTS_P12` | oba certifikáty exportované z Klíčenky jako jeden `.p12`, zakódované `base64 -i certs.p12` |
   | `MAC_CERTS_PASSWORD` | heslo k `.p12` |
   | `MAC_APP_IDENTITY` | `Developer ID Application: Jméno (TEAMID)` |
   | `MAC_INSTALLER_IDENTITY` | `Developer ID Installer: Jméno (TEAMID)` |
   | `MAC_APPLE_ID` | e-mail Apple ID |
   | `MAC_TEAM_ID` | 10znakové Team ID |
   | `MAC_APP_PASSWORD` | app-specific password |

   Další build pak pluginy podepíše (hardened runtime), `.pkg` podepíše, pošle Applu k notarizaci a „staplne“.
4. Varianta B – **ručně na svém Macu** (Xcode + CMake):
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
   cmake --build build --parallel --target VoodooKilla_VST3 VoodooKilla_AU VoodooKilla_Standalone
   xcrun notarytool store-credentials trapwin --apple-id <email> --team-id <TEAMID> --password <app-specific>
   MAC_APP_IDENTITY="Developer ID Application: Jméno (TEAMID)" \
   MAC_INSTALLER_IDENTITY="Developer ID Installer: Jméno (TEAMID)" \
   MAC_NOTARY_PROFILE=trapwin \
   Packaging/mac/build_pkg.sh build/VoodooKilla_artefacts/Release 1.0.0 installers
   ```
5. Kontrola: `spctl -a -vv -t install installers/TrapWin-Timer-1.0.0-macOS.pkg` → *accepted, Notarized Developer ID*.
   AU ověříš v Terminálu: `auval -v aumf TwTm Trpw`.

## 3. Windows – podpis (volitelné, ale doporučené)

Bez podpisu instalačka funguje, jen SmartScreen varuje. Pro podpis potřebuješ code-signing certifikát
(např. Azure Trusted Signing, Certum, Sectigo). Podepisuje se `signtool sign /fd sha256 /tr <timestamp-url> /td sha256 ...`
nejdřív `TrapWin Timer.vst3\Contents\x86_64-win\TrapWin Timer.vst3` a `.exe`, pak výsledný Setup.exe.
Až budeš mít certifikát, doplním to do workflow stejně jako u Macu.

## 4. Licence

- **JUCE:** komerční prodej vyžaduje JUCE licenci (registrace na juce.com). Pro malé firmy existuje
  bezplatná/levná úroveň podle ročního obratu – aktuální podmínky ověř na juce.com/get-juce. Splash screen
  je v projektu vypnutý (`JUCE_DISPLAY_SPLASH_SCREEN=0`), což vyžaduje platnou licenci odpovídající úrovni.
- **VST3:** Steinberg SDK je součást JUCE; logo „VST“ v e-shopu jen podle Steinberg VST guidelines.
- **Fonty** Archivo a IBM Plex Mono: SIL OFL 1.1 (smí se vkládat do komerčního software).
- **EULA:** `Packaging/EULA.txt` je **koncept** – zobrazuje se v obou instalačkách. Před prodejem ho nech zkontrolovat.

## 5. Do e-shopu (Shopify)

Na stažení dej zákazníkům dva soubory: `TrapWin-Timer-<verze>-Windows-Setup.exe` a `TrapWin-Timer-<verze>-macOS.dmg`
(např. přes aplikaci Shopify Digital Downloads). Do popisu produktu: Windows 10/11 64-bit, macOS 10.13+ (Apple Silicon i Intel),
VST3 / AU / Standalone, testováno ve FL Studiu a Ableton Live.

## Kontrolní seznam před vydáním

- [ ] verze v `CMakeLists.txt`
- [ ] Actions build zelený (testy prošly na Windows i Macu)
- [ ] Mac: podepsáno + notarizováno (`spctl` = accepted)
- [ ] Windows: instalace, odinstalace, reinstalace přes starší verzi
- [ ] FL Studio (Win), Ableton Live (Win + Mac), Logic (AU): načtení, uložení projektu, zavření bez zamrznutí
- [ ] EULA zkontrolovaná, JUCE licence vyřízená
