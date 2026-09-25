#!/bin/bash
# Builds an Unleashed Recompiled IPA on macOS in one go.
#
# Usage: ./build_ios.sh "/path/to/your game files"
#
# The folder must contain the base game's default.xex and shader.ar, and the title update's default.xexp,
# anywhere inside it. Optional environment variables:
#   TEAM_ID=XXXXXXXXXX     Apple development team, detected from your signing certificate if not set.
#   BUNDLE_ID=com.x.y      Bundle identifier, derived from the team if not set.
#   EXTENDED_MEMORY=OFF    Build without the extended memory entitlements if your account can't sign them.

set -euo pipefail

GAME_DIR="${1:-}"
REPO="$(cd "$(dirname "$0")" && pwd)"
PRIVATE_DIR="$REPO/UnleashedRecompLib/private"
MAC_PRESET="macos-release"
IOS_PRESET="ios-xcode-release"
MAC_BUILD="$REPO/out/build/$MAC_PRESET"
IOS_BUILD="$REPO/out/build/$IOS_PRESET"
ARCHIVE="$REPO/out/UnleashedRecomp.xcarchive"
IPA_DIR="$REPO/out/ipa"

step() { printf '\n\033[1;34m==> %s\033[0m\n' "$1"; }
fail() { printf '\n\033[1;31mError: %s\033[0m\n' "$1" >&2; exit 1; }

[[ "$(uname)" == "Darwin" ]] || fail "This script has to run on a Mac."
[[ -n "$GAME_DIR" ]] || fail "Usage: ./build_ios.sh \"/path/to/your game files\""
[[ -d "$GAME_DIR" ]] || fail "Folder not found: $GAME_DIR"

for tool in cmake ninja xcodebuild git; do
    command -v "$tool" >/dev/null || fail "$tool is missing. Install Xcode, then run: brew install cmake ninja pkg-config"
done

cd "$REPO"

step "Updating submodules"
git submodule sync --recursive
git submodule update --init --recursive

step "Finding the game files in $GAME_DIR"
# Skip files this project generates, so a folder that was already set up by the game still works.
find_file() {
    find "$GAME_DIR" -type f -iname "$1" ! -path "*/patched/*" ! -iname "*_patched*" 2>/dev/null | head -n 1
}

XEX="$(find_file default.xex)"
XEXP="$(find_file default.xexp)"
[[ -n "$XEX" ]] || fail "default.xex not found. It's in the base game's main folder."
[[ -n "$XEXP" ]] || fail "default.xexp not found. It comes from the title update, which must be extracted (not the raw update package)."

# shader.ar sits next to the base game's default.xex.
SHADER="$(dirname "$XEX")/shader.ar"
[[ -f "$SHADER" ]] || SHADER="$(find_file shader.ar)"
[[ -n "$SHADER" && -f "$SHADER" ]] || fail "shader.ar not found. It's in the base game's main folder, next to default.xex."

echo "  default.xex:  $XEX"
echo "  default.xexp: $XEXP"
echo "  shader.ar:    $SHADER"

mkdir -p "$PRIVATE_DIR"
cp "$XEX" "$PRIVATE_DIR/default.xex"
cp "$XEXP" "$PRIVATE_DIR/default.xexp"
cp "$SHADER" "$PRIVATE_DIR/shader.ar"

step "Finding your Apple development team"
if [[ -z "${TEAM_ID:-}" ]]; then
    TEAM_ID="$(security find-certificate -a -c "Apple Development" -p 2>/dev/null \
        | openssl x509 -noout -subject 2>/dev/null \
        | sed -n 's/.*OU *= *\([A-Z0-9]\{10\}\).*/\1/p' | head -n 1 || true)"
fi
[[ -n "$TEAM_ID" ]] || fail "No Apple Development certificate found. Open Xcode > Settings > Accounts, sign in with your Apple ID, click Manage Certificates and add an Apple Development certificate. Then run this again, or pass TEAM_ID=XXXXXXXXXX."
BUNDLE_ID="${BUNDLE_ID:-com.$(echo "$TEAM_ID" | tr '[:upper:]' '[:lower:]').unleashedrecomp}"
echo "  Team:      $TEAM_ID"
echo "  Bundle ID: $BUNDLE_ID"

# The iOS build can't run the recompilers itself, so generate the recompiled code, shaders and resources with a macOS build first.
step "Recompiling the game code and shaders on the Mac (this takes a while)"
cmake --preset "$MAC_PRESET"

