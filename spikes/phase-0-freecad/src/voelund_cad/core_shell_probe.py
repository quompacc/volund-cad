from __future__ import annotations

import ctypes
import json
import statistics
import sys
import time
from pathlib import Path
from typing import Any

from PySide6 import QtCore, QtGui, QtTest, QtWidgets

from .core_viewport import CoreViewport
from .freecad_adapter import FreeCadAdapter


def launch_core_shell_probe(output_dir: Path) -> int:
    """Run FreeCAD's modeling core behind a Völund-owned UI and viewport."""

    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "core-shell-report.json"
    screenshot_path = output_dir / "voelund-core-shell.png"
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    adapter = FreeCadAdapter()
    started = time.perf_counter()

    try:
        initial = adapter.create_model()
        initial_mesh = adapter.display_mesh()
        shell = _create_shell()
        viewport = CoreViewport()
        shell.centralWidget().layout().itemAt(0).widget().layout().insertWidget(1, viewport, 1)
        viewport.set_mesh(initial_mesh, fit=True)
        shell.show()
        shell.raise_()

        result: dict[str, Any] = {}

        def finish() -> None:
            try:
                app.processEvents()
                viewport.viewAll()
                app.processEvents()
                hit_point = viewport.find_hit_point()

                preselection_events: list[bool] = []
                selection_events: list[bool] = []
                viewport.preselection_changed.connect(preselection_events.append)
                viewport.selection_changed.connect(selection_events.append)

                for point in (QtCore.QPoint(5, 5), hit_point):
                    move = QtGui.QMouseEvent(
                        QtCore.QEvent.Type.MouseMove,
                        QtCore.QPointF(point),
                        QtCore.QPointF(viewport.mapToGlobal(point)),
                        QtCore.Qt.MouseButton.NoButton,
                        QtCore.Qt.MouseButton.NoButton,
                        QtCore.Qt.KeyboardModifier.NoModifier,
                    )
                    QtWidgets.QApplication.sendEvent(viewport, move)
                app.processEvents()

                pick_started = time.perf_counter()
                QtTest.QTest.mouseClick(viewport, QtCore.Qt.MouseButton.LeftButton, pos=hit_point, delay=20)
                app.processEvents()
                pick_latency = (time.perf_counter() - pick_started) * 1000

                transform_before_zoom = viewport.transform_signature()
                zoom_samples = []
                for _ in range(8):
                    wheel_started = time.perf_counter()
                    wheel = QtGui.QWheelEvent(
                        QtCore.QPointF(hit_point),
                        QtCore.QPointF(viewport.mapToGlobal(hit_point)),
                        QtCore.QPoint(),
                        QtCore.QPoint(0, 120),
                        QtCore.Qt.MouseButton.NoButton,
                        QtCore.Qt.KeyboardModifier.NoModifier,
                        QtCore.Qt.ScrollPhase.ScrollUpdate,
                        False,
                    )
                    QtWidgets.QApplication.sendEvent(viewport, wheel)
                    app.processEvents()
                    zoom_samples.append((time.perf_counter() - wheel_started) * 1000)
                transform_after_zoom = viewport.transform_signature()

                transform_before_orbit = transform_after_zoom
                orbit_started = time.perf_counter()
                QtTest.QTest.mousePress(viewport, QtCore.Qt.MouseButton.LeftButton, pos=hit_point)
                for step in range(1, 9):
                    QtTest.QTest.mouseMove(viewport, hit_point + QtCore.QPoint(step * 8, step * 4), delay=8)
                QtTest.QTest.mouseRelease(
                    viewport, QtCore.Qt.MouseButton.LeftButton, pos=hit_point + QtCore.QPoint(64, 32)
                )
                app.processEvents()
                orbit_latency = (time.perf_counter() - orbit_started) * 1000
                transform_after_orbit = viewport.transform_signature()

                transform_before_pan = transform_after_orbit
                pan_started = time.perf_counter()
                QtTest.QTest.mousePress(viewport, QtCore.Qt.MouseButton.RightButton, pos=hit_point)
                for step in range(1, 7):
                    QtTest.QTest.mouseMove(viewport, hit_point + QtCore.QPoint(step * 7, -step * 3), delay=8)
                QtTest.QTest.mouseRelease(
                    viewport, QtCore.Qt.MouseButton.RightButton, pos=hit_point + QtCore.QPoint(42, -18)
                )
                app.processEvents()
                pan_latency = (time.perf_counter() - pan_started) * 1000
                transform_after_pan = viewport.transform_signature()

                changed = adapter.set_extrusion_length(20.0)
                changed_mesh = adapter.display_mesh()
                viewport.set_mesh(changed_mesh)
                viewport.reset_view()
                app.processEvents()
                QtTest.QTest.qWait(120)

                screen = shell.screen() or QtGui.QGuiApplication.primaryScreen()
                screenshot_ok = screen.grabWindow(int(shell.winId())).save(str(screenshot_path.resolve()), "PNG")
                gui_module_loaded = "FreeCADGui" in sys.modules
                gui_dll_loaded = _windows_module_loaded("FreeCADGui.dll")
                model_update_pass = changed.volume_mm3 != initial.volume_mm3 and changed_mesh != initial_mesh
                interactions = {
                    "preselection_pass": True in preselection_events,
                    "picking_pass": True in selection_events,
                    "zoom_pass": transform_before_zoom != transform_after_zoom,
                    "orbit_pass": transform_before_orbit != transform_after_orbit,
                    "pan_pass": transform_before_pan != transform_after_pan,
                    "event_counts": viewport.event_counts,
                    "picking_latency_ms": round(pick_latency, 3),
                    "zoom_event_median_ms": round(statistics.median(zoom_samples), 3),
                    "orbit_sequence_ms": round(orbit_latency, 3),
                    "pan_sequence_ms": round(pan_latency, 3),
                }
                functional_pass = all(
                    interactions[key]
                    for key in ("preselection_pass", "picking_pass", "zoom_pass", "orbit_pass", "pan_pass")
                )
                result.update(
                    {
                        "probe": "voelund-core-only-shell",
                        "status": "PASS"
                        if functional_pass
                        and model_update_pass
                        and screenshot_ok
                        and not gui_module_loaded
                        and not gui_dll_loaded
                        else "FAIL",
                        "process_executable": Path(sys.executable).name,
                        "freecad_executable_used": False,
                        "freecad_gui_python_module_loaded": gui_module_loaded,
                        "freecad_gui_dll_loaded": gui_dll_loaded,
                        "classic_freecad_main_window_created": False,
                        "ui_owner": "Voelund",
                        "viewport_owner": "Voelund CoreViewport (Coin3D/Quarter)",
                        "interactions": interactions,
                        "model_update": {
                            "initial_depth_mm": initial.extrusion_length_mm,
                            "changed_depth_mm": changed.extrusion_length_mm,
                            "initial_volume_mm3": initial.volume_mm3,
                            "changed_volume_mm3": changed.volume_mm3,
                            "initial_triangles": len(initial_mesh.triangles),
                            "changed_triangles": len(changed_mesh.triangles),
                            "mesh_changed": changed_mesh != initial_mesh,
                            "pass": model_update_pass,
                        },
                        "screenshot_created": screenshot_ok and screenshot_path.exists(),
                        "startup_to_report_ms": round((time.perf_counter() - started) * 1000, 3),
                    }
                )
            except Exception as exc:
                result.update(
                    {
                        "probe": "voelund-core-only-shell",
                        "status": "FAIL",
                        "error": {"type": type(exc).__name__, "message": str(exc)},
                    }
                )
            finally:
                report_path.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
                shell.close()
                adapter.close()
                QtCore.QTimer.singleShot(100, app.quit)

        QtCore.QTimer.singleShot(700, finish)
        app.exec()
    except Exception as exc:
        result = {
            "probe": "voelund-core-only-shell",
            "status": "FAIL",
            "error": {"type": type(exc).__name__, "message": str(exc)},
        }
        report_path.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
        adapter.close()

    return 0 if result.get("status") == "PASS" else 1


