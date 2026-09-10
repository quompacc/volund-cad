from __future__ import annotations

from pathlib import Path
from typing import Any

import FreeCAD as App
import Part
import Sketcher

from .contracts import DisplayMesh, ModelSnapshot, StepSnapshot


class FreeCadAdapter:
    """Small FreeCAD adapter used only to test the architectural boundary."""

    DOCUMENT_NAME = "VoelundBackendProof"

    def __init__(self) -> None:
        self._doc: Any | None = None

    def create_model(
        self,
        width_mm: float = 40.0,
        height_mm: float = 24.0,
        depth_mm: float = 12.0,
        fillet_mm: float = 2.0,
    ) -> ModelSnapshot:
        self._validate_dimensions(width_mm, height_mm, depth_mm, fillet_mm)
        self.close()
        self._doc = App.newDocument(self.DOCUMENT_NAME)
        self._doc.UndoMode = 1

        sketch = self._doc.addObject("Sketcher::SketchObject", "Sketch")
        points = (
            App.Vector(0, 0, 0),
            App.Vector(width_mm, 0, 0),
            App.Vector(width_mm, height_mm, 0),
            App.Vector(0, height_mm, 0),
        )
        for start, end in zip(points, points[1:] + points[:1]):
            sketch.addGeometry(Part.LineSegment(start, end), False)

        for line_index in (0, 2):
            sketch.addConstraint(Sketcher.Constraint("Horizontal", line_index))
        for line_index in (1, 3):
            sketch.addConstraint(Sketcher.Constraint("Vertical", line_index))
        for first, second in ((0, 1), (1, 2), (2, 3), (3, 0)):
            sketch.addConstraint(Sketcher.Constraint("Coincident", first, 2, second, 1))
        sketch.addConstraint(Sketcher.Constraint("Coincident", 0, 1, -1, 1))
        sketch.addConstraint(Sketcher.Constraint("Distance", 0, width_mm))
        sketch.addConstraint(Sketcher.Constraint("Distance", 1, height_mm))

        sketch.addProperty("App::PropertyLength", "VoelundWidth", "Völund")
        sketch.addProperty("App::PropertyLength", "VoelundHeight", "Völund")
        sketch.VoelundWidth = width_mm
        sketch.VoelundHeight = height_mm

        extrusion = self._doc.addObject("Part::Extrusion", "Extrusion")
        extrusion.Base = sketch
        extrusion.Dir = App.Vector(0, 0, depth_mm)
        extrusion.Solid = True
        extrusion.addProperty("App::PropertyLength", "VoelundDepth", "Völund")
        extrusion.VoelundDepth = depth_mm

        fillet = self._doc.addObject("PartDesign::Feature", "Fillet")
        fillet.addProperty("App::PropertyLink", "Source", "Völund")
        fillet.addProperty("App::PropertyLength", "Radius", "Völund")
        fillet.Source = extrusion
        fillet.Radius = fillet_mm

        self._doc.recompute()
        self._rebuild_fillet()
        self._doc.recompute()
        return self.snapshot()

    def set_extrusion_length(self, depth_mm: float) -> ModelSnapshot:
        if depth_mm <= 0:
            raise ValueError("Extrusion length must be positive")
        doc = self._require_doc()
        doc.openTransaction("Set extrusion length")
        extrusion = doc.getObject("Extrusion")
        extrusion.Dir = App.Vector(0, 0, depth_mm)
        extrusion.VoelundDepth = depth_mm
        doc.recompute()
        self._rebuild_fillet()
        doc.recompute()
        doc.commitTransaction()
        return self.snapshot()

    def undo(self) -> ModelSnapshot:
        doc = self._require_doc()
        doc.undo()
        doc.recompute()
        return self.snapshot()

    def redo(self) -> ModelSnapshot:
        doc = self._require_doc()
        doc.redo()
        doc.recompute()
        return self.snapshot()

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self._require_doc().saveAs(str(path.resolve()))

    def reopen(self, path: Path) -> ModelSnapshot:
        self.close()
        self._doc = App.openDocument(str(path.resolve()))
        self._doc.recompute()
        return self.snapshot()

    def export_step(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        fillet = self._require_doc().getObject("Fillet")
        if fillet is None or fillet.Shape.isNull():
            raise RuntimeError("No final shape is available for STEP export")
        Part.export([fillet], str(path.resolve()))

    def display_mesh(self, object_name: str = "Fillet", deflection_mm: float = 0.25) -> DisplayMesh:
        """Tessellate a shape without leaking FreeCAD objects into the UI layer."""

        if deflection_mm <= 0:
            raise ValueError("Display deflection must be positive")
        obj = self._require_doc().getObject(object_name)
        if obj is None or not hasattr(obj, "Shape") or obj.Shape.isNull():
            raise RuntimeError(f"No displayable shape exists for {object_name}")
        vertices, triangles = obj.Shape.tessellate(deflection_mm)
        return DisplayMesh(
            vertices=tuple((float(vertex.x), float(vertex.y), float(vertex.z)) for vertex in vertices),
            triangles=tuple(tuple(int(index) for index in triangle) for triangle in triangles),
        )

    def validate_step(self, path: Path) -> StepSnapshot:
        shape = Part.read(str(path.resolve()))
        if shape.isNull() or len(shape.Solids) != 1:
            raise RuntimeError("STEP round-trip did not produce exactly one solid")
        return StepSnapshot(solid_count=len(shape.Solids), volume_mm3=float(shape.Volume))

    def snapshot(self) -> ModelSnapshot:
        doc = self._require_doc()
        extrusion = doc.getObject("Extrusion")
        fillet = doc.getObject("Fillet")
        sketch = doc.getObject("Sketch")
        if sketch is None or extrusion is None or fillet is None or fillet.Shape.isNull():
            raise RuntimeError("The proof model is incomplete")
        return ModelSnapshot(
            document_name=doc.Name,
            object_names=tuple(obj.Name for obj in doc.Objects),
            volume_mm3=float(fillet.Shape.Volume),
            extrusion_length_mm=float(extrusion.VoelundDepth),
            fillet_radius_mm=float(fillet.Radius),
            sketch_fully_constrained=bool(sketch.FullyConstrained),
        )

    def close(self) -> None:
        if self._doc is not None:
            name = self._doc.Name
            self._doc = None
            App.closeDocument(name)

    def _rebuild_fillet(self) -> None:
        doc = self._require_doc()
        extrusion = doc.getObject("Extrusion")
        fillet = doc.getObject("Fillet")
        radius = float(fillet.Radius)
        vertical_edges = [
            edge
            for edge in extrusion.Shape.Edges
            if (edge.Vertexes[-1].Point - edge.Vertexes[0].Point).z != 0
            and abs((edge.Vertexes[-1].Point - edge.Vertexes[0].Point).x) < 1e-7
            and abs((edge.Vertexes[-1].Point - edge.Vertexes[0].Point).y) < 1e-7
        ]
        if not vertical_edges:
            raise RuntimeError("Could not locate a vertical edge for the fillet proof")
        fillet.Shape = extrusion.Shape.makeFillet(radius, [vertical_edges[0]])

    def _require_doc(self) -> Any:
        if self._doc is None:
            raise RuntimeError("No FreeCAD document is open")
        return self._doc

    @staticmethod
    def _validate_dimensions(width_mm: float, height_mm: float, depth_mm: float, fillet_mm: float) -> None:
        if min(width_mm, height_mm, depth_mm, fillet_mm) <= 0:
            raise ValueError("All dimensions must be positive")
        if fillet_mm >= min(width_mm, height_mm) / 2:
            raise ValueError("Fillet radius is too large for the rectangle")
