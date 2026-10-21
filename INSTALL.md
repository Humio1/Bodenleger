# Installing Bodenleger (macOS)

1. Double-click **Install Bodenleger.command**.
   macOS may say the file is from an unidentified developer. In that case right-click it, choose **Open**, then **Open** again.
2. Restart Ableton Live, or rescan plug-ins in Settings → Plug-Ins.
3. Put Bodenleger on a **return track**, send it some of your track, and set Mix to 100 % pad.

Manual install: copy `Bodenleger.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `Bodenleger.component` to
`~/Library/Audio/Plug-Ins/Components`, then run in Terminal:

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Bodenleger.vst3 ~/Library/Audio/Plug-Ins/Components/Bodenleger.component

This build is not yet signed with an Apple Developer ID, which is why macOS asks.
