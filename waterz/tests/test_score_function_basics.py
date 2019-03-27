import waterz
import unittest
import numpy as np
import logging

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)


class TestScoreFunctionBasics(unittest.TestCase):

    def test_max(self):
        score_function = 'OneMinus<MaxAffinity<RegionGraphType, ScoreValue>>'
        affs = np.ones(shape=(3, 4, 4, 4), dtype=np.float32)
        affs[0] = 0.4
        affs[0,:,0,0] = 0.6
        thresholds = [0, 0.5]
        fragments = np.ones(shape=(4, 4, 4), dtype=np.uint64)
        fragments[1] = 2
        fragments[2] = 3
        fragments[3] = 4
        generator = waterz.agglomerate(
                affs=affs,
                thresholds=thresholds,
                fragments=fragments,
                scoring_function=score_function,
                return_merge_history=True,
                return_region_graph=True,
                force_rebuild=False)

        segmentation, _, _ = next(generator)
        np.testing.assert_array_equal(
            segmentation,
            fragments)

        segmentation, _, _ = next(generator)
        self.assertEqual(len(np.unique(segmentation)), 1)
