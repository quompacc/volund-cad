import ast
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ALLOWED_FILES = {
    (ROOT / "src" / "voelund_cad" / "freecad_adapter.py").resolve(),
    (ROOT / "src" / "voelund_cad" / "freecad_gui_probe.py").resolve(),
    (ROOT / "src" / "voelund_cad" / "freecad_shell_probe.py").resolve(),
    (ROOT / "src" / "voelund_cad" / "core_viewport.py").resolve(),
    (ROOT / "src" / "voelund_cad" / "core_shell_probe.py").resolve(),
}
FORBIDDEN_MODULES = {"FreeCAD", "FreeCADGui", "Part", "Sketcher", "pivy"}


class SourceBoundaryTests(unittest.TestCase):
    def test_only_adapter_imports_freecad_modules(self):
        violations = []
        source_files = list((ROOT / "src").rglob("*.py")) + list((ROOT / "scripts").rglob("*.py"))
        for path in source_files:
            if path.resolve() in ALLOWED_FILES:
                continue
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                names = []
                if isinstance(node, ast.Import):
                    names = [alias.name.split(".", 1)[0] for alias in node.names]
                elif isinstance(node, ast.ImportFrom) and node.module:
                    names = [node.module.split(".", 1)[0]]
                for name in names:
                    if name in FORBIDDEN_MODULES:
                        violations.append(f"{path.relative_to(ROOT)} imports {name}")
        self.assertEqual([], violations)

    def test_core_only_host_never_imports_freecad_gui(self):
        paths = (
            ROOT / "src" / "voelund_cad" / "core_viewport.py",
            ROOT / "src" / "voelund_cad" / "core_shell_probe.py",
            ROOT / "scripts" / "prove_core_shell.py",
        )
        for path in paths:
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            imported = []
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    imported.extend(alias.name for alias in node.names)
                elif isinstance(node, ast.ImportFrom) and node.module:
                    imported.append(node.module)
            self.assertNotIn("FreeCADGui", imported, str(path.relative_to(ROOT)))


if __name__ == "__main__":
    unittest.main()
