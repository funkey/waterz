from libc.stdint cimport uint64_t
import numpy as np

def evaluate(segmentation, gt):

    for d in range(3):
        assert segmentation.shape[d] == gt.shape[d], (
            "Shapes in dim %d do not match"%d)

    # the C++ part assumes contiguous memory, make sure we have it (and do 
    # nothing, if we do)
    if not segmentation.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous segmentation arrray (avoid this by passing C_CONTIGUOUS arrays)")
        segmentation = np.ascontiguousarray(segmentation)
    if gt is not None and not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth arrray (avoid this by passing C_CONTIGUOUS arrays)")
        gt = np.ascontiguousarray(gt)

    return __evaluate(segmentation, gt)

def __evaluate(
        const uint64_t[:, :, ::1] segmentation,
        const uint64_t[:, :, ::1] gt):

    return compare_arrays(
        segmentation.shape[0], segmentation.shape[1], segmentation.shape[2],
        &gt[0, 0, 0],
        &segmentation[0, 0, 0])

cdef extern from "frontend_evaluate.h":

    struct Metrics:
        double voi_split
        double voi_merge
        double rand_split
        double rand_merge

    Metrics compare_arrays(
            size_t          width,
            size_t          height,
            size_t          depth,
            const uint64_t* gt_data,
            const uint64_t* segmentation_data);
