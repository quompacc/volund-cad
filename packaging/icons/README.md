# Programmsymbol (N16)

Eigene Gestaltung des Projekts: das Ambosszeichen der Kopfleiste
(`src/desktop/ShellIcons.cpp`) auf der Tintenfarbe des Themes. Es gehört zur Komponente
`voelund` (packaging/Components.cmake) und hat dieselbe, noch offene Lizenz.

- `voelund-cad.svg`: Quelle.
- `voelund-cad-256.png`: Linux `hicolor/256x256` und Fenstersymbol (Qt-Ressource).
- `voelund-cad.ico`: Windows (16, 24, 32, 48, 64, 256 px) in der EXE
  (`apps/voelund-cad/voelund-cad.rc`).

Erzeugt mit ImageMagick 7 auf Fedora:

```sh
for s in 16 24 32 48 64 256; do
    magick -background none -density 384 voelund-cad.svg -resize ${s}x${s} icon-$s.png
done
magick icon-16.png icon-24.png icon-32.png icon-48.png icon-64.png icon-256.png voelund-cad.ico
cp icon-256.png voelund-cad-256.png
```
