"""How the agglomerate extension is rendered and compiled.

Used by the JIT path and by `setup.py` (which loads this file by path, before
the package is importable), so it must only depend on the standard library.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

HERE = Path(__file__).parent
TEMPLATE = HERE / "agglomerate.pyx"
WIN = sys.platform == "win32"

COMPILE_ARGS = ["/std:c++14", "/EHsc", "/w"] if WIN else ["-std=c++11", "-w"]
# keep <windows.h> from defining min/max macros, which break std::min/std::max
DEFINE_MACROS = [("NOMINMAX", None)] if WIN else []


def build_wrapper(scoring_function: str, discretize_queue: int) -> str:
    """Render the pyx wrapper for the given scoring function and queue."""
    source = TEMPLATE.read_text()
    source = source.replace("@SCORING_FUNCTION@", scoring_function)
    return source.replace("@QUEUE_BINS@", str(int(discretize_queue)))


def depends() -> list[str]:
    """C++ files included by the wrapper, a change in which requires a rebuild."""
    frontend = [HERE / "frontend_agglomerate.h", HERE / "frontend_agglomerate.cpp"]
    return [str(f) for f in (*frontend, *sorted((HERE / "backend").glob("*.hpp")))]


def _boost_include_dirs() -> list[str]:
    """Candidate directories for the boost headers, existing ones only."""
    candidates = []
    if include_dir := os.environ.get("BOOST_INCLUDEDIR"):
        candidates.append(Path(include_dir))
    if root := os.environ.get("BOOST_ROOT"):
        candidates += [Path(root) / "include", Path(root)]
    # conda environments
    candidates += [Path(sys.prefix) / "Library" / "include", Path(sys.prefix) / "include"]
    if sys.platform == "darwin":
        candidates += [Path("/opt/homebrew/include"), Path("/usr/local/include")]
    return [str(d) for d in candidates if (d / "boost" / "multi_array.hpp").is_file()]


def include_dirs() -> list[str]:
    """Include directories needed to compile the wrapper."""
    return [str(HERE), str(HERE / "backend"), *_boost_include_dirs()]
