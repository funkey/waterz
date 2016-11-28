from libcpp.vector cimport vector
from libc.stdint cimport uint64_t, uint32_t
import numpy as np
cimport numpy as np

def agglomerate(affs, thresholds, gt = None, aff_threshold_low  = 0.0001, aff_threshold_high = 0.9999):
    '''
    Compute segmentations from an affinity graph for several thresholds.

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

        gt  : numpy array, uint32, 3 dimensional (optional)

            An optional ground-truth segmentation as an array with gt[z][y][x].
            If given, metrics

    Returns
    -------

        Segmentations (and metrics) are returned as generator objects, and only
        computed on-the-fly when iterated over. This way, you can ask for
        hundreds of thresholds while at any point only one segmentation is
        stored in memory.

        [segmentation]

            Generator object for segmentations (numpy arrays, uint64, 3 dimensional).

        [(segmentation, metrics)]

            Generator object for tuples of segmentations and metrics, if ground-truth volume was
            given. Metrics are given as a dictionary with the keys
            'V_Rand_split', 'V_Rand_merge', 'V_Info_split', and 'V_Info_merge'.
    '''

    # the C++ part assumes contiguous memory, make sure we have it (and do 
    # nothing, if we do)
    if not affs.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous affinity arrray (avoid this by passing C_CONTIGUOUS arrays)")
        affs = np.ascontiguousarray(affs)
    if gt is not None and not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth arrray (avoid this by passing C_CONTIGUOUS arrays)")
        gt = np.ascontiguousarray(gt)

    print("Preparing segmentation volume...")

    volume_shape = (affs.shape[1], affs.shape[2], affs.shape[3])
    thresholds.sort()
    segmentation = np.zeros(volume_shape, dtype=np.uint64)

    cdef WaterzState state = __initialize(affs, segmentation, gt, aff_threshold_low, aff_threshold_high)

    for threshold in thresholds:

        mergeUntil(state, threshold)

        if gt is not None:

            stats = {}
            stats['V_Rand_split'] = state.metrics.rand_split
            stats['V_Rand_merge'] = state.metrics.rand_merge
            stats['V_Info_split'] = state.metrics.voi_split
            stats['V_Info_merge'] = state.metrics.voi_merge

            yield (segmentation, stats)

        else:

            yield segmentation

    free(state)

def __initialize(
        np.ndarray[np.float32_t, ndim=4] affs,
        np.ndarray[uint64_t, ndim=3]     segmentation,
        np.ndarray[uint32_t, ndim=3]     gt = None,
        aff_threshold_low  = 0.0001,
        aff_threshold_high = 0.9999):

    cdef float*    aff_data
    cdef uint64_t* segmentation_data
    cdef uint32_t* gt_data = NULL

    aff_data = &affs[0,0,0,0]
    segmentation_data = &segmentation[0,0,0]
    if gt is not None:
        gt_data = &gt[0,0,0]

    return initialize(
        affs.shape[1], affs.shape[2], affs.shape[3],
        aff_data,
        segmentation_data,
        gt_data,
        aff_threshold_low,
        aff_threshold_high)

cdef extern from "c_frontend.h":

    struct Metrics:
        double voi_split
        double voi_merge
        double rand_split
        double rand_merge

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
            float           affThresholdLow,
            float           affThresholdHigh);

    void mergeUntil(
            WaterzState& state,
            float        threshold)

    void free(WaterzState& state)
