from __future__ import annotations

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from voelund_cad.core_shell_probe import launch_core_shell_probe  # noqa: E402


parser = argparse.ArgumentParser(description="Run the Völund core-only host proof")
parser.add_argument("--output", type=Path, default=ROOT / "artifacts" / "core-shell-probe")
args = parser.parse_args()
raise SystemExit(launch_core_shell_probe(args.output.resolve()))
