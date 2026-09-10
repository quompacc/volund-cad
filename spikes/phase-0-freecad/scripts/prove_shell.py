from __future__ import annotations

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from voelund_cad.freecad_shell_probe import launch_shell_probe  # noqa: E402


parser = argparse.ArgumentParser(description="Run the Völund-owned Qt shell probe")
parser.add_argument("--output", type=Path, default=ROOT / "artifacts" / "shell-probe")
args, _freecad_args = parser.parse_known_args()
launch_shell_probe(args.output.resolve())
