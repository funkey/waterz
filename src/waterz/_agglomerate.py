from __future__ import annotations

import importlib
import operator
from typing import TYPE_CHECKING

from ._codegen import (
    COMPILE_ARGS,
    DEFINE_MACROS,
    PREBUILT_PACKAGE,
    build_wrapper,
    depends,
    env_enabled,
    include_dirs,
    module_name,
    normalize,
)

if TYPE_CHECKING:
    from collections.abc import Iterator, Sequence
    from types import ModuleType

    import numpy as np
    from numpy.typing import NDArray


def _load_prebuilt(scoring_function: str, discretize_queue: int) -> ModuleType | None:
    """Return the ahead-of-time compiled module, or None if not shipped."""
    if env_enabled("WATERZ_NO_PREBUILT"):
        return None
    try:
        bins = operator.index(discretize_queue)
    except TypeError:  # not an integer: not a variant that is shipped
        return None
    name = f"{PREBUILT_PACKAGE}.{module_name(scoring_function, bins)}"
    try:
        module = importlib.import_module(name)
    except ModuleNotFoundError as e:
        if e.name != name:
            raise
        return None
    # the module records what it was compiled for
    compiled_for = (normalize(module.SCORING_FUNCTION), module.DISCRETIZE_QUEUE)
    if compiled_for != (normalize(scoring_function), str(bins)):
        raise RuntimeError(
            f"{name} was compiled for {compiled_for}, not for {scoring_function!r}"
        )
    return module


def _jit_compile(
    scoring_function: str, discretize_queue: int, force_rebuild: bool
) -> ModuleType:
    """Compile a module with the system C++ compiler.

    Only reached for variants that are not shipped prebuilt. witty is imported
    here, such that it is not needed for the others.
    """
    try:
        import witty
    except ImportError as e:
        raise ImportError(
            f"Compiling the agglomeration for {scoring_function!r} with "
            f"discretize_queue={discretize_queue!r} requires witty (`pip install "
            "witty`), a C++ compiler, and the boost headers."
        ) from e

    return witty.compile_cython(
        build_wrapper(scoring_function, discretize_queue),
        depends_on=depends(),
        extra_compile_args=COMPILE_ARGS,
        include_dirs=include_dirs(),
        define_macros=DEFINE_MACROS,
        language="c++",
        quiet=True,
        force_rebuild=force_rebuild,
    )


def agglomerate(
    affs: NDArray[np.float32],
    thresholds: Sequence[float],
    gt: NDArray[np.uint32] | None = None,
    fragments: NDArray[np.uint64] | None = None,
    aff_threshold_low: float = 0.0001,
    aff_threshold_high: float = 0.9999,
    return_merge_history: bool = False,
    return_region_graph: bool = False,
    scoring_function: str = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>",
    discretize_queue: int = 0,
    force_rebuild: bool = False,
) -> Iterator[tuple | NDArray[np.uint64]]:
    """Compute segmentations from an affinity graph for several thresholds.

    Passed volumes need to be converted into contiguous memory arrays. This will
    be done for you if needed, but you can save memory by making sure your
    volumes are already C_CONTIGUOUS.

    Parameters
    ----------

        affs: numpy array, float32, 4 dimensional

            The affinities as an array with affs[channel][z][y][x].

        thresholds: list of float32

            The thresholds to compute segmentations for. For each threshold, one
            segmentation is returned.

        gt: numpy array, uint32, 3 dimensional (optional)

            An optional ground-truth segmentation as an array with gt[z][y][x].
            If given, metrics

        fragments: numpy array, uint64, 3 dimensional (optional)

            An optional volume of fragments to use, instead of the build-in
            zwatershed.

        aff_threshold_low: float, default 0.0001
        aff_threshold_high: float, default 0.9999,

            Thresholds on the affinities for the initial segmentation step.

        return_merge_history: bool

            If set to True, the returning tuple will contain a merge history,
            relative to the previous segmentation.

        return_region_graph: bool

            If set to True, the returning tuple will contain the region graph
            for the returned segmentation.

        scoring_function: string, default 'OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>'

            A C++ type string specifying the edge scoring function to use.
            Common ones are precompiled, others are compiled on first use (which
            requires a C++ compiler and the boost headers). See

                https://github.com/funkey/waterz/blob/master/src/waterz/backend/MergeFunctions.hpp

            for available functions, and

                https://github.com/funkey/waterz/blob/master/src/waterz/backend/Operators.hpp

            for operators to combine them.

        discretize_queue: int

            If set to non-zero, a bin queue with that many bins will be used to
            approximate the priority queue for merge operations.

        force_rebuild: bool

            Force the rebuild of the module, even if it is precompiled. Only
            needed for development.

    Returns
    -------

        Results are returned as tuples from a generator object, and only
        computed on-the-fly when iterated over. This way, you can ask for
        hundreds of thresholds while at any point only one segmentation is
        stored in memory.

        Depending on the given parameters, the returned values are a subset of
        the following items (in that order):

        segmentation

            The current segmentation (numpy array, uint64, 3 dimensional).

        metrics (only if ground truth was provided)

            A  dictionary with the keys 'V_Rand_split', 'V_Rand_merge',
            'V_Info_split', and 'V_Info_merge'.

        merge_history (only if return_merge_history is True)

            A list of dictionaries with keys 'a', 'b', 'c', and 'score',
            indicating that region a got merged with b into c with the given
            score.

        region_graph (only if return_region_graph is True)

            A list of dictionaries with keys 'u', 'v', and 'score', indicating
            an edge between u and v with the given score.

    Examples
    --------

        affs = ...
        gt   = ...

        # only segmentation
        for segmentation in agglomerate(affs, range(100,10000,100)):
            # ...

        # segmentation with merge history
        for segmentation, merge_history in agglomerate(
            affs, range(100,10000,100), return_merge_history = True):
            # ...

        # segmentation with merge history and metrics compared to gt
        for segmentation, metrics, merge_history in agglomerate(
            affs, range(100,10000,100), gt, return_merge_history = True):
            # ...
    """
    module = None
    if not force_rebuild:
        module = _load_prebuilt(scoring_function, discretize_queue)
    if module is None:
        module = _jit_compile(scoring_function, discretize_queue, force_rebuild)

    # call compiled function
    return module.agglomerate(
        affs,
        thresholds,
        gt,
        fragments,
        aff_threshold_low,
        aff_threshold_high,
        return_merge_history,
        return_region_graph,
    )
