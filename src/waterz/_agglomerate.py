from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from collections.abc import Iterator, Sequence

    from numpy.typing import NDArray

HERE = Path(__file__).parent


def agglomerate(
    affs: NDArray[np.float32] | None = None,
    thresholds: Sequence[float] | None = None,
    gt: NDArray[np.uint32] | None = None,
    fragments: NDArray[np.uint64] | None = None,
    semantic: NDArray[np.uint8] | None = None,
    segconstraint: NDArray[np.uint64] | None = None,
    input_rag=None,
    input_rag_metadata=None,
    aff_threshold_low: float = 0.0001,
    aff_threshold_high: float = 0.9999,
    return_merge_history: bool = False,
    return_region_graph: bool = False,
    return_region_graph_metadata: bool = False,
    scoring_function: str = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>",
    semantic_aff_threshold: float = 0.5,
    semantic_size_threshold: int = 100_000,
    semantic_signal_ratio: float = 0.6,
    semantic_taint_labels: list[int] = [],
    semantic_taint_threshold: float = 0.0,
    size_heuristic_aff_threshold: float = 1.0,
    size_heuristic_small_threshold: int = 1_000_000,
    size_heuristic_large_threshold: int = 10_000_000,
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

            A C++ type string specifying the edge scoring function to use. See

                https://github.com/funkey/waterz/blob/master/waterz/backend/MergeFunctions.hpp

            for available functions, and

                https://github.com/funkey/waterz/blob/master/waterz/backend/Operators.hpp

            for operators to combine them.

        discretize_queue: int

            If set to non-zero, a bin queue with that many bins will be used to
            approximate the priority queue for merge operations.

        force_rebuild: bool

            Force the rebuild of the module. Only needed for development.

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
    _DEFAULT_SCORING = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>"
    # Pre-compiled (shipped) modules keyed by discretize_queue, for the default
    # mean scoring. Importing a ready .so avoids runtime JIT (witty) compilation
    # -- and the concurrent-compile cache races that can corrupt it on workers.
    # Other discretize_queue values / scoring functions fall back to JIT below.
    _PRECOMPILED_BY_QUEUE = {0: "_agglomerate_default", 256: "_agglomerate_mean_bin256"}
    _precompiled_name = (
        _PRECOMPILED_BY_QUEUE.get(discretize_queue)
        if scoring_function == _DEFAULT_SCORING
        else None
    )

    if _precompiled_name is not None:
        import importlib

        module = importlib.import_module(f"waterz.{_precompiled_name}")
    else:
        import witty

        with TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            scoredef = f"typedef {scoring_function} ScoringFunctionType;"
            (tmp_path / "ScoringFunction.h").write_text(scoredef)

            queue_src = "template<typename T, typename S> using QueueType = " + (
                "PriorityQueue<T, S>;"
                if discretize_queue == 0
                else f"BinQueue<T, S, {discretize_queue}>;"
            )
            (tmp_path / "Queue.h").write_text(queue_src)

            _include_dirs = [
                str(HERE),
                tmpdir,
                str(HERE / "backend"),
                np.get_include(),
                "/opt/homebrew/include",
            ]
            _compile_args = ["-std=c++11", "-w"]

            def _build_frontend(cache_dir: Path) -> list[str]:
                import subprocess

                obj_path = cache_dir / "frontend_agglomerate.o"
                cpp_path = HERE / "frontend_agglomerate.cpp"
                if not obj_path.exists() or obj_path.stat().st_mtime < cpp_path.stat().st_mtime:
                    cmd = [
                        "c++", *_compile_args,
                        *[f"-I{d}" for d in _include_dirs],
                        "-fPIC", "-c", str(cpp_path), "-o", str(obj_path),
                    ]
                    subprocess.check_call(cmd)
                return [str(obj_path)]

            module = witty.compile_cython(
                (HERE / "agglomerate.pyx").read_text(),
                source_files=[str(HERE / "frontend_agglomerate.cpp")],
                build_extra_objects=_build_frontend,
                extra_link_args=["-std=c++11"],
                extra_compile_args=_compile_args,
                include_dirs=_include_dirs,
                language="c++",
                quiet=True,
                force_rebuild=force_rebuild,
            )

    # call compiled function
    if input_rag is not None or input_rag_metadata is not None:
        return module.agglomerate_rag(
            rag=input_rag,
            rag_metadata=input_rag_metadata,
            thresholds=thresholds,
            fragments=fragments,
        )

    return module.agglomerate(
        affs=affs,
        thresholds=thresholds,
        gt=gt,
        fragments=fragments,
        semantic=semantic,
        segconstraint=segconstraint,
        aff_threshold_low=aff_threshold_low,
        aff_threshold_high=aff_threshold_high,
        semantic_aff_threshold=semantic_aff_threshold,
        semantic_size_threshold=semantic_size_threshold,
        semantic_signal_ratio=semantic_signal_ratio,
        semantic_taint_labels=semantic_taint_labels,
        semantic_taint_threshold=semantic_taint_threshold,
        size_heuristic_aff_threshold=size_heuristic_aff_threshold,
        size_heuristic_small_threshold=size_heuristic_small_threshold,
        size_heuristic_large_threshold=size_heuristic_large_threshold,
        return_merge_history=return_merge_history,
        return_region_graph=return_region_graph,
        return_region_graph_metadata=return_region_graph_metadata,
    )
