from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from voelund_cad.freecad_gui_probe import probe_gui  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe the FreeCAD GUI boundary")
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts" / "gui-probe")
    args, _freecad_args = parser.parse_known_args()
    output = args.output.resolve()
    report_path = output / "gui-capabilities.json"
    report = {"probe": "freecad-gui-capabilities", "status": "FAIL"}
    try:
        report["capabilities"] = probe_gui(output)
        report["status"] = "PASS"
        return_code = 0
    except Exception as exc:
        report["error"] = {"type": type(exc).__name__, "message": str(exc)}
        return_code = 1
    output.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return return_code


main()

# FreeCAD executes the script before entering its event loop. Scheduling the
# quit makes the probe deterministic; calling quit immediately is ignored by
# the launcher on Windows.
from PySide6 import QtCore, QtWidgets  # noqa: E402

QtCore.QTimer.singleShot(100, QtWidgets.QApplication.instance().quit)
