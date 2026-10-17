#!/bin/sh
set -eu

# L02: registers this unpacked copy of Völund CAD for the current user: the desktop entry
# (Exec pointing here) and the .volund MIME type under XDG_DATA_HOME, without root.
# uninstall-user.sh removes them again.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
product="$here/bin/voelund-cad"
data=${XDG_DATA_HOME:-$HOME/.local/share}
entry=io.github.quompacc.Voelund.desktop

if [ ! -x "$product" ]; then
    echo "voelund-cad not found beside this script: $product" >&2
    exit 1
fi
case "$product" in
    *'"'* | *'\'* | *'`'* | *'$'*)
        echo "The folder name must not contain quotes, backslashes, \` or \$: $here" >&2
        exit 1
        ;;
esac

mkdir -p "$data/applications" "$data/mime/packages"
sed "s|^Exec=.*|Exec=\"$product\" %F|" "$here/share/applications/$entry" \
    >"$data/applications/$entry"
cp "$here/share/mime/packages/io.github.quompacc.Voelund.xml" "$data/mime/packages/"
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$data/mime"
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$data/applications"
fi
if command -v xdg-mime >/dev/null 2>&1; then
    XDG_DATA_HOME="$data" xdg-mime default "$entry" application/x-voelund
fi
echo "Registered .volund for $(id -un) with $product"
