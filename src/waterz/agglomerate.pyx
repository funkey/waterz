from libcpp.vector cimport vector
from libc.stdint cimport uint64_t, uint32_t, uint8_t
from libcpp cimport bool
from libcpp.string cimport string

import numpy as np
cimport numpy as np

def agglomerate_rag(
        rag,
        rag_metadata,
        thresholds,
        fragments,
        ):

    if fragments is not None and not fragments.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous fragments arrray (avoid this by passing C_CONTIGUOUS arrays)")
        fragments = np.ascontiguousarray(fragments)

    cdef WaterzState state = __initialize_with_rag(rag, rag_metadata, fragments)

    thresholds.sort()
    for threshold in thresholds:
        merge_history = mergeUntil(state, threshold)
        yield merge_history, fragments

    free(state)


def agglomerate(
        affs: np.array | None,
        thresholds: np.array | None,
        gt: np.array | None,
        fragments: np.array | None,
        semantic: np.array | None,
        segconstraint: np.array | None,

        aff_threshold_low: float,
        aff_threshold_high: float,

        semantic_aff_threshold: float,
        semantic_size_threshold: int,
        semantic_signal_ratio: float,

        semantic_taint_labels: list,
        semantic_taint_threshold: float,

        size_heuristic_aff_threshold: float,
        size_heuristic_small_threshold: int,
        size_heuristic_large_threshold: int,

        return_merge_history: bool,
        return_region_graph: bool,
        return_region_graph_metadata: bool,
    ):

    # the C++ part assumes contiguous memory, make sure we have it (and do 
    # nothing, if we do)
    if affs is not None and not affs.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous affinity arrray (avoid this by passing C_CONTIGUOUS arrays)")
        affs = np.ascontiguousarray(affs)
    if gt is not None and not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth arrray (avoid this by passing C_CONTIGUOUS arrays)")
        gt = np.ascontiguousarray(gt)
    if fragments is not None and not fragments.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous fragments arrray (avoid this by passing C_CONTIGUOUS arrays)")
        fragments = np.ascontiguousarray(fragments)
    if semantic is not None and not semantic.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous semantic arrray (avoid this by passing C_CONTIGUOUS arrays)")
        semantic = np.ascontiguousarray(semantic)
    if segconstraint is not None and not segconstraint.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous segconstraint arrray (avoid this by passing C_CONTIGUOUS arrays)")
        segconstraint = np.ascontiguousarray(segconstraint)

    print("Preparing segmentation volume...")

    if fragments is None:
        volume_shape = (affs.shape[1], affs.shape[2], affs.shape[3])
        segmentation = np.zeros(volume_shape, dtype=np.uint64)
        find_fragments = True
    else:
        segmentation = fragments
        find_fragments = False

    cdef WaterzState state = __initialize(
        affs=affs,
        segmentation=segmentation,
        gt=gt,
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
        find_fragments=find_fragments,
        )

    thresholds.sort()
    for threshold in thresholds:

        merge_history = mergeUntil(state, threshold)

        result = (segmentation,)

        if gt is not None:

            stats = {}
            stats['V_Rand_split'] = state.metrics.rand_split
            stats['V_Rand_merge'] = state.metrics.rand_merge
            stats['V_Info_split'] = state.metrics.voi_split
            stats['V_Info_merge'] = state.metrics.voi_merge

            result += (stats,)

        if return_merge_history:

            result += (merge_history,)

        if return_region_graph:

            result += (getRegionGraph(state),)

        if return_region_graph_metadata:

            result += (getRegionGraphMeta(state),)

        if len(result) == 1:
            yield result[0]
        else:
            yield result

    free(state)

