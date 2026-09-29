"""Tests for ahead-of-time compiled agglomerate modules.

The `requires_prebuilt` tests only mean something against an install that
actually shipped them, and are skipped otherwise -- except when
`WATERZ_REQUIRE_PREBUILT` is set (as CI does), where their absence is the very
regression we want to catch.
"""

from __future__ import annotations

import sys
from importlib.util import find_spec

import numpy as np
import pytest

import waterz as wz
from waterz import _agglomerate
from waterz._agglomerate import _load_prebuilt
from waterz._codegen import Spec, env_enabled, iter_specs, module_name, normalize

MEAN = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>"

has_prebuilt = _load_prebuilt(MEAN, 0) is not None
requires_prebuilt = pytest.mark.skipif(
    not has_prebuilt, reason="no prebuilt modules in this install"
)
requires_jit = pytest.mark.skipif(find_spec("witty") is None, reason="needs waterz[jit]")


def _agglomerate_all(spec: Spec) -> list[tuple]:
    rng = np.random.default_rng(0)
    affs = rng.random((3, 6, 20, 20), dtype=np.float32)
    gt = rng.integers(1, 5, size=(6, 20, 20)).astype(np.uint32)
    return [
        (segmentation.copy(), metrics, history, region_graph)
        for segmentation, metrics, history, region_graph in wz.agglomerate(
            affs,
            [0.2, 0.5, 0.8],
            gt=gt,
            aff_threshold_low=0.1,
            aff_threshold_high=0.9,
            return_merge_history=True,
            return_region_graph=True,
            scoring_function=spec.scoring_function,
            discretize_queue=spec.discretize_queue,
        )
    ]


def _no_compiling(*args, **kwargs):
    raise AssertionError("triggered runtime compilation")


def test_prebuilt_modules_were_shipped() -> None:
    """Guard against a wheel that silently shipped without them."""
    if not env_enabled("WATERZ_REQUIRE_PREBUILT"):
        pytest.skip("WATERZ_REQUIRE_PREBUILT not enabled")
    assert has_prebuilt, "install shipped no prebuilt agglomerate modules"


@requires_prebuilt
@pytest.mark.parametrize("spec", list(iter_specs()), ids=lambda s: module_name(*s))
def test_declared_specs_are_shipped_and_never_compile(
    spec: Spec, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(_agglomerate, "_jit_compile", _no_compiling)
    results = _agglomerate_all(spec)
    assert len(results) == 3
    assert len(results[0][2]) > 0


@requires_prebuilt
@pytest.mark.parametrize(
    "scoring_function",
    [
        "OneMinus<MeanAffinity<RegionGraphType,ScoreValue>>",
        "OneMinus<EdgeStatisticValue<RegionGraphType, "
        "MeanAffinityProvider<RegionGraphType, ScoreValue>>>",
        "OneMinus<HistogramQuantileAffinity<RegionGraphType, 50, ScoreValue, 256>>",
        "OneMinus<HistogramQuantileAffinity<RegionGraphType, 85, ScoreValue, 256>>",
        "OneMinus<HistogramQuantileAffinity<RegionGraphType, 25, ScoreValue, 256, false>>",
    ],
)
def test_common_spellings_are_prebuilt(scoring_function: str) -> None:
    assert _load_prebuilt(scoring_function, 256) is not None


def test_normalize() -> None:
    assert normalize(" OneMinus< MeanAffinity<RegionGraphType,  ScoreValue> > ") == (
        "OneMinus<MeanAffinity<RegionGraphType,ScoreValue>>"
    )
    four_args = "OneMinus<HistogramQuantileAffinity<RegionGraphType, 50, ScoreValue, 256>>"
    assert normalize(four_args) == normalize(four_args.replace("256>", "256, true>"))
    assert normalize(four_args) != normalize(four_args.replace("256>", "256, false>"))
    # only whitespace around punctuation is insignificant
    assert normalize("Foo<unsigned  int>") == "Foo<unsigned int>"


@pytest.mark.parametrize(
    ("scoring_function", "discretize_queue"),
    [
        ("OneMinus<MaxAffinity<RegionGraphType, ScoreValue>>", 0),
        ("OneMinus<HistogramQuantileAffinity<RegionGraphType, 50, ScoreValue, 128>>", 0),
        (MEAN, 128),
    ],
)
def test_unlisted_variants_are_not_prebuilt(
    scoring_function: str, discretize_queue: int
) -> None:
    assert _load_prebuilt(scoring_function, discretize_queue) is None


def test_no_prebuilt_env_var_forces_jit(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("WATERZ_NO_PREBUILT", "1")
    assert _load_prebuilt(MEAN, 0) is None


def test_missing_witty_is_explained(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setitem(sys.modules, "witty", None)
    with pytest.raises(ImportError, match=r"waterz\[jit\]"):
        wz.agglomerate(np.zeros((3, 2, 2, 2), np.float32), [0.5], discretize_queue=7)


@requires_jit
@requires_prebuilt
@pytest.mark.parametrize(
    "spec",
    [
        Spec(MEAN, 0),
        Spec(MEAN, 256),
        Spec("OneMinus<HistogramQuantileAffinity<RegionGraphType, 50, ScoreValue, 256>>", 0),
        Spec(
            "OneMinus<HistogramQuantileAffinity<RegionGraphType, 85, ScoreValue, 256, false>>",
            256,
        ),
    ],
    ids=lambda s: module_name(*s),
)
def test_prebuilt_and_jit_agree(spec: Spec, monkeypatch: pytest.MonkeyPatch) -> None:
    prebuilt = _agglomerate_all(spec)
    monkeypatch.setenv("WATERZ_NO_PREBUILT", "1")
    jit = _agglomerate_all(spec)
    for (seg_a, *rest_a), (seg_b, *rest_b) in zip(prebuilt, jit, strict=True):
        np.testing.assert_array_equal(seg_a, seg_b)
        assert rest_a == rest_b
