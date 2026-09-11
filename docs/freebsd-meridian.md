# FreeBSD and Meridian development target

## Purpose

FreeBSD with Meridian is Völund's primary desktop target. Windows remains the fast
development and regression environment, while the FreeBSD machine provides
authoritative graphics, Wayland, integration, and packaging evidence.

## Host prerequisites

Install the FreeBSD packages providing:

- Clang, CMake, Ninja, and pkgconf;
- Qt 6 Base including Widgets, OpenGLWidgets, and the native Wayland platform plugin;
- OpenCASCADE Technology 7.9.3 or newer (8.0.1 remains the Windows pin);
- Wayland, a session D-Bus, `xdg-desktop-portal`, and Meridian's portal backend.

The repository setup command is intentionally unprivileged:

```sh
./tools/setup-freebsd.sh
cmake --build --preset freebsd-clang
```

The stock FreeBSD OCCT package builds its EGL/GLES renderer with `HAVE_XLIB`, so it
cannot attach Qt's Wayland EGL context while `DISPLAY` is absent. It also brings Qt 5,
TBB, VTK, and import/export dependencies into the installed package. Völund therefore
uses a project-local OCCT build for the native desktop gate:

```sh
sh tools/build-freebsd-occt.sh
cmake --preset freebsd-clang-lean-occt
cmake --build --preset freebsd-clang-lean-occt
```

The script checks out the immutable OCCT 7.9.3 commit, builds the exact toolkit
dependency closure required by Völund, enables EGL/GLES, and disables Xlib, TBB, VTK,
Qt 5, Tcl/Tk, data exchange, samples, and documentation.
Völund's central OCCT compatibility header supplies the `occ::handle` spelling on
7.9, so application code stays identical across the packaged and pinned versions.

For the temporary Sway control desktop, copy the supplied session fragment and start
Sway from a local console login:

```sh
mkdir -p ~/.config/sway/config.d
cp packaging/freebsd/sway-control.conf ~/.config/sway/config.d/voelund.conf
sh tools/start-freebsd-control-session.sh
```

Inside Sway, `Super+Return` opens a terminal, `Super+d` opens the launcher, and
`Super+Shift+v` starts the locally built Völund executable.

## Two-stage validation

Run the same build first under a mature FreeBSD Wayland desktop and then under
Meridian. The first run is a control; only the Meridian run satisfies product
acceptance.

Inside either active Wayland session:

```sh
cmake --build --preset freebsd-clang-lean-occt
sh ./tools/run-freebsd-integration.sh control freebsd-clang-lean-occt
# Later, in the Meridian session:
sh ./tools/run-freebsd-integration.sh meridian freebsd-clang-lean-occt
```

The test removes `DISPLAY` and forces `QT_QPA_PLATFORM=wayland`. It waits up to ten
seconds for the Qt/OCCT renderer, enables an interactive CAD command, sends a harmless
viewport event, and exits. The executable also verifies the selected Qt platform and
returns exit code 2 if a requested Wayland run fell back to another QPA backend.

The Sway control run on 11 September 2026 passed on the Intel HD 620 with `DISPLAY`
removed. The stock Qt 6 package itself remains a combined X11/Wayland build and links
`libX11` and `libGLX` from `Qt6Gui`/`Qt6OpenGL`; the native gate proves these backends
are not selected, but eliminating the dormant linkage would require a separate
Wayland-only Qt package flavor.

The same control session was used for interactive checks of the persistent Connection
toolbar and body transforms. A Separate cylinder remains at its world position when
the input box moves. A joined box/cylinder result exposes Move from either contributing
feature and translates as one result body. Both behaviors are covered by the native
application regression suite in addition to the manual check.

## Integration checklist

- The Wayland application ID resolves to `io.github.quompacc.Voelund.desktop`.
- Meridian supplies server-side decorations through normal protocol negotiation.
- Qt follows Meridian's dark/light preference through the portal platform theme.
- `QFileDialog` uses the native portal path; code must not force a non-native dialog.
- `.volund` files resolve to `application/x-voelund` and open through `%F`.
- Clipboard, drag-and-drop, keyboard focus, pointer input, scale, maximize and resize
  work without application-specific compositor rules.
- Suspend/resume and compositor/session restart do not leave a stale OCCT context.

## Packaging

CMake installs the executable, desktop entry, and shared MIME description using GNU
install directories. A FreeBSD package or ports overlay should stage with
`cmake --install` and run `update-desktop-database` and `update-mime-database` through
the package framework. Branding can replace the temporary themed
`applications-engineering` icon after a Völund application icon is approved.
