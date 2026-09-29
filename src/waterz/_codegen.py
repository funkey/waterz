"""How the agglomerate extension is rendered and compiled.

Used by the JIT path and by `setup.py` (which loads this file by path, before
the package is importable), so it must only depend on the standard library.
"""

from __future__ import annotations

import hashlib
import operator
import os
import re
import sys
from pathlib import Path
from typing import TYPE_CHECKING, NamedTuple

if TYPE_CHECKING:
    from collections.abc import Iterator

HERE = Path(__file__).parent
TEMPLATE = HERE / "agglomerate.pyx"
WIN = sys.platform == "win32"

COMPILE_ARGS = ["/std:c++14", "/EHsc", "/w"] if WIN else ["-std=c++11", "-w"]
# keep <windows.h> from defining min/max macros, which break std::min/std::max
DEFINE_MACROS = [("NOMINMAX", None)] if WIN else []

# subpackage holding ahead-of-time compiled modules, populated by `setup.py`
PREBUILT_PACKAGE = "waterz._prebuilt"

# Variants compiled ahead of time into binary wheels, everything else is
# compiled on first use (which requires `waterz[jit]` and a C++ compiler).
QUANTILES = (10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95)
QUEUE_BINS = (0, 256)

_FALSEY = {"", "0", "false", "no", "off"}


class Spec(NamedTuple):
    scoring_function: str
    discretize_queue: int


def iter_specs() -> Iterator[Spec]:
    """Yield every variant that should be compiled into a wheel."""
    functions = ["OneMinus<MeanAffinity<RegionGraphType,ScoreValue>>"]
    functions += [
        f"OneMinus<HistogramQuantileAffinity<RegionGraphType,{q},ScoreValue,256,{init}>>"
        for q in QUANTILES
        for init in ("false", "true")
    ]
    for function in functions:
        for bins in QUEUE_BINS:
            yield Spec(function, bins)


def env_enabled(name: str) -> bool:
    """Whether the named on/off environment variable is switched on."""
    return os.environ.get(name, "").strip().lower() not in _FALSEY


def normalize(scoring_function: str) -> str:
    """Canonical spelling of a scoring function, equal for equal C++ types.

    Only covers the differences in spelling that are in common use, anything
    else is a different (JIT compiled) variant.
    """
    function = re.sub(r"\s+", " ", scoring_function.strip())
    function = re.sub(r" ?([<>,]) ?", r"\1", function)
    # MeanAffinity is an alias for this
    function = function.replace(
        "EdgeStatisticValue<RegionGraphType,MeanAffinityProvider<RegionGraphType,ScoreValue>>",
        "MeanAffinity<RegionGraphType,ScoreValue>",
    )
    # InitWithMax defaults to true
    return re.sub(
        r"(HistogramQuantileAffinity<RegionGraphType,\d+,ScoreValue,\d+)>",
        r"\1,true>",
        function,
    )


def module_name(scoring_function: str, discretize_queue: int) -> str:
    """Deterministic module name for the given scoring function and queue."""
    function = normalize(scoring_function)
    bins = operator.index(discretize_queue)
    digest = hashlib.sha256(f"{function}|{bins}".encode()).hexdigest()[:8]
    readable = function.replace("RegionGraphType", "").replace("ScoreValue", "")
    readable = re.sub(r"\W+", "_", readable).strip("_")[:60]
    return f"{readable}_q{bins}_{digest}"


def build_wrapper(scoring_function: str, discretize_queue: int) -> str:
    """Render the pyx wrapper for the given scoring function and queue."""
    queue = (
        "PriorityQueue<T, S>"
        if discretize_queue == 0
        else f"BinQueue<T, S, {discretize_queue}>"
    )
    parameters = (
        f"typedef {scoring_function} ScoringFunctionType;\n"
        f"template<typename T, typename S> using QueueType = {queue};"
    )
    # it ends up in a string in the pyx, which has to evaluate to just that
    parameters = parameters.replace("\\", "\\\\").replace('"', '\\"')
    return TEMPLATE.read_text().replace("@PARAMETERS@", parameters)


def depends() -> list[str]:
    """C++ files included by the wrapper, a change in which requires a rebuild."""
    names = (
        "frontend_agglomerate_types.h",
        "frontend_agglomerate.h",
        "frontend_agglomerate.cpp",
    )
    frontend = [HERE / name for name in names]
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
