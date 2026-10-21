#!/bin/bash
# Installs Bodenleger for the current user and clears the download quarantine flag.
cd "$(dirname "$0")" || exit 1
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/Bodenleger.vst3" "$AU/Bodenleger.component"
cp -R Bodenleger.vst3 "$VST3/"
cp -R Bodenleger.component "$AU/"
xattr -dr com.apple.quarantine "$VST3/Bodenleger.vst3" "$AU/Bodenleger.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "Bodenleger is installed."
echo "Restart Ableton Live (or rescan plug-ins) and look for Bodenleger under Plug-Ins."
echo ""
read -n 1 -s -r -p "Press any key to close this window."
