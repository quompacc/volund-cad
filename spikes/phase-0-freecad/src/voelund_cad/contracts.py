from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


@dataclass(frozen=True)
class ModelSnapshot:
    document_name: str
    object_names: tuple[str, ...]
    volume_mm3: float
    extrusion_length_mm: float
    fillet_radius_mm: float
    sketch_fully_constrained: bool


@dataclass(frozen=True)
class StepSnapshot:
    solid_count: int
    volume_mm3: float


@dataclass(frozen=True)
class DisplayMesh:
    """Renderer-neutral triangle mesh returned by the CAD boundary."""

    vertices: tuple[tuple[float, float, float], ...]
    triangles: tuple[tuple[int, int, int], ...]


class CadBackend(Protocol):
    """The product-facing boundary; callers must not depend on FreeCAD objects."""

    def create_model(self, width_mm: float, height_mm: float, depth_mm: float, fillet_mm: float) -> ModelSnapshot: ...

    def set_extrusion_length(self, depth_mm: float) -> ModelSnapshot: ...

    def undo(self) -> ModelSnapshot: ...

    def redo(self) -> ModelSnapshot: ...

    def save(self, path: Path) -> None: ...

    def reopen(self, path: Path) -> ModelSnapshot: ...

    def export_step(self, path: Path) -> None: ...

    def display_mesh(self, object_name: str = "Fillet", deflection_mm: float = 0.25) -> DisplayMesh: ...

    def validate_step(self, path: Path) -> StepSnapshot: ...

    def close(self) -> None: ...
