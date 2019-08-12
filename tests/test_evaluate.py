import numpy as np
import waterz as wz
from math import isclose


def test_evaluate():
    # make the random value consistent with different runs
    np.random.seed(0)

    seg = np.random.randint(500, size=(3, 3, 3), dtype=np.uint64)
    scores = wz.evaluate(seg, seg)
    assert scores['voi_split'] == 0.0
    assert scores['voi_merge'] == 0.0
    assert scores['rand_split'] == 1.0
    assert scores['rand_merge'] == 1.0

    seg2 = np.random.randint(500, size=(3,3,3), dtype=np.uint64)
    scores = wz.evaluate(seg, seg2)
    #print('scores: ', scores)
    # Note that these values are from the first run 
    # I have not double checked that this is correct or not.
    # This assertion only make sure that future changes of 
    # code will not change the result of the evaluation
    assert isclose(scores['rand_split'], 0.8181818181818182)
    assert isclose(scores['rand_merge'], 0.8709677419354839)
    assert isclose(scores['voi_split'], 0.22222222222222232)
    assert isclose(scores['voi_merge'], 0.14814814814814792)
