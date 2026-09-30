"""How the agglomerate extension is rendered and compiled.

Used by the JIT path and by `setup.py` (which loads this file by path, before
the package is importable), so it must only depend on the standard library.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path
from typing import TYPE_CHECKING, NamedTuple

if TYPE_CHECKING:
    from collections.abc import Iterable, Iterator

HERE = Path(__file__).parent
TEMPLATE = HERE / "agglomerate.pyx"
WIN = sys.platform == "win32"

COMPILE_ARGS = ["/std:c++14", "/EHsc", "/w"] if WIN else ["-std=c++11", "-w"]
# keep <windows.h> from defining min/max macros, which break std::min/std::max
DEFINE_MACROS = [("NOMINMAX", None)] if WIN else []

# subpackage holding ahead-of-time compiled modules, populated by `setup.py`
PREBUILT_PACKAGE = "waterz._prebuilt"

# Variants compiled ahead of time into binary wheels, everything else is
# compiled on first use (which requires a C++ compiler and the boost headers).
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
    # the relative difference of the sizes, a control that ignores affinities
    functions += [
        "Divide<Subtract<MaxSize<RegionGraphType>,MinSize<RegionGraphType>>,"
        "Add<MaxSize<RegionGraphType>,MinSize<RegionGraphType>>>"
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
    # only the whitespace that C++ knows
    function = re.sub(r"[ \t\n\r\f\v]+", " ", scoring_function.strip(" \t\n\r\f\v"))
    function = re.sub(r" ?([<>,]) ?", r"\1", function)
    # MeanAffinity is an alias for this
    function = re.sub(
        r"\bEdgeStatisticValue<RegionGraphType,MeanAffinityProvider<RegionGraphType,ScoreValue>>",
        "MeanAffinity<RegionGraphType,ScoreValue>",
        function,
    )
    # InitWithMax defaults to true
    return re.sub(
        r"(\bHistogramQuantileAffinity<RegionGraphType,\d+,ScoreValue,\d+)>",
        r"\1,true>",
        function,
    )


PREBUILT_MODULE = f"{PREBUILT_PACKAGE}.agglomerate"


def _declarations(scoring_function: str, discretize_queue: int) -> str:
    """The C++ declaring the scoring function and the queue of one variant."""
    queue = (
        "PriorityQueue<T, S>"
        if discretize_queue == 0
        else f"BinQueue<T, S, {discretize_queue}>"
    )
    return (
        f"typedef {scoring_function} ScoringFunctionType;\n"
        f"template<typename T, typename S> using QueueType = {queue};"
    )


def build_wrapper(specs: Iterable[tuple[str, int]]) -> str:
    """Render the pyx wrapper for the given scoring functions and queues."""
    specs = list(specs)
    variants = "\n".join(
        f"namespace v{i} {{\n{_declarations(*spec)}\n"
        '#include "frontend_agglomerate.h"\n#include "frontend_agglomerate.cpp"\n}'
        for i, spec in enumerate(specs)
    )
    table = ",\n".join(
        f"{{v{i}::initialize, v{i}::mergeUntil, v{i}::getRegionGraph, v{i}::free}}"
        for i in range(len(specs))
    )
    substitutions = {
        # they end up in a string in the pyx, which has to evaluate to just that
        "VARIANTS": variants.replace("\\", "\\\\").replace('"', '\\"'),
        "TABLE": table,
        # what the module was compiled for, as text
        "SPECS": repr([(str(f), str(q)) for f, q in specs]),
    }
    return re.sub(
        r"@(\w+)@", lambda m: substitutions[m.group(1)], TEMPLATE.read_text(encoding="utf-8")
    )


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
    # conda environments (sys.prefix is a throwaway venv during an isolated build)
    prefixes = [sys.prefix, sys.base_prefix, os.environ.get("CONDA_PREFIX", "")]
    for prefix in dict.fromkeys(filter(None, prefixes)):
        candidates += [Path(prefix) / "Library" / "include", Path(prefix) / "include"]
    if sys.platform == "darwin":
        candidates += [Path("/opt/homebrew/include"), Path("/usr/local/include")]
    return [str(d) for d in candidates if (d / "boost" / "multi_array.hpp").is_file()]


def include_dirs() -> list[str]:
    """Include directories needed to compile the wrapper."""
    return [str(HERE), str(HERE / "backend"), *_boost_include_dirs()]
