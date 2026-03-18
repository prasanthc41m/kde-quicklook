#!/bin/bash
# dolphin-quicklook.sh
# Gets the currently selected file in Dolphin and opens it in kde-quicklook.
# Bind this script to a keyboard shortcut (e.g. Shift+Space) via:
#   System Settings → Shortcuts → Custom Shortcuts

# Dynamically find the Dolphin DBus service (PID is appended to the name)
DOLPHIN_SERVICE=$(qdbus | grep "org.kde.dolphin" | head -1 | tr -d ' ')

if [ -z "$DOLPHIN_SERVICE" ]; then
    notify-send "QuickLook" "Dolphin is not running." 2>/dev/null
    exit 1
fi

# Use Dolphin's copy_location action to push the selected file path to clipboard
qdbus "$DOLPHIN_SERVICE" /dolphin/Dolphin_1 org.kde.KMainWindow.activateAction "copy_location" 2>/dev/null
sleep 0.3

# Read the file path from the Wayland clipboard
FILE=$(wl-paste 2>/dev/null | head -1 | tr -d '\n')

if [ -z "$FILE" ]; then
    notify-send "QuickLook" "No file selected in Dolphin." 2>/dev/null
    exit 1
fi

if [ ! -f "$FILE" ]; then
    notify-send "QuickLook" "Cannot preview: $FILE" 2>/dev/null
    exit 1
fi

# Launch the preview window
kde-quicklook "$FILE"
