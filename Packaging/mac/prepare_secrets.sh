#!/usr/bin/env bash
# Run on your Mac after installing the two Developer ID certificates.
# Prints the values for the GitHub secrets (Settings > Secrets and variables > Actions).
#
#   Packaging/mac/prepare_secrets.sh ~/Desktop/trapwin-certs.p12
#
# Export the .p12 first: Keychain Access > My Certificates > select BOTH
# "Developer ID Application: ..." and "Developer ID Installer: ..." > right-click > Export 2 items.
set -euo pipefail
P12="${1:?path to the exported .p12}"

echo "== Signing identities found in your keychain =="
security find-identity -v | grep -E "Developer ID (Application|Installer)" || {
    echo "No Developer ID certificates found - create them at developer.apple.com > Certificates."; exit 1; }

APP=$(security find-identity -v | sed -nE 's/.*"(Developer ID Application: [^"]+)".*/\1/p' | head -1)
INS=$(security find-identity -v | sed -nE 's/.*"(Developer ID Installer: [^"]+)".*/\1/p' | head -1)
TEAM=$(sed -nE 's/.*\(([A-Z0-9]{10})\)$/\1/p' <<<"$APP")

base64 -i "$P12" | pbcopy
cat <<MSG

Create these GitHub secrets:
  MAC_CERTS_P12          (already in your clipboard - paste it)
  MAC_CERTS_PASSWORD     the password you chose when exporting the .p12
  MAC_APP_IDENTITY       $APP
  MAC_INSTALLER_IDENTITY $INS
  MAC_TEAM_ID            $TEAM
  MAC_APPLE_ID           your Apple ID e-mail
  MAC_APP_PASSWORD       app-specific password (appleid.apple.com > Sign-In and Security)
MSG
