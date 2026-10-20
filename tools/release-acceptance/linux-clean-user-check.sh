#!/bin/sh
# L08 Linux: the release tarball for a fresh user (empty HOME, private XDG dirs, minimal PATH),
# run under a private virtual KWin Wayland session (native Wayland QPA, no DISPLAY), 3200 x 2000
# so that the 1300 x 860 test window also fits at scale 2.
#
#   sh tools/release-acceptance/linux-clean-user-check.sh [<release-build-dir> [<output-dir>]]
#
# Needs the tarball (cpack) and the UI flow tests of the release build; results in
# <output-dir>/results.txt with smoke and ui-flow-scale-* pictures.
set -u
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=${1:-$repo/build/linux-desktop-release}
out=${2:-$HOME/l08-linux-out}
rm -rf "$out"; mkdir -p "$out"
log=$out/results.txt
note() { echo "$*" >> "$log"; }
note "L08 Linux run $(date -Iseconds)"
note "OS: $(. /etc/os-release; echo "$PRETTY_NAME") kernel $(uname -r)"
note "Commit: $(cd $repo && git log -1 --format=%h) + $(cd $repo && git status --short | wc -l) local changes"

home=$(mktemp -d /tmp/l08-home.XXXX)
mkdir -p "$home/Downloads"
tarball=$(ls "$build"/voelund-cad-*-Linux-x86_64.tar.gz | head -1)
tar xzf "$tarball" -C "$home/Downloads"
pkg="$home/Downloads/$(basename "$tarball" .tar.gz)"
cleanenv() {
  env -i HOME="$home" USER="$USER" LANG=de_DE.UTF-8 PATH=/usr/bin:/bin \
    XDG_DATA_HOME="$home/.local/share" XDG_CONFIG_HOME="$home/.config" XDG_CACHE_HOME="$home/.cache" \
    XDG_RUNTIME_DIR="$rt" WAYLAND_DISPLAY="$sock" QT_QPA_PLATFORM=wayland "$@"
}
rt=$(mktemp -d); chmod 700 "$rt"; sock=l08-$$
env -u DISPLAY -u WAYLAND_DISPLAY XDG_RUNTIME_DIR="$rt" setsid dbus-run-session -- kwin_wayland --virtual --socket "$sock" --width 3200 --height 2000 >/tmp/l08-kwin.log 2>&1 &
kp=$!; i=0; while [ ! -S "$rt/$sock" ] && [ $i -lt 50 ]; do i=$((i+1)); sleep 0.2; done

run() { name=$1; shift; start=$(date +%s.%N); cleanenv "$@" >"$out/$name.out.txt" 2>"$out/$name.err.txt"; code=$?
  note "$name : exit $code ($(echo "$(date +%s.%N) - $start" | bc | cut -c1-5) s)"; }

run install sh "$pkg/install-user.sh"
exe="$pkg/bin/voelund-cad"
note "Installed exe: $exe"
note "Desktop entry: $(ls "$home/.local/share/applications" 2>/dev/null)"
docs="$home/Dokumente/Prüf Ordner äöü"; mkdir -p "$docs"
cp $repo/tests/fixtures/documents/v09.volund "$docs/Teil mit Ä.volund"
cp $repo/tests/fixtures/documents/v16.volund "$docs/Älteres Teil.volund"
mkdir -p "$out/smoke"
run version "$exe" --version
run smoke-start env VOELUND_SMOKE_ARTIFACT_DIR="$out/smoke" "$exe" --smoke-test
run smoke-open-unicode env VOELUND_SMOKE_ARTIFACT_DIR="$out/smoke" "$exe" --smoke-test-open "$docs/Teil mit Ä.volund"
run check-unicode "$exe" --check-document "$docs/Teil mit Ä.volund"
run resave-old "$exe" --check-document "$docs/Älteres Teil.volund" --resave "$docs/Älteres Teil neu.volund"
run check-resaved "$exe" --check-document "$docs/Älteres Teil neu.volund"
note "MIME default: $(cleanenv xdg-mime query default application/x-voelund 2>&1)"
note "MIME of file (KDE): $(cleanenv env XDG_CURRENT_DESKTOP=KDE xdg-mime query filetype "$docs/Teil mit Ä.volund" 2>&1)"
note "MIME of file (gio): $(cleanenv gio info -a standard::content-type "$docs/Teil mit Ä.volund" 2>&1 | grep content-type)"
note "QPA check: $(grep -il xcb "$out"/smoke-start.err.txt >/dev/null && echo 'xcb mentioned' || echo 'no xcb')"

# The UI flows through the real shell against the package's libraries.
lib="$pkg/lib64"
for flow in voelund_reference_part_flow_tests voelund_reference_ring_flow_tests voelund_ui_flow_tests; do
  run "$flow" env LD_LIBRARY_PATH="$lib" $build/tests/$flow
done
for scale in 1 1.5 2; do
  mkdir -p "$out/ui-flow-scale-$scale"
  run "ui-flow-captures-$scale" env LD_LIBRARY_PATH="$lib" VOELUND_UI_FLOW_ARTIFACT_DIR="$out/ui-flow-scale-$scale" QT_SCALE_FACTOR=$scale $build/tests/voelund_ui_flow_tests
done
run uninstall sh "$pkg/uninstall-user.sh"
note "After uninstall: $(find "$home/.local" -iname '*voelund*' -not -path '*/mime/*' | wc -l) entries left"
kill -TERM -- "-$kp" 2>/dev/null; rm -rf "$rt" "$home"
note DONE
cat "$log"
