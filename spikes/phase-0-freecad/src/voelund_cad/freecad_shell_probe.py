from __future__ import annotations

import json
import statistics
import time
from pathlib import Path
from typing import Any

import FreeCAD as App
import FreeCADGui as Gui
from PySide6 import QtCore, QtGui, QtTest, QtWidgets

from .freecad_adapter import FreeCadAdapter


def launch_shell_probe(output_dir: Path) -> None:
    """Launch a disposable Völund-owned window around FreeCAD's native view."""

    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "shell-report.json"
    screenshot_path = output_dir / "voelund-shell.png"
    app = QtWidgets.QApplication.instance()
    adapter = FreeCadAdapter()

    try:
        snapshot = adapter.create_model()
        App.ActiveDocument.getObject("Sketch").ViewObject.Visibility = False
        App.ActiveDocument.getObject("Extrusion").ViewObject.Visibility = False
        App.ActiveDocument.getObject("Fillet").ViewObject.Visibility = True
        Gui.activeDocument().activeView().setNavigationType("Gui::CADNavigationStyle")
        Gui.activeDocument().activeView().viewAxonometric()
        Gui.activeDocument().activeView().fitAll()
        Gui.updateGui()

        freecad_main = Gui.getMainWindow()
        mdi = freecad_main.findChild(QtWidgets.QMdiArea)
        if mdi is None or mdi.activeSubWindow() is None:
            raise RuntimeError("FreeCAD did not expose an active MDI viewport widget")
        subwindow = mdi.activeSubWindow()
        viewport_widget = subwindow.widget()
        if viewport_widget is None:
            raise RuntimeError("The active FreeCAD MDI subwindow has no widget")

        mdi.removeSubWindow(viewport_widget)
        viewport_widget.setParent(None)

        shell = QtWidgets.QMainWindow()
        shell.setObjectName("VoelundPhase0Shell")
        shell.setWindowTitle("Völund CAD — Phase 0")
        shell.resize(1200, 780)
        shell.setStyleSheet(
            "QMainWindow, QWidget { background: #151916; color: #e7e2d8; }"
            "QFrame#rail, QFrame#properties, QFrame#timeline { background: #1d231f; }"
            "QLabel#section { color: #c8a35c; font-weight: 600; }"
        )

        root = QtWidgets.QWidget()
        outer = QtWidgets.QVBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        body = QtWidgets.QWidget()
        body_layout = QtWidgets.QHBoxLayout(body)
        body_layout.setContentsMargins(0, 0, 0, 0)
        body_layout.setSpacing(1)

        rail = _panel("rail", "MODELL", "Sketch\nExtrusion\nFillet", 190)
        properties = _panel("properties", "EIGENSCHAFTEN", "Tiefe  20 mm\nRadius  2 mm", 220)
        body_layout.addWidget(rail)
        body_layout.addWidget(viewport_widget, 1)
        body_layout.addWidget(properties)

        timeline = QtWidgets.QFrame()
        timeline.setObjectName("timeline")
        timeline.setFixedHeight(92)
        timeline_layout = QtWidgets.QVBoxLayout(timeline)
        title = QtWidgets.QLabel("FEATURE TIMELINE")
        title.setObjectName("section")
        items = QtWidgets.QLabel("01  Sketch     02  Extrusion     03  Fillet")
        timeline_layout.addWidget(title)
        timeline_layout.addWidget(items)

        outer.addWidget(body, 1)
        outer.addWidget(timeline)
        shell.setCentralWidget(root)

        freecad_main.hide()
        shell.show()
        shell.raise_()

        def finish() -> None:
            view = Gui.activeDocument().activeView()
            global_center = viewport_widget.mapToGlobal(viewport_widget.rect().center())
            interaction_target = QtWidgets.QApplication.widgetAt(global_center) or viewport_widget
            local_center = interaction_target.mapFromGlobal(global_center)

            widget_inventory = sorted(
                (
                    {
                        "class": type(widget).__name__,
                        "object_name": widget.objectName(),
                        "visible": bool(widget.isVisible()),
                        "width": widget.width(),
                        "height": widget.height(),
                    }
                    for widget in [viewport_widget, *viewport_widget.findChildren(QtWidgets.QWidget)]
                ),
                key=lambda item: item["width"] * item["height"],
                reverse=True,
            )[:12]

            Gui.Selection.clearSelection()
            observer = _SelectionObserver()
            Gui.Selection.addObserver(observer)
            window_handle = shell.windowHandle()
            window_position = shell.mapFromGlobal(global_center)
            outside_position = shell.mapFromGlobal(
                viewport_widget.mapToGlobal(QtCore.QPoint(12, viewport_widget.height() - 12))
            )
            QtTest.QTest.mouseMove(window_handle, outside_position, delay=25)
            for step_index in range(1, 7):
                fraction = step_index / 6.0
                point = QtCore.QPoint(
                    round(outside_position.x() + (window_position.x() - outside_position.x()) * fraction),
                    round(outside_position.y() + (window_position.y() - outside_position.y()) * fraction),
                )
                QtTest.QTest.mouseMove(window_handle, point, delay=20)
            app.processEvents()
            Gui.updateGui()
            try:
                preselection_object = Gui.Selection.getPreselection()
                preselection = {
                    "repr": repr(preselection_object),
                    "document": getattr(preselection_object, "DocumentName", ""),
                    "object": getattr(preselection_object, "ObjectName", ""),
                    "subelements": tuple(getattr(preselection_object, "SubElementNames", ())),
                }
            except Exception as exc:
                preselection = {"error": f"{type(exc).__name__}: {exc}"}
            preselection["observer_events"] = observer.events
            preselection["pass"] = any(event[1] == "Fillet" and event[2] for event in observer.events)
            Gui.Selection.removeObserver(observer)

            click_started = time.perf_counter()
            QtTest.QTest.mouseClick(
                interaction_target,
                QtCore.Qt.MouseButton.LeftButton,
                QtCore.Qt.KeyboardModifier.NoModifier,
                local_center,
                delay=25,
            )
            app.processEvents()
            Gui.updateGui()
            click_latency_ms = (time.perf_counter() - click_started) * 1000
            selected = tuple(obj.Name for obj in Gui.Selection.getSelection())

            zoom_samples = []
            camera_before_zoom = view.getCamera()
            for _ in range(10):
                started = time.perf_counter()
                QtTest.QTest.wheelEvent(
                    window_handle,
                    QtCore.QPointF(window_position),
                    QtCore.QPoint(0, 120),
                )
                app.processEvents()
                Gui.updateGui()
                zoom_samples.append((time.perf_counter() - started) * 1000)
            camera_after_zoom = view.getCamera()

            view.setNavigationType("Gui::GestureNavigationStyle")
            camera_before_orbit = camera_after_zoom
            orbit_started = time.perf_counter()
            QtTest.QTest.mousePress(window_handle, QtCore.Qt.MouseButton.LeftButton, pos=window_position)
            for step_index in range(1, 10):
                QtTest.QTest.mouseMove(
                    window_handle,
                    window_position + QtCore.QPoint(step_index * 10, step_index * 5),
                    delay=15,
                )
            QtTest.QTest.mouseRelease(
                window_handle,
                QtCore.Qt.MouseButton.LeftButton,
                pos=window_position + QtCore.QPoint(90, 45),
            )
            app.processEvents()
            Gui.updateGui()
            orbit_latency_ms = (time.perf_counter() - orbit_started) * 1000
            camera_after_orbit = view.getCamera()

            camera_before_pan = camera_after_orbit
            pan_started = time.perf_counter()
            QtTest.QTest.mousePress(window_handle, QtCore.Qt.MouseButton.RightButton, pos=window_position)
            for step_index in range(1, 8):
                QtTest.QTest.mouseMove(
                    window_handle,
                    window_position + QtCore.QPoint(step_index * 10, step_index * -5),
                    delay=15,
                )
            QtTest.QTest.mouseRelease(
                window_handle,
                QtCore.Qt.MouseButton.RightButton,
                QtCore.Qt.KeyboardModifier.NoModifier,
                pos=window_position + QtCore.QPoint(70, -35),
            )
            app.processEvents()
            Gui.updateGui()
            pan_latency_ms = (time.perf_counter() - pan_started) * 1000
            camera_after_pan = view.getCamera()

            screen = shell.screen() or QtGui.QGuiApplication.primaryScreen()
            pixmap: QtGui.QPixmap = screen.grabWindow(int(shell.winId()))
            screenshot_ok = pixmap.save(str(screenshot_path.resolve()), "PNG")
            interaction = {
                "target_class": type(interaction_target).__name__,
                "target_object_name": interaction_target.objectName(),
                "navigation_type": view.getNavigationType(),
                "preselection": preselection,
                "picking_selected": selected,
                "picking_pass": bool(selected),
                "click_latency_ms": round(click_latency_ms, 3),
                "zoom_pass": camera_before_zoom != camera_after_zoom,
                "zoom_event_median_ms": round(statistics.median(zoom_samples), 3),
                "zoom_event_max_ms": round(max(zoom_samples), 3),
                "orbit_pass": camera_before_orbit != camera_after_orbit,
                "orbit_latency_ms": round(orbit_latency_ms, 3),
                "pan_pass": camera_before_pan != camera_after_pan,
                "pan_latency_ms": round(pan_latency_ms, 3),
            }
            interactive_pass = all(
                interaction[key] for key in ("picking_pass", "zoom_pass", "orbit_pass", "pan_pass")
            ) and bool(preselection.get("pass"))
            report: dict[str, Any] = {
                "probe": "voelund-owned-qt-shell",
                "status": "CONDITIONAL" if screenshot_ok and interactive_pass else "PARTIAL",
                "freecad_main_visible": bool(freecad_main.isVisible()),
                "shell_visible": bool(shell.isVisible()),
                "shell_class": type(shell).__name__,
                "viewport_widget_class": type(viewport_widget).__name__,
                "viewport_parent_class": type(viewport_widget.parent()).__name__,
                "interaction": interaction,
                "widget_inventory": widget_inventory,
                "screenshot_created": screenshot_ok and screenshot_path.exists(),
                "model": {
                    "objects": snapshot.object_names,
                    "volume_mm3": snapshot.volume_mm3,
                },
                "coupling_note": (
                    "The FreeCAD main window and MDI view are still used as a bootstrap factory; "
                    "the native view is then reparented into the Völund-owned window."
                ),
            }
            report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
            shell.close()
            adapter.close()
            QtCore.QTimer.singleShot(100, app.quit)

        QtCore.QTimer.singleShot(700, finish)
    except Exception as exc:
        report_path.write_text(
            json.dumps(
                {
                    "probe": "voelund-owned-qt-shell",
                    "status": "FAIL",
                    "error": {"type": type(exc).__name__, "message": str(exc)},
                },
                indent=2,
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        adapter.close()
        QtCore.QTimer.singleShot(100, app.quit)


def _panel(object_name: str, title_text: str, content_text: str, width: int) -> QtWidgets.QFrame:
    panel = QtWidgets.QFrame()
    panel.setObjectName(object_name)
    panel.setFixedWidth(width)
    layout = QtWidgets.QVBoxLayout(panel)
    layout.setContentsMargins(18, 20, 18, 20)
    title = QtWidgets.QLabel(title_text)
    title.setObjectName("section")
    content = QtWidgets.QLabel(content_text)
    content.setAlignment(QtCore.Qt.AlignmentFlag.AlignTop)
    layout.addWidget(title)
    layout.addSpacing(12)
    layout.addWidget(content, 1)
    return panel


class _SelectionObserver:
    def __init__(self) -> None:
        self.events: list[tuple[str, str, str]] = []

    def setPreselection(self, document: str, object_name: str, subelement: str) -> None:
        self.events.append((document, object_name, subelement))
