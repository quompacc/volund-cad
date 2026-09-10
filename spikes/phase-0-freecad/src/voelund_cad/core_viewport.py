from __future__ import annotations

import sys

from PySide6 import QtCore, QtGui, QtOpenGLWidgets, QtWidgets

# Pivy 0.6.9's Qt compatibility package omits this Qt 6 module. Supplying the
# official PySide6 module keeps the workaround local and leaves the runtime untouched.
sys.modules.setdefault("pivy.qt.QtOpenGLWidgets", QtOpenGLWidgets)

from pivy import coin  # noqa: E402
from pivy.quarter import QuarterWidget  # noqa: E402

from .contracts import DisplayMesh


class CoreViewport(QuarterWidget):
    """Völund-owned Coin viewport with no dependency on FreeCADGui."""

    preselection_changed = QtCore.Signal(bool)
    selection_changed = QtCore.Signal(bool)

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("VoelundCoreViewport")
        self.setMouseTracking(True)
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.StrongFocus)
        self._drag_origin: QtCore.QPointF | None = None
        self._drag_button = QtCore.Qt.MouseButton.NoButton
        self._dragged = False
        self._preselected = False
        self._selected = False
        self._event_counts = {"move": 0, "pick": 0, "zoom": 0, "orbit": 0, "pan": 0}

        self._root = coin.SoSeparator()
        self._root.ref()
        self._camera = coin.SoPerspectiveCamera()
        self._light = coin.SoDirectionalLight()
        self._pan = coin.SoTranslation()
        self._scale = coin.SoScale()
        self._yaw = coin.SoRotationXYZ()
        self._yaw.axis = coin.SoRotationXYZ.Y
        self._pitch = coin.SoRotationXYZ()
        self._pitch.axis = coin.SoRotationXYZ.X
        self._center = coin.SoTranslation()
        self._material = coin.SoMaterial()
        self._coordinates = coin.SoCoordinate3()
        self._faces = coin.SoIndexedFaceSet()
        self._faces.setName("VoelundSolid")
        self._root.addChild(self._camera)
        self._root.addChild(self._light)
        self._root.addChild(self._pan)
        self._root.addChild(self._scale)
        self._root.addChild(self._yaw)
        self._root.addChild(self._pitch)
        self._root.addChild(self._center)
        self._root.addChild(self._material)
        self._root.addChild(self._coordinates)
        self._root.addChild(self._faces)
        self._set_color()
        self.setSceneGraph(self._root)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        self.setSceneGraph(None)
        self._root.unref()
        super().closeEvent(event)

    def set_mesh(self, mesh: DisplayMesh, *, fit: bool = False) -> None:
        if not mesh.vertices or not mesh.triangles:
            raise ValueError("Display mesh must contain vertices and triangles")
        flat_indices = [index for triangle in mesh.triangles for index in (*triangle, -1)]
        self._coordinates.point.setValues(0, len(mesh.vertices), mesh.vertices)
        self._faces.coordIndex.setValues(0, len(flat_indices), flat_indices)
        mins = [min(vertex[axis] for vertex in mesh.vertices) for axis in range(3)]
        maxs = [max(vertex[axis] for vertex in mesh.vertices) for axis in range(3)]
        self._center.translation = tuple(-(low + high) / 2.0 for low, high in zip(mins, maxs))
        if fit:
            self.viewAll()
        self.update()

    def camera_signature(self) -> tuple[float, ...]:
        position = self._camera.position.getValue().getValue()
        orientation = self._camera.orientation.getValue().getValue()
        return tuple(round(float(value), 6) for value in (*position, *orientation))

    def transform_signature(self) -> tuple[float, ...]:
        translation = self._pan.translation.getValue().getValue()
        scale = self._scale.scaleFactor.getValue().getValue()
        return tuple(
            round(float(value), 6)
            for value in (*translation, *scale, float(self._yaw.angle.getValue()), float(self._pitch.angle.getValue()))
        )

    def reset_view(self) -> None:
        """Restore a deterministic axonometric presentation after interaction tests."""

        self._pan.translation = (0.0, 0.0, 0.0)
        self._scale.scaleFactor = (1.0, 1.0, 1.0)
        self._yaw.angle = 0.55
        self._pitch.angle = -0.35
        self._preselected = False
        self._selected = False
        self._set_color()
        self.viewAll()
        self.update()

    @property
    def event_counts(self) -> dict[str, int]:
        return dict(self._event_counts)

    def event(self, event: QtCore.QEvent) -> bool:
        """Keep input ownership in Völund instead of Quarter's examiner state machine."""

        owned_types = {
            QtCore.QEvent.Type.MouseButtonPress,
            QtCore.QEvent.Type.MouseButtonRelease,
            QtCore.QEvent.Type.MouseMove,
            QtCore.QEvent.Type.Wheel,
        }
        if event.type() in owned_types:
            return QtOpenGLWidgets.QOpenGLWidget.event(self, event)
        return super().event(event)

    def find_hit_point(self) -> QtCore.QPoint:
        for y_fraction in (0.5, 0.4, 0.6, 0.3, 0.7):
            for x_fraction in (0.5, 0.4, 0.6, 0.3, 0.7):
                point = QtCore.QPoint(round(self.width() * x_fraction), round(self.height() * y_fraction))
                if self._hit_test(point):
                    return point
        raise RuntimeError("Could not locate the rendered solid for the interaction proof")

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        self._event_counts["move"] += 1
        if self._drag_origin is not None and event.buttons():
            delta = event.position() - self._drag_origin
            self._drag_origin = event.position()
            if delta.manhattanLength() >= 1:
                self._dragged = True
            if self._drag_button == QtCore.Qt.MouseButton.LeftButton:
                self._yaw.angle = float(self._yaw.angle.getValue()) + delta.x() * 0.008
                self._pitch.angle = float(self._pitch.angle.getValue()) + delta.y() * 0.008
                self._event_counts["orbit"] += 1
            elif self._drag_button == QtCore.Qt.MouseButton.RightButton:
                current = self._pan.translation.getValue().getValue()
                self._pan.translation = (current[0] + delta.x() * 0.06, current[1] - delta.y() * 0.06, current[2])
                self._event_counts["pan"] += 1
            self.update()
        elif not event.buttons():
            hit = self._hit_test(event.position().toPoint())
            if hit != self._preselected:
                self._preselected = hit
                self._set_color()
                self.preselection_changed.emit(hit)
                self.update()
        event.accept()

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        self._drag_origin = event.position()
        self._drag_button = event.button()
        self._dragged = False
        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() == QtCore.Qt.MouseButton.LeftButton and not self._dragged:
            self._selected = self._hit_test(event.position().toPoint())
            self._event_counts["pick"] += 1
            self._set_color()
            self.selection_changed.emit(self._selected)
            self.update()
        self._drag_origin = None
        self._drag_button = QtCore.Qt.MouseButton.NoButton
        event.accept()

    def wheelEvent(self, event: QtGui.QWheelEvent) -> None:
        factor = 1.12 if event.angleDelta().y() > 0 else 1 / 1.12
        current = self._scale.scaleFactor.getValue().getValue()
        self._scale.scaleFactor = tuple(value * factor for value in current)
        self._event_counts["zoom"] += 1
        self.update()
        event.accept()

    def _hit_test(self, point: QtCore.QPoint) -> bool:
        if self.width() <= 1 or self.height() <= 1:
            return False
        viewport = coin.SbViewportRegion(self.width(), self.height())
        action = coin.SoRayPickAction(viewport)
        action.setPoint(coin.SbVec2s(point.x(), self.height() - point.y()))
        action.setRadius(3.0)
        action.apply(self._root)
        return action.getPickedPoint() is not None

    def _set_color(self) -> None:
        if self._selected:
            self._material.diffuseColor = (0.95, 0.55, 0.12)
        elif self._preselected:
            self._material.diffuseColor = (0.91, 0.75, 0.38)
        else:
            self._material.diffuseColor = (0.55, 0.66, 0.58)
