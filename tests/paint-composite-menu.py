#!/usr/bin/env python3
"""Actual512 FLTK paint; reuse native client acknowledgements and worker gate."""
import os
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("DOME_SCULPT_TEST_BUILD", str(ROOT / "build")))
os.environ["DOME_NATIVE_GUI_PAINT"] = "1"
os.environ["DOME_NATIVE_GUI_HELPER"] = str(BUILD / "dome-sculpt-paint-composite-fixture")
os.environ.setdefault("DOME_NATIVE_GUI_OUTPUT", str(ROOT / "artifacts" / (
    "paint-composite-menu-accept" if os.environ.get("DOME_NATIVE_GUI_ACCEPT") == "1"
    else "paint-composite-menu-cancel")))
runpy.run_path(str(ROOT / "tests/native-strokes-menu.py"), run_name="__main__")
