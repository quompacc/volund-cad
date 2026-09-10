from __future__ import annotations

from pathlib import Path
from typing import Any

import FreeCAD as App
import FreeCADGui as Gui
import Part
from PySide6 import QtWidgets
from pivy import coin


def probe_gui(output_dir: Path) -> dict[str, Any]:
    """Inspect the FreeCAD GUI boundary without claiming viewport independence."""

    output_dir.mkdir(parents=True, exist_ok=True)
    doc = App.newDocument("VoelundGuiProbe")
    box = doc.addObject("Part::Box", "ProbeBox")
    box.Length = 40
    box.Width = 24
    box.Height = 12
    doc.recompute()

    gui_doc = Gui.activeDocument()
    view = gui_doc.activeView()
    view.viewAxonometric()
    view.fitAll()
    Gui.updateGui()
    screenshot = output_dir / "freecad-active-view.png"
    view.saveImage(str(screenshot.resolve()), 1000, 700, "Current")

    main_window = Gui.getMainWindow()
    scene_graph_probe: dict[str, Any]
    try:
        scene_graph = view.getSceneGraph()
        scene_graph_probe = {"available": True, "class": type(scene_graph).__name__}
    except Exception as exc:
        scene_graph_probe = {
            "available": False,
            "error_type": type(exc).__name__,
            "error": str(exc),
        }
    created_viewer_probe: dict[str, Any]
    try:
        created_viewer = Gui.createViewer()
        created_viewer_probe = {
            "available": True,
            "class": type(created_viewer).__name__,
        }
    except Exception as exc:
        created_viewer_probe = {
            "available": False,
            "error_type": type(exc).__name__,
            "error": str(exc),
        }
    capabilities = {
        "freecad_version": ".".join(App.Version()[:3]),
        "qt_binding": "PySide6",
        "qt_version": getattr(__import__("PySide6"), "__version__", "unknown"),
        "application_class": type(QtWidgets.QApplication.instance()).__name__,
        "main_window_class": type(main_window).__name__,
        "main_window_visible_during_probe": bool(main_window.isVisible()),
        "active_view_class": type(view).__name__,
        "scene_graph": scene_graph_probe,
        "coin_version": coin.SoDB.getVersion(),
        "create_viewer_api": hasattr(Gui, "createViewer"),
        "create_viewer_call": created_viewer_probe,
        "active_view_methods": {
            name: hasattr(view, name)
            for name in (
                "fitAll",
                "getCameraOrientation",
                "getSceneGraph",
                "getViewerType",
                "saveImage",
                "setCameraOrientation",
                "viewAxonometric",
            )
        },
        "navigation_related_api": sorted(
            name for name in dir(view) if "nav" in name.lower() or "viewer" in name.lower()
        ),
        "navigation_type": view.getNavigationType(),
        "navigation_types": view.listNavigationTypes(),
        "screenshot_created": screenshot.exists() and screenshot.stat().st_size > 0,
        "screenshot_path": str(screenshot),
        "solid_volume_mm3": float(box.Shape.Volume),
    }
    App.closeDocument(doc.Name)
    return capabilities
