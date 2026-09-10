from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import asdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from voelund_cad import FreeCadAdapter  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Völund FreeCAD backend proof")
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts" / "backend-proof")
    # FreeCAD keeps its own launcher arguments in sys.argv when executing a
    # script. Ignore those and consume only the proof-specific options.
    args, _freecad_args = parser.parse_known_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    report: dict[str, object] = {
        "proof": "freecad-backend",
        "status": "FAIL",
        "started_unix": time.time(),
        "steps": [],
    }
    adapter = FreeCadAdapter()

    def step(name, operation):
        started = time.perf_counter()
        result = operation()
        report["steps"].append(
            {
                "name": name,
                "status": "PASS",
                "duration_ms": round((time.perf_counter() - started) * 1000, 2),
                "snapshot": asdict(result) if result is not None else None,
            }
        )
        return result

    try:
        initial = step("create_sketch_extrusion_fillet", lambda: adapter.create_model())
        if not initial.sketch_fully_constrained:
            raise AssertionError("The proof sketch is not fully constrained")
        changed = step("change_parameter_and_recompute", lambda: adapter.set_extrusion_length(20.0))
        if changed.volume_mm3 <= initial.volume_mm3:
            raise AssertionError("Increasing extrusion length did not increase volume")

        undone = step("undo", adapter.undo)
        if abs(undone.extrusion_length_mm - initial.extrusion_length_mm) > 1e-7:
            raise AssertionError("Undo did not restore the original extrusion length")

        redone = step("redo", adapter.redo)
        if abs(redone.extrusion_length_mm - changed.extrusion_length_mm) > 1e-7:
            raise AssertionError("Redo did not restore the changed extrusion length")

        fcstd_path = output / "voelund_backend_proof.FCStd"
        step("save_fcstd", lambda: adapter.save(fcstd_path))
        reopened = step("close_and_reopen", lambda: adapter.reopen(fcstd_path))
        if abs(reopened.volume_mm3 - redone.volume_mm3) > 1e-5:
            raise AssertionError("Reopened model volume differs from the saved model")

        step_path = output / "voelund_backend_proof.step"
        step("export_step", lambda: adapter.export_step(step_path))
        if not step_path.exists() or step_path.stat().st_size == 0:
            raise AssertionError("STEP export was not created")
        imported = step("validate_step_round_trip", lambda: adapter.validate_step(step_path))
        if abs(imported.volume_mm3 - reopened.volume_mm3) > 1e-4:
            raise AssertionError("STEP round-trip volume differs from the source model")

        report["status"] = "PASS"
        return_code = 0
    except Exception as exc:
        report["error"] = {"type": type(exc).__name__, "message": str(exc)}
        return_code = 1
    finally:
        adapter.close()
        report["finished_unix"] = time.time()
        report_path = output / "proof-report.json"
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(json.dumps(report, indent=2, ensure_ascii=False))

    return return_code


# FreeCAD executes command-line scripts in its own namespace rather than with
# ``__name__ == "__main__"``. This file is an executable entry point, so call
# the runner unconditionally.
raise SystemExit(main())