def __initialize(
        np.ndarray[np.float32_t, ndim=4] affs,
        np.ndarray[uint64_t, ndim=3] segmentation,
        np.ndarray[uint32_t, ndim=3] gt,
        np.ndarray[uint8_t, ndim=3] semantic,
        np.ndarray[uint64_t, ndim=3] segconstraint,
        aff_threshold_low: float,
        aff_threshold_high: float,
        semantic_aff_threshold: float,
        semantic_size_threshold: int,
        semantic_signal_ratio: float,
        semantic_taint_labels: list,
        semantic_taint_threshold: float,
        size_heuristic_aff_threshold: float,
        size_heuristic_small_threshold: int,
        size_heuristic_large_threshold: int,
        find_fragments: bool,
    ):

    cdef float*    aff_data
    cdef uint64_t* segmentation_data
    cdef uint32_t* gt_data = NULL
    cdef uint8_t* semantic_data = NULL
    cdef uint64_t* segconstraint_data = NULL
    cdef vector[uint8_t] taint_labels_vec
    for label in semantic_taint_labels:
        taint_labels_vec.push_back(<uint8_t>label)

    aff_data = &affs[0,0,0,0]
    segmentation_data = &segmentation[0,0,0]
    if gt is not None:
        gt_data = &gt[0,0,0]
    if semantic is not None:
        semantic_data = &semantic[0,0,0]
    if segconstraint is not None:
        segconstraint_data = &segconstraint[0,0,0]

    # return initialize(
    #     affs.shape[1], affs.shape[2], affs.shape[3],
    #     aff_data,
    #     segmentation_data,
    #     gt_data,
    #     aff_threshold_low,
    #     aff_threshold_high,
    #     find_fragments

    return initialize(
        width=affs.shape[1],
        height=affs.shape[2],
        depth=affs.shape[3],
        affinity_data=aff_data,
        segmentation_data=segmentation_data,
        groundtruth_data=gt_data,
        semantic_data=semantic_data,
        segconstraint_data=segconstraint_data,

        affThresholdLow=aff_threshold_low,
        affThresholdHigh=aff_threshold_high,

        semantic_aff_threshold=semantic_aff_threshold,
        semantic_size_threshold=semantic_size_threshold,
        semantic_signal_ratio=semantic_signal_ratio,

        semantic_taint_labels=taint_labels_vec,
        semantic_taint_threshold=semantic_taint_threshold,

        size_heuristic_aff_threshold=size_heuristic_aff_threshold,
        size_heuristic_small_threshold=size_heuristic_small_threshold,
        size_heuristic_large_threshold=size_heuristic_large_threshold,

        findFragments=find_fragments,
    )

def __initialize_with_rag(
        rag,
        rag_metadata,
        np.ndarray[uint64_t, ndim=3]     segmentation = None):

    cdef uint64_t* segmentation_data = NULL
    shape = (0, 0, 0)

    if segmentation is not None:
        segmentation_data = &segmentation[0,0,0]
        shape = (segmentation.shape[0], segmentation.shape[1], segmentation.shape[2])


    return initialize_with_rag(
        rag,
        rag_metadata,
        segmentation_data,
        shape[0], shape[1], shape[2])

cdef extern from "frontend_agglomerate.h":

    struct Metrics:
        double voi_split
        double voi_merge
        double rand_split
        double rand_merge

    struct Merge:
        uint64_t a
        uint64_t b
        uint64_t c
        double score

    struct ScoredEdge:
        uint64_t u
        uint64_t v
        double score

    struct WaterzState:
        int     context
        Metrics metrics

    WaterzState initialize(
            size_t          width,
            size_t          height,
            size_t          depth,
            const float*    affinity_data,
            uint64_t*       segmentation_data,
            const uint32_t* groundtruth_data,
            const uint8_t*  semantic_data,
            const uint64_t* segconstraint_data,
            float           affThresholdLow,
            float           affThresholdHigh,
            float           semantic_aff_threshold,
            uint64_t        semantic_size_threshold,
            float           semantic_signal_ratio,
            const vector[uint8_t]& semantic_taint_labels,
            float           semantic_taint_threshold,
            float           size_heuristic_aff_threshold,
            uint64_t        size_heuristic_small_threshold,
            uint64_t        size_heuristic_large_threshold,
            bool            findFragments);

    WaterzState initialize_with_rag(
            const vector[ScoredEdge]& rag,
            const vector[double]& rag_metadata,
            uint64_t*       segmentation_data,
            size_t          width,
            size_t          height,
            size_t          depth,);

    vector[Merge] mergeUntil(
            WaterzState& state,
            float        threshold)

    vector[ScoredEdge] getRegionGraph(WaterzState& state)

    vector[double] getRegionGraphMeta(WaterzState& state)

    void free(WaterzState& state)
