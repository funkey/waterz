from math import isclose

import numpy as np
import pytest

import waterz as wz
from waterz._codegen import build_wrapper


def test_evaluate() -> None:
    np.random.seed(0)
    seg = np.random.randint(500, size=(3, 3, 3), dtype=np.uint64)
    scores = wz.evaluate(seg, seg)
    assert scores["voi_split"] == 0.0
    assert scores["voi_merge"] == 0.0
    assert scores["rand_split"] == 1.0
    assert scores["rand_merge"] == 1.0

    seg2 = np.random.randint(500, size=(3, 3, 3), dtype=np.uint64)
    scores = wz.evaluate(seg, seg2)

    # print('scores: ', scores)
    # Note that these values are from the first run
    # I have not double checked that this is correct or not.
    # This assertion only make sure that future changes of
    # code will not change the result of the evaluation
    assert isclose(scores["rand_split"], 0.8181818181818182)
    assert isclose(scores["rand_merge"], 0.8709677419354839)
    assert isclose(scores["voi_split"], 0.22222222222222232)
    assert isclose(scores["voi_merge"], 0.14814814814814792)


def test_agglomerate() -> None:
    np.random.seed(0)
    # affinities is a [3,depth,height,width] numpy array of float32
    affinities = np.random.rand(3, 4, 4, 4).astype(np.float32)

    thresholds = [0, 100, 200]
    results = list(wz.agglomerate(affinities, thresholds))
    assert len(results) == 3
    for segmentation in results:
        assert isinstance(segmentation, np.ndarray)
        assert segmentation.shape == (4, 4, 4)
        assert segmentation.dtype == np.uint64
        # just what I observed... from my random test
        # change when better test data is available
        assert np.all(segmentation == 1)


MEAN = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>"
HIST_QUANT = "OneMinus<HistogramQuantileAffinity<RegionGraphType, 50, ScoreValue, 256, false>>"
MAX_SIZE = "MaxSize<RegionGraphType>"
SIZE_RATIO = (
    "Divide<Subtract<MaxSize<RegionGraphType>, MinSize<RegionGraphType>>, "
    "Add<MaxSize<RegionGraphType>, MinSize<RegionGraphType>>>"
)


def _four_fragments() -> tuple[np.ndarray, np.ndarray]:
    fragments = np.zeros((4, 16, 16), np.uint64)
    fragments[:, :8, :8] = 1
    fragments[:, :8, 8:] = 2
    fragments[:, 8:, :4] = 3
    fragments[:, 8:, 4:] = 4
    rng = np.random.default_rng(0)
    affs = rng.random((3, 4, 16, 16), dtype=np.float32) * 0.5 + 0.3
    return affs, fragments


def _merge_history(threshold: float = 1.0, **kwargs) -> list[tuple[int, int, float]]:
    affs, fragments = _four_fragments()
    _, history = next(
        wz.agglomerate(
            affs, [threshold], fragments=fragments, return_merge_history=True, **kwargs
        )
    )
    return [(m["a"], m["b"], m["score"]) for m in history]


def test_scoring_functions_are_not_mixed_up() -> None:
    functions = (MEAN, HIST_QUANT, MAX_SIZE)
    histories = [_merge_history(1000, scoring_function=f) for f in functions]
    assert histories[0] != histories[1]
    assert histories[0] != histories[2]
    assert histories[1] != histories[2]
    # and again, now that all of them are compiled
    assert histories == [_merge_history(1000, scoring_function=f) for f in functions]


def test_mean_affinity_scores() -> None:
    affs, fragments = _four_fragments()
    boundary: dict[tuple[int, int], list[float]] = {}
    for d in range(3):
        a = np.moveaxis(fragments, d, 0)[1:]
        b = np.moveaxis(fragments, d, 0)[:-1]
        aff = np.moveaxis(affs[d], d, 0)[1:]
        for u, v, value in zip(a[a != b], b[a != b], aff[a != b], strict=True):
            boundary.setdefault((min(u, v), max(u, v)), []).append(value)
    expected = min(1 - np.mean(values) for values in boundary.values())

    u, v, score = _merge_history(scoring_function=MEAN)[0]
    assert isclose(score, expected, rel_tol=1e-5)
    assert isclose(1 - np.mean(boundary[min(u, v), max(u, v)]), expected)


def test_max_size_scores() -> None:
    # fragment sizes are 256, 256, 128, and 384
    # the first merge is between two of the three smallest
    assert _merge_history(scoring_function=MAX_SIZE, threshold=1000)[0][2] == 256


def test_size_ratio_compiles() -> None:
    # dividing sizes used to fail to compile (std::abs of an unsigned type)
    assert len(_merge_history(scoring_function=SIZE_RATIO)) == 3


def test_discretized_queue() -> None:
    exact = _merge_history(scoring_function=MEAN, discretize_queue=0)
    fine = _merge_history(scoring_function=MEAN, discretize_queue=256)
    coarse = _merge_history(scoring_function=MEAN, discretize_queue=4)
    assert fine == exact
    assert coarse != exact


def test_discretized_queue_has_to_be_an_integer() -> None:
    with pytest.raises(TypeError):
        _merge_history(discretize_queue=0.5)


def test_multiline_scoring_function() -> None:
    multiline = """
        OneMinus<
            MeanAffinity<RegionGraphType, ScoreValue>
        >
    """
    assert _merge_history(scoring_function=multiline) == _merge_history()


def test_evaluate_invalid_input() -> None:
    seg = np.arange(1, 28, dtype=np.uint64).reshape(3, 3, 3)
    with pytest.raises(TypeError):
        wz.evaluate(seg.tolist(), seg)
    with pytest.raises(ValueError, match="dimensions"):
        wz.evaluate(seg[0], seg[0])
    with pytest.raises(ValueError, match="dtype"):
        wz.evaluate(seg.astype(np.int64), seg)
    with pytest.raises(AssertionError, match="Shapes"):
        wz.evaluate(seg, seg[:2])
    # not contiguous
    assert wz.evaluate(seg[:, ::2], seg[:, ::2])["rand_split"] == 1.0


def test_build_wrapper() -> None:
    source = build_wrapper(HIST_QUANT, 256)
    assert f"#define WATERZ_SCORING_FUNCTION {HIST_QUANT}\n" in source
    assert "#define WATERZ_QUEUE_BINS 256\n" in source
    assert source != build_wrapper(HIST_QUANT, 0)
    assert source != build_wrapper(MEAN, 256)