GENERATED=()
while IFS= read -r target; do
    GENERATED+=("$target")
done < <(ninja -C "$MAC_BUILD" -t targets all \
    | sed -n 's/^\(.*\): CUSTOM_COMMAND$/\1/p' \
    | grep -E '\.(c|cpp|h)$' \
    | grep -F "$REPO/" || true)

[[ ${#GENERATED[@]} -gt 0 ]] || fail "Couldn't find the code generation steps in the macOS build."
ninja -C "$MAC_BUILD" "${GENERATED[@]}"

# The macOS pass compiles the Metal shaders for macOS, which an iPhone can't load. Recompile them for iOS
# and embed those instead, as the iOS build reuses whatever embedded shaders already exist.
step "Compiling the Metal shaders for iOS"
FILE_TO_C="$(find "$MAC_BUILD" -type f -name file_to_c -perm -u+x | head -n 1)"
[[ -n "$FILE_TO_C" ]] || fail "file_to_c wasn't built by the macOS pass."

for embedded in "$REPO"/UnleashedRecomp/gpu/shader/msl/*.metallib.c; do
    [[ -e "$embedded" ]] || continue

    metallib="${embedded%.c}"
    source="${metallib%.metallib}"
    name="$(basename "$source" .metal)"
    ir="$source.ios.ir"

    if ! xcrun -sdk iphoneos metal -mios-version-min=15.0 -o "$ir" -c "$source" -D__air__ -frecord-sources -gline-tables-only 2>/dev/null; then
        xcrun -sdk iphoneos metal -o "$ir" -c "$source" -D__air__ -frecord-sources -gline-tables-only
    fi

    xcrun -sdk iphoneos metallib -o "$metallib" "$ir"
    rm -f "$ir"
    "$FILE_TO_C" "$metallib" "g_${name}_air" none "$metallib.c" "$metallib.h"
    echo "  $name"
done

step "Configuring the iOS project"
cmake --preset "$IOS_PRESET" \
    -DCMAKE_XCODE_GENERATE_SCHEME=ON \
    -DUNLEASHED_RECOMP_IOS_DEVELOPMENT_TEAM="$TEAM_ID" \
    -DUNLEASHED_RECOMP_IOS_BUNDLE_ID="$BUNDLE_ID" \
    -DUNLEASHED_RECOMP_IOS_EXTENDED_MEMORY="${EXTENDED_MEMORY:-ON}"

archive_app() {
    rm -rf "$ARCHIVE"
    xcodebuild \
        -project "$IOS_BUILD/UnleashedRecomp.xcodeproj" \
        -scheme UnleashedRecomp \
        -configuration Release \
        -destination "generic/platform=iOS" \
        -archivePath "$ARCHIVE" \
        -allowProvisioningUpdates \
        archive
}

step "Building and archiving the app (the first build can take over an hour)"
if ! archive_app; then
    # Free Apple accounts can't sign the extended memory entitlements. Retry without them, unless they were asked for explicitly.
    [[ -z "${EXTENDED_MEMORY:-}" ]] || fail "The build failed. Check the first error above."

    step "Build failed, retrying without the extended memory entitlements"
    cmake --preset "$IOS_PRESET" -DUNLEASHED_RECOMP_IOS_EXTENDED_MEMORY=OFF
    archive_app || fail "The build failed. Check the first error above."
    echo "  Built without the extended memory entitlements. Older and 4 GB devices may crash at launch."
fi

step "Exporting the IPA"
EXPORT_OPTIONS="$REPO/out/ExportOptions.plist"
cat > "$EXPORT_OPTIONS" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key>
    <string>development</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>teamID</key>
    <string>$TEAM_ID</string>
    <key>compileBitcode</key>
    <false/>
</dict>
</plist>
EOF

rm -rf "$IPA_DIR"
xcodebuild -exportArchive \
    -archivePath "$ARCHIVE" \
    -exportPath "$IPA_DIR" \
    -exportOptionsPlist "$EXPORT_OPTIONS" \
    -allowProvisioningUpdates

IPA="$(find "$IPA_DIR" -name "*.ipa" | head -n 1)"
[[ -n "$IPA" ]] || fail "The IPA wasn't exported. Check the messages above."

step "Done"
echo "  IPA: $IPA"
echo
echo "  The game files in UnleashedRecompLib/private are ignored by git, so they won't be committed."
echo "  To test, install the IPA on your iPhone, then copy your game, update and dlc folders"
echo "  into Files > On My iPhone > Unleashed > UnleashedRecomp."
