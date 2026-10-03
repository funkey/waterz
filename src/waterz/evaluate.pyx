from libc.stdint cimport uint64_t
import numpy as np

def evaluate(segmentation, gt):

    for name, array in (("segmentation", segmentation), ("gt", gt)):
        if not isinstance(array, np.ndarray):
            raise TypeError(
                f"Argument '{name}' has incorrect type "
                f"(expected numpy.ndarray, got {type(array).__name__})")

    # checks the number of dimensions and the dtype
    cdef const uint64_t[:, :, :] segmentation_view = segmentation
    cdef const uint64_t[:, :, :] gt_view = gt

    if segmentation.shape != gt.shape:
        raise ValueError(
            f"Shapes do not match: segmentation {segmentation.shape}, "
            f"gt {gt.shape}")

    # the C++ part assumes contiguous memory, make sure we have it (and do 
    # nothing, if we do)
    if not segmentation.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous segmentation arrray (avoid this by passing C_CONTIGUOUS arrays)")
        segmentation_view = np.ascontiguousarray(segmentation)
    if not gt.flags['C_CONTIGUOUS']:
        print("Creating memory-contiguous ground-truth arrray (avoid this by passing C_CONTIGUOUS arrays)")
        gt_view = np.ascontiguousarray(gt)

    return compare_arrays(
        segmentation.shape[0], segmentation.shape[1], segmentation.shape[2],
        &gt_view[0, 0, 0],
        &segmentation_view[0, 0, 0])

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
