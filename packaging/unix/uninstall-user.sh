#!/bin/sh
set -eu

# L02: removes what install-user.sh registered for the current user, but only when the desktop
# entry points to this copy.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
product="$here/bin/voelund-cad"
data=${XDG_DATA_HOME:-$HOME/.local/share}
entry="$data/applications/io.github.quompacc.Voelund.desktop"

if [ ! -f "$entry" ]; then
    echo "No .volund registration found"
    exit 0
fi
if ! grep -qF "Exec=\"$product\" %F" "$entry"; then
    echo "The .volund registration belongs to another copy; left unchanged"
    exit 0
fi
rm -f "$entry" "$data/mime/packages/io.github.quompacc.Voelund.xml"
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$data/mime"
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$data/applications"
fi
echo "Removed the .volund registration"
