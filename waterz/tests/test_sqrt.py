import waterz
import unittest
import numpy as np
import logging

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)


class TestSkipKQuantile(unittest.TestCase):

    def test_min(self):
        score_function = 'MinAffinity<RegionGraphType, ScoreValue>'
        affs = np.ones(shape=(3, 4, 4, 4), dtype=np.float32)
        affs[:][0] = 0.0
        thresholds = [np.float32(0.0)]
        fragments = np.ones(shape=(4, 4, 4), dtype=np.uint64)
        fragments[1] = 2
        fragments[2] = 3
        generator = waterz.agglomerate(
                affs=affs,
                thresholds=thresholds,
                fragments=fragments,
                scoring_function=score_function,
                return_merge_history=True,
                return_region_graph=True,
                force_rebuild=False)
        result = next(generator)
        print(result)

    def test_histogram_quantile(self):
        score_function = 'HistogramQuantileAffinity<RegionGraphType, 0, ScoreValue, 10, false>'
        affs = np.array([[[[0.1, 0.2]]]], dtype=np.float32)
        thresholds = [np.float32(1.0)]
        fragments = np.array([[[1, 2]]], dtype=np.uint64)
        generator = waterz.agglomerate(
                affs=affs,
                thresholds=thresholds,
                fragments=fragments,
                scoring_function=score_function,
                return_merge_history=True,
                return_region_graph=False,
                force_rebuild=True)
        result = next(generator)
        print(result)

    def test_skip_k_simple(self):
        score_function = 'SkipKHistogramQuantileAffinity<<AffinityHistogram<RegionGraphType>,<Constant<RegionGraphType, 1>>,50>'
        affs = np.array([[[[0.1, 0.1], [0.9, 0.9]]]])
        thresholds = [1.0]
        fragments = np.array([[[1, 2], [1, 2]]])
        generator = waterz.agglomerate(
                affs=affs,
                thresholds=thresholds,
                fragments=fragments,
                scoring_function=score_function,
                return_merge_history=True,
                return_region_graph=True)
        result = next(generator)
        print(result)
