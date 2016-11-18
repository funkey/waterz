from libcpp.vector cimport vector
from libc.stdint cimport uint64_t, uint32_t
import numpy as np
cimport numpy as np

def agglomerate(np.ndarray[np.float32_t, ndim=4] affs, thresholds, np.ndarray[uint32_t, ndim=3] gt = None):
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

        segmentations

            List of segmentations (numpy arrays, uint64, 3 dimensional).

        (segmentations, metrics)

            Tuple of segmentations and metrics, if ground-truth volume was
            given.
    '''

    cdef vector[uint64_t*]            segmentation_data
    cdef np.ndarray[uint64_t, ndim=3] segmentation
    cdef uint32_t*                    gt_data = NULL

    # the C++ part assumes contiguous memory, make sure we have it (and do 
    # nothing, if we do)
    if not affs.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous affinity arrray (avoid this by passing C_CONTIGUOUS arrays)")
        affs = np.ascontiguousarray(affs)
    if gt is not None and not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth arrray (avoid this by passing C_CONTIGUOUS arrays)")
        gt = np.ascontiguousarray(gt)

    print("Preparing segmentation volumes...")

    segmentations = []
    volume_shape = (affs.shape[1], affs.shape[2], affs.shape[3])
    thresholds.sort()
    for i in range(len(thresholds)):
        segmentation = np.zeros(volume_shape, dtype=np.uint64)
        segmentations.append(segmentation)
        segmentation_data.push_back(&segmentation[0,0,0])

    if gt is not None:
        gt_data = &gt[0,0,0]

    print("Processing thresholds")

    metrics = process_thresholds(
        thresholds,
        affs.shape[1], affs.shape[2], affs.shape[3],
        &affs[0, 0, 0, 0],
        segmentation_data,
        gt_data)

    if gt is not None:
        stats = {
            'V_Rand'      : 0,
            'V_Rand_split': [],
            'V_Rand_merge': [],
            'V_Info'      : 0,
            'V_Info_split': [],
            'V_Info_merge': []
        }
        for metric in metrics:
            rand_f_score = 2.0/(1.0/metric.rand_split + 1.0/metric.rand_merge)
            voi_score = 2.0/(1.0/metric.voi_split + 1.0/metric.voi_merge)
            stats['V_Rand'] = max(stats['V_Rand'], rand_f_score)
            stats['V_Rand_split'].append(metric.rand_split)
            stats['V_Rand_merge'].append(metric.rand_merge)
            stats['V_Info'] = max(stats['V_Info'], voi_score)
            stats['V_Info_split'].append(metric.voi_split)
            stats['V_Info_merge'].append(metric.voi_merge)
        return (segmentations, stats)
    return segmentations

cdef extern from "c_frontend.h":

    struct Metrics:
        double voi_split
        double voi_merge
        double rand_split
        double rand_merge

    vector[Metrics] process_thresholds(
            vector[float] thresholds,
            size_t width, size_t height, size_t depth,
            np.float32_t* affs,
            vector[uint64_t*]& segmentation_data,
            uint32_t* gt_data)
