from libcpp.vector cimport vector
from libc.stdint cimport uint64_t, uint32_t
from libcpp cimport bool
import numpy as np

def agglomerate(
        variant,
        affs,
        thresholds,
        gt=None,
        fragments=None,
        aff_threshold_low=0.0001,
        aff_threshold_high=0.9999,
        return_merge_history=False,
        return_region_graph=False):

    # the C++ part assumes contiguous memory, make sure we have it (and do
    # nothing, if we do)
    if not affs.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous affinity array (avoid this by passing C_CONTIGUOUS arrays)")
        affs = np.ascontiguousarray(affs)
    if gt is not None and not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth array (avoid this by passing C_CONTIGUOUS arrays)")
        gt = np.ascontiguousarray(gt)
    if fragments is not None and not fragments.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous fragments array (avoid this by passing C_CONTIGUOUS arrays)")
        fragments = np.ascontiguousarray(fragments)

    print("Preparing segmentation volume...")

    if fragments is None:
        volume_shape = (affs.shape[1], affs.shape[2], affs.shape[3])
        segmentation = np.zeros(volume_shape, dtype=np.uint64)
        find_fragments = True
    else:
        segmentation = fragments
        find_fragments = False

    if not 0 <= variant < len(SPECS):
        raise IndexError(f"no variant {variant}, this module has {len(SPECS)}")
    cdef const Variant* v = &VARIANTS[<size_t>variant]
    cdef WaterzState state = __initialize(v, affs, segmentation, gt, aff_threshold_low, aff_threshold_high, find_fragments)
    # frees the state also if this generator is never exhausted
    cdef _StateOwner owner = _StateOwner()
    owner.v = v
    owner.state = state

    thresholds.sort()
    for threshold in thresholds:

        merge_history = v.mergeUntil(state, threshold)

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

            result += (v.getRegionGraph(state),)

        if len(result) == 1:
            yield result[0]
        else:
            yield result

    owner.free()

cdef WaterzState __initialize(
        const Variant* v,
        const float[:, :, :, ::1] affs,
        uint64_t[:, :, ::1]       segmentation,
        const uint32_t[:, :, ::1] gt = None,
        aff_threshold_low  = 0.0001,
        aff_threshold_high = 0.9999,
        find_fragments = True):

    cdef const float*    aff_data
    cdef uint64_t*       segmentation_data
    cdef const uint32_t* gt_data = NULL

    aff_data = &affs[0,0,0,0]
    segmentation_data = &segmentation[0,0,0]
    if gt is not None:
        gt_data = &gt[0,0,0]

    return v.initialize(
        affs.shape[1], affs.shape[2], affs.shape[3],
        aff_data,
        segmentation_data,
        gt_data,
        aff_threshold_low,
        aff_threshold_high,
        find_fragments)

# The scoring functions and queues this module was compiled for, in the order
# of the variants
SPECS = @SPECS@

# The scoring function and the queue are C++ template parameters: the frontend
# is included once per variant, in its own namespace, with the parameters
# declared by `_codegen.build_wrapper` in place of the placeholder.
cdef extern from *:
    """
    #include "frontend_agglomerate_types.h"

    @VARIANTS@

    struct Variant {
        WaterzState (*initialize)(
            size_t, size_t, size_t, const float*, uint64_t*, const uint32_t*,
            float, float, bool);
        std::vector<Merge> (*mergeUntil)(WaterzState&, float);
        std::vector<ScoredEdge> (*getRegionGraph)(WaterzState&);
        void (*free)(WaterzState&);
    };

    static const Variant VARIANTS[] = {
    @TABLE@
    };
    """

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

    struct Variant:
        WaterzState (*initialize)(
            size_t          width,
            size_t          height,
            size_t          depth,
            const float*    affinity_data,
            uint64_t*       segmentation_data,
            const uint32_t* groundtruth_data,
            float           affThresholdLow,
            float           affThresholdHigh,
            bool            findFragments)
        vector[Merge] (*mergeUntil)(WaterzState& state, float threshold)
        vector[ScoredEdge] (*getRegionGraph)(WaterzState& state)
        void (*free)(WaterzState& state)

    const Variant VARIANTS[]

cdef class _StateOwner:
    """Frees the state of a variant, at the latest when garbage collected.

    `agglomerate` cannot free it in a `finally`: compiled for the limited
    API, Cython generators are not finalized when they are collected.
    """

    cdef const Variant* v
    cdef WaterzState state

    cdef void free(self):
        if self.v != NULL:
            self.v.free(self.state)
            self.v = NULL

    def __dealloc__(self):
        self.free()
