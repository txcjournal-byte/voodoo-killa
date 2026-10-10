#!/usr/bin/env bash
# TrapWin Timer - macOS installer (.pkg inside a .dmg).
#
#   Packaging/mac/build_pkg.sh <artefacts dir> <version> [output dir]
#
# <artefacts dir> = build/VoodooKilla_artefacts/Release (contains VST3/, AU/, Standalone/)
#
# Signing is optional and driven by environment variables:
#   MAC_APP_IDENTITY        "Developer ID Application: Your Name (TEAMID)"   -> signs plug-ins + app
#   MAC_INSTALLER_IDENTITY  "Developer ID Installer: Your Name (TEAMID)"     -> signs the .pkg
#   MAC_NOTARY_PROFILE      keychain profile created with `xcrun notarytool store-credentials`
#     or MAC_APPLE_ID + MAC_TEAM_ID + MAC_APP_PASSWORD (app-specific password)
# Without them the bundles get an ad-hoc signature (fine for testing: right-click > Open).
set -euo pipefail

ART="${1:?artefacts dir}"
VERSION="${2:?version}"
OUT="${3:-installers}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
NAME="TrapWin Timer"
ID="com.trapwin.timer"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

VST3="$ART/VST3/$NAME.vst3"
AU="$ART/AU/$NAME.component"
APP="$ART/Standalone/$NAME.app"

sign_bundle() {
    local bundle="$1"
    local ent=()
    [[ "$bundle" == *.app ]] && ent=(--entitlements "$HERE/app.entitlements")   # audio input under hardened runtime
    if [[ -n "${MAC_APP_IDENTITY:-}" ]]; then
        codesign --force --deep --options runtime --timestamp "${ent[@]+"${ent[@]}"}" --sign "$MAC_APP_IDENTITY" "$bundle"
    else
        codesign --force --deep "${ent[@]+"${ent[@]}"}" --sign - "$bundle"
    fi
    codesign --verify --deep --strict "$bundle"
}

stage() {   # stage <bundle> <component id> <install location>
    local src="$1" comp="$2" dest="$3"
    [[ -d "$src" ]] || { echo "skip: $src not found"; return 0; }
    mkdir -p "$WORK/$comp/root"
    ditto "$src" "$WORK/$comp/root/$(basename "$src")"
    sign_bundle "$WORK/$comp/root/$(basename "$src")"
    pkgbuild --root "$WORK/$comp/root" \
             --identifier "$ID.$comp" \
             --version "$VERSION" \
             --install-location "$dest" \
             "$WORK/$comp.pkg"
}

stage "$VST3" vst3 "/Library/Audio/Plug-Ins/VST3"
stage "$AU"   au   "/Library/Audio/Plug-Ins/Components"
stage "$APP"  app  "/Applications"

# installer pages
mkdir -p "$WORK/resources"
cp "$ROOT/Packaging/EULA.txt" "$WORK/resources/License.txt"
cp "$ROOT/Packaging/README-USER.txt" "$WORK/resources/ReadMe.txt"
sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"

PKG="$OUT/TrapWin-Timer-$VERSION-macOS.pkg"
SIGN_ARGS=()
[[ -n "${MAC_INSTALLER_IDENTITY:-}" ]] && SIGN_ARGS=(--sign "$MAC_INSTALLER_IDENTITY" --timestamp)
productbuild --distribution "$WORK/distribution.xml" \
             --resources "$WORK/resources" \
             --package-path "$WORK" \
             ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} \
             "$PKG"

notarize() {   # notarize <file>: submit to Apple, fail on rejection, staple the ticket
    local file="$1"
    local args=()
    if [[ -n "${MAC_NOTARY_PROFILE:-}" ]]; then
        args=(--keychain-profile "$MAC_NOTARY_PROFILE")
    elif [[ -n "${MAC_APPLE_ID:-}" ]]; then
        args=(--apple-id "$MAC_APPLE_ID" --team-id "$MAC_TEAM_ID" --password "$MAC_APP_PASSWORD")
    else
        echo "warning: no notarization credentials - $file is signed but not notarized"
        return 0
    fi
    local out
    out="$(xcrun notarytool submit "$file" "${args[@]}" --wait --output-format plist)"
    echo "$out"
    if ! grep -q "<string>Accepted</string>" <<<"$out"; then
        local id
        id="$(/usr/libexec/PlistBuddy -c 'Print :id' /dev/stdin <<<"$out" 2>/dev/null || true)"
        [[ -n "$id" ]] && xcrun notarytool log "$id" "${args[@]}" || true
        echo "notarization FAILED for $file"; exit 1
    fi
    xcrun stapler staple "$file"
    xcrun stapler validate "$file"
}

[[ -n "${MAC_INSTALLER_IDENTITY:-}" ]] && notarize "$PKG"

# .dmg with the installer + read me
mkdir -p "$WORK/dmg"
cp "$PKG" "$WORK/dmg/Install $NAME.pkg"
cp "$ROOT/Packaging/README-USER.txt" "$WORK/dmg/Read Me.txt"
# hdiutil sometimes fails with "Resource busy" right after other disk activity -> retry
DMG="$OUT/TrapWin-Timer-$VERSION-macOS.dmg"
for attempt in 1 2 3 4 5; do
    if hdiutil create -volname "$NAME $VERSION" -srcfolder "$WORK/dmg" -ov -format UDZO "$DMG"; then break; fi
    [[ $attempt == 5 ]] && { echo "hdiutil failed"; exit 1; }
    echo "hdiutil busy, retrying ($attempt)"; sleep $((attempt * 5))
done
if [[ -n "${MAC_APP_IDENTITY:-}" ]]; then
    codesign --force --timestamp --sign "$MAC_APP_IDENTITY" "$DMG"
    notarize "$DMG"
    spctl -a -vv -t install "$PKG"
    spctl -a -vv -t open --context context:primary-signature "$DMG"
    SIGNED="signed + notarized"
else
    SIGNED="UNSIGNED test build"
fi
echo "done ($SIGNED): $PKG and $DMG"
