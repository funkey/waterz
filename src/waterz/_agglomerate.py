from __future__ import annotations

import importlib
import operator
from typing import TYPE_CHECKING

import numpy as np

from ._codegen import (
    COMPILE_ARGS,
    DEFINE_MACROS,
    PREBUILT_MODULE,
    build_wrapper,
    depends,
    env_enabled,
    include_dirs,
    iter_specs,
    normalize,
)

if TYPE_CHECKING:
    from collections.abc import Iterator, Sequence
    from types import ModuleType

    from numpy.typing import NDArray

# more bins than this make for a queue of a size that is no use (each bin is a
# `std::queue`), and at some point for one that cannot be allocated
MAX_QUEUE_BINS = 2**16


def _bins(discretize_queue: object) -> int | None:
    """The queue as an integer, if it is one (also as a string, or a float)."""
    try:
        return operator.index(discretize_queue)
    except TypeError:
        pass
    if isinstance(discretize_queue, str):
        return int(discretize_queue) if discretize_queue.strip().isdecimal() else None
    try:
        as_float = float(discretize_queue)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return None
    return int(as_float) if as_float.is_integer() else None


def _check_queue(discretize_queue: object) -> int:
    """The number of bins of the queue, or a `ValueError` if it is not one."""
    bins = None if isinstance(discretize_queue, bool) else _bins(discretize_queue)
    if bins is None or not 0 <= bins <= MAX_QUEUE_BINS:
        raise ValueError(
            "discretize_queue must be 0 (no bins) or a number of bins of at most "
            f"{MAX_QUEUE_BINS}, not {discretize_queue!r}"
        )
    return bins


def _check_shapes(affs: object, gt: object, fragments: object) -> None:
    """Raise a `ValueError` if the volumes do not fit the affinities."""
    shape = np.shape(affs)
    if len(shape) != 4 or shape[0] < 3:
        raise ValueError(
            f"affs must have the shape (channels, z, y, x), with at least 3 "
            f"channels, not {shape}"
        )
    for name, volume in (("gt", gt), ("fragments", fragments)):
        if volume is not None and np.shape(volume) != shape[1:]:
            raise ValueError(
                f"{name} must have the shape of the affinities without the "
                f"channels, {shape[1:]}, not {np.shape(volume)}"
            )


# the variants that are compiled ahead of time, by what is asked for
_VARIANTS = {
    (normalize(function), str(queue)): variant
    for variant, (function, queue) in enumerate(iter_specs())
}


def _load_prebuilt(
    scoring_function: str, discretize_queue: int
) -> tuple[ModuleType, int] | None:
    """Return the module compiled ahead of time and the variant, or None."""
    if env_enabled("WATERZ_NO_PREBUILT"):
        return None
    bins = _bins(discretize_queue)
    if bins is None:  # not an integer: not a variant that is shipped
        return None
    wanted = (normalize(scoring_function), str(bins))
    variant = _VARIANTS.get(wanted)
    if variant is None:
        return None
    try:
        module = importlib.import_module(PREBUILT_MODULE)
    except ModuleNotFoundError as e:
        if e.name != PREBUILT_MODULE:
            raise
        return None
    # the module records what each variant was compiled for
    function, queue = module.SPECS[variant]
    if (normalize(function), queue) != wanted:
        raise RuntimeError(
            f"variant {variant} of {PREBUILT_MODULE} was compiled for "
            f"{module.SPECS[variant]}, not for {scoring_function!r}"
        )
    return module, variant


def _jit_compile(
    scoring_function: str, discretize_queue: int, force_rebuild: bool
) -> tuple[ModuleType, int]:
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

    module = witty.compile_cython(
        build_wrapper([(scoring_function, discretize_queue)]),
        depends_on=depends(),
        extra_compile_args=COMPILE_ARGS,
        include_dirs=include_dirs(),
        define_macros=DEFINE_MACROS,
        language="c++",
        quiet=True,
        force_rebuild=force_rebuild,
    )
    return module, 0


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
            segmentation is returned, in increasing order of the thresholds.

        gt: numpy array, uint32, 3 dimensional (optional)

            An optional ground-truth segmentation as an array with gt[z][y][x].
            If given, metrics

        fragments: numpy array, uint64, 3 dimensional (optional)

            An optional volume of fragments to use, instead of the build-in
            zwatershed.

        aff_threshold_low: float, default 0.0001
        aff_threshold_high: float, default 0.9999,

            Thresholds on the affinities for the initial segmentation step
            (not used if fragments are given). The low one has to be smaller
            than the high one.

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

                https://github.com/funkey/waterz/blob/main/src/waterz/backend/MergeFunctions.hpp

            for available functions, and

                https://github.com/funkey/waterz/blob/main/src/waterz/backend/Operators.hpp

            for operators to combine them.

        discretize_queue: int

            If set to non-zero, a bin queue with that many bins (at most
            65536) will be used to approximate the priority queue for merge
            operations.

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
    discretize_queue = _check_queue(discretize_queue)
    _check_shapes(affs, gt, fragments)
    # only used by the watershed, which is skipped for given fragments
    if fragments is None and not aff_threshold_low < aff_threshold_high:
        raise ValueError(
            f"aff_threshold_low ({aff_threshold_low}) must be smaller than "
            f"aff_threshold_high ({aff_threshold_high})"
        )

    compiled = None
    if not force_rebuild:
        compiled = _load_prebuilt(scoring_function, discretize_queue)
    if compiled is None:
        compiled = _jit_compile(scoring_function, discretize_queue, force_rebuild)
    module, variant = compiled

    # call compiled function
    return module.agglomerate(
        variant,
        affs,
        # any iterable (also a range), and leave the one passed alone
        sorted(thresholds),
        gt,
        fragments,
        aff_threshold_low,
        aff_threshold_high,
        return_merge_history,
        return_region_graph,
    )
