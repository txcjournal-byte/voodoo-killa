# TrapWin Timer – od buildu do e-shopu

Každý push sestaví v GitHub **Actions** instalačky pro Windows i Mac.
Když nahraješ tag verze (`v1.0.0`), vznikne navíc **GitHub Release** s hotovými soubory pro e-shop.

| Soubor | Pro koho |
|---|---|
| `TrapWin-Timer-<verze>-Windows-Setup.exe` | Windows 10/11 64-bit – VST3 + Standalone |
| `TrapWin-Timer-<verze>-macOS.dmg` | macOS 10.13+ (Apple Silicon i Intel) – VST3 + AU + Standalone |

---

## Krok 1 – Apple (jednou, cca 30 min + schválení účtu)

1. **Apple Developer Program** – developer.apple.com/programs → Enroll (99 USD/rok).
   Schválení trvá obvykle 1–2 dny (u firmy potřebuješ D-U-N-S číslo, jako fyzická osoba ne).
2. Na Macu otevři **Xcode → Settings → Accounts** → přidej Apple ID → *Manage Certificates* → **+** →
   - **Developer ID Application**
   - **Developer ID Installer**
   (Případně přes developer.apple.com → Certificates; certifikáty se uloží do Klíčenky.)
3. **Klíčenka** (Keychain Access) → *Moje certifikáty* → označ oba „Developer ID …“ → pravý klik →
   *Exportovat 2 položky* → `trapwin-certs.p12` + zvol heslo.
4. **App-specific password**: appleid.apple.com → Sign-In and Security → App-Specific Passwords → vytvoř („GitHub notarize“).
5. V Terminálu ve složce projektu:
   ```bash
   Packaging/mac/prepare_secrets.sh ~/Desktop/trapwin-certs.p12
   ```
   Skript vypíše přesné hodnoty a obsah `.p12` dá do schránky.
6. GitHub → repozitář → **Settings → Secrets and variables → Actions → New repository secret** – založ:

   | Secret | Hodnota |
   |---|---|
   | `MAC_CERTS_P12` | vložit ze schránky (krok 5) |
   | `MAC_CERTS_PASSWORD` | heslo k `.p12` |
   | `MAC_APP_IDENTITY` | `Developer ID Application: Jméno (TEAMID)` |
   | `MAC_INSTALLER_IDENTITY` | `Developer ID Installer: Jméno (TEAMID)` |
   | `MAC_TEAM_ID` | 10znakové Team ID |
   | `MAC_APPLE_ID` | e-mail Apple ID |
   | `MAC_APP_PASSWORD` | app-specific password z kroku 4 |

   **Místo app-specific password** (když neznáš heslo k Apple ID) jde notarizovat přes API klíč:
   App Store Connect → Users and Access → Integrations → App Store Connect API → Team Keys → **+** (role *Developer*),
   stáhni `.p8` a založ secrets `MAC_NOTARY_KEY` (celý obsah `.p8`), `MAC_NOTARY_KEY_ID` (Key ID)
   a `MAC_NOTARY_ISSUER` (Issuer ID). Když je `MAC_NOTARY_KEY` nastavený, má přednost před heslem.

Od té chvíle každý build na Macu: podepíše plugin i aplikaci (hardened runtime), podepíše `.pkg`,
pošle ho Applu k **notarizaci**, přibije tiket (staple), totéž s `.dmg`, a ověří výsledek (`spctl`).
Když Apple notarizaci odmítne, build selže a v logu je důvod.

> Bez GitHubu to jde i ručně na Macu – viz „Ruční build na Macu“ dole.
>
> **Bez Macu (Windows/Linux):** klíč a CSR lze vytvořit přes `openssl req -new -newkey rsa:2048 -nodes`,
> CSR nahrát na developer.apple.com → Certificates → Developer ID Application / Installer (G2 Sub-CA),
> a pro každou identitu složit vlastní `.p12` (`openssl pkcs12 -export -legacy …`, stejné heslo).
> Do `MAC_CERTS_P12` pak dej oba base64 řetězce oddělené čárkou (`base64(app.p12),base64(installer.p12)`) –
> `security import` z ručně sloučeného `.p12` načte jen první identitu.