def _create_shell() -> QtWidgets.QMainWindow:
    shell = QtWidgets.QMainWindow()
    shell.setObjectName("VoelundCoreShell")
    shell.setWindowTitle("Völund CAD — Core-only Host")
    shell.resize(1200, 780)
    shell.setStyleSheet(
        "QMainWindow, QWidget { background: #151916; color: #e7e2d8; }"
        "QFrame { background: #1d231f; }"
        "QLabel#section { color: #c8a35c; font-weight: 600; }"
    )
    root = QtWidgets.QWidget()
    outer = QtWidgets.QVBoxLayout(root)
    outer.setContentsMargins(0, 0, 0, 0)
    outer.setSpacing(1)
    body = QtWidgets.QWidget()
    body_layout = QtWidgets.QHBoxLayout(body)
    body_layout.setContentsMargins(0, 0, 0, 0)
    body_layout.setSpacing(1)
    body_layout.addWidget(_panel("MODELL", "Sketch\nExtrusion\nFillet", 190))
    body_layout.addWidget(_panel("EIGENSCHAFTEN", "Tiefe  20 mm\nRadius  2 mm", 220))
    timeline = _panel("FEATURE TIMELINE", "01  Sketch     02  Extrusion     03  Fillet", 0)
    timeline.setFixedHeight(92)
    outer.addWidget(body, 1)
    outer.addWidget(timeline)
    shell.setCentralWidget(root)
    return shell


def _panel(title_text: str, content_text: str, width: int) -> QtWidgets.QFrame:
    panel = QtWidgets.QFrame()
    if width:
        panel.setFixedWidth(width)
    layout = QtWidgets.QVBoxLayout(panel)
    title = QtWidgets.QLabel(title_text)
    title.setObjectName("section")
    layout.addWidget(title)
    layout.addWidget(QtWidgets.QLabel(content_text), 1)
    return panel


def _windows_module_loaded(module_name: str) -> bool:
    if sys.platform != "win32":
        return False
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    kernel32.GetModuleHandleW.restype = ctypes.c_void_p
    return bool(kernel32.GetModuleHandleW(module_name))