## Krok 2 – Windows (volitelné)

Instalačka funguje i nepodepsaná – Windows jen při prvním spuštění ukáže modré okno SmartScreen
(*Další informace → Přesto spustit*). Hodně menších výrobců pluginů tak prodává; do popisu v e-shopu to stačí napsat.

Když budeš chtít podpis:
- **Certifikát v souboru .pfx** (některé OV/EV certifikáty): přidej secrets `WIN_CERT_PFX` (`base64` z .pfx)
  a `WIN_CERT_PASSWORD` → build sám podepíše plugin, aplikaci i instalačku.
- **Cloudový podpis** (Azure Trusted Signing, SSL.com eSigner, Certum SimplySign – dnes nejlevnější a nejčastější,
  protože nové certifikáty se do souboru už exportovat nedají): napiš mi, kterou službu máš, a doplním ji do buildu.

## Krok 3 – Vydání verze

1. Verze je v `CMakeLists.txt`: `project(TrapWinTimer VERSION 1.0.0 ...)`.
2. Tag:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
3. Actions sestaví, otestuje, podepíše a vytvoří **Release** (záložka *Releases* na GitHubu) s `.exe` a `.dmg`.
   Release se **nikdy nevydá nepodepsaný** – bez Apple secrets build na tagu schválně selže.

## Krok 4 – E-shop (Shopify)

- Produkt jako digitální: aplikace **Shopify Digital Downloads** (zdarma) → k produktu nahraj oba soubory z Release.
- Do popisu: *Windows 10/11 (64-bit), macOS 10.13+ (Apple Silicon & Intel) · VST3 / AU / Standalone ·
  FL Studio, Ableton Live, Logic Pro, Studio One, Reaper, Cubase*.
- Návod pro zákazníka je přímo v instalačce (README).

## Licence

- **JUCE** – pro prodej potřebuješ JUCE licenci odpovídající obratu (registrace na juce.com, pro malé výrobce
  je nízká/bezplatná úroveň – aktuální podmínky ověř na juce.com/get-juce).
- **EULA** (`Packaging/EULA.txt`) je **koncept**, zobrazí se v obou instalačkách – před prodejem nech zkontrolovat.
- Fonty Archivo a IBM Plex Mono: SIL OFL 1.1 (komerční použití v software povoleno).
- „VST“ je ochranná známka Steinberg – logo VST v e-shopu jen podle jejich pravidel (stačí text „VST3“).

## Ruční build na Macu (bez GitHubu)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build build --parallel --target VoodooKilla_VST3 VoodooKilla_AU VoodooKilla_Standalone
xcrun notarytool store-credentials trapwin --apple-id <email> --team-id <TEAMID> --password <app-specific>
MAC_APP_IDENTITY="Developer ID Application: Jméno (TEAMID)" \
MAC_INSTALLER_IDENTITY="Developer ID Installer: Jméno (TEAMID)" \
MAC_NOTARY_PROFILE=trapwin \
Packaging/mac/build_pkg.sh build/VoodooKilla_artefacts/Release 1.0.0 installers
```

## Kontrolní seznam před prodejem

- [ ] Apple secrets nastavené, build zelený, v logu „signed + notarized“
- [ ] Mac: `.dmg` stažený z prohlížeče se otevře a nainstaluje bez varování
- [ ] Windows: instalace, odinstalace, reinstalace přes starší verzi
- [ ] FL Studio (Win), Ableton Live (Win + Mac), Logic (AU): načtení, uložení projektu, zavření bez zamrznutí
- [ ] EULA zkontrolovaná, JUCE licence vyřízená
- [ ] verze v `CMakeLists.txt`, tag `v…` nahraný, Release obsahuje `.exe` + `.dmg`
