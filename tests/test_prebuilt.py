"""Tests for ahead-of-time compiled agglomerate modules.

The `requires_prebuilt` tests only mean something against an install that
actually shipped them, and are skipped otherwise -- except when
`WATERZ_REQUIRE_PREBUILT` is set (as CI does), where their absence is the very
regression we want to catch.
"""

from __future__ import annotations

import os
import subprocess
import sys
from importlib.util import find_spec

import numpy as np
import pytest

import waterz as wz
from waterz import _agglomerate
from waterz._agglomerate import _load_prebuilt
from waterz._codegen import (
    PREBUILT_MODULE,
    Spec,
    build_wrapper,
    env_enabled,
    iter_specs,
    normalize,
)

MEAN = "OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>"

has_prebuilt = _load_prebuilt(MEAN, 0) is not None
requires_prebuilt = pytest.mark.skipif(
    not has_prebuilt, reason="no prebuilt modules in this install"
)
requires_jit = pytest.mark.skipif(find_spec("witty") is None, reason="needs waterz[jit]")


def _agglomerate_all(spec: Spec, **kwargs: object) -> list[tuple]:
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
            **kwargs,
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
@pytest.mark.parametrize(
    "spec", list(iter_specs()), ids=lambda s: f"{s.scoring_function}|{s.discretize_queue}"
)
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


@requires_prebuilt
@pytest.mark.parametrize(
    "discretize_queue", ["256", " 256 ", 256.0, np.float32(256), np.uint8(0), "0"]
)
def test_queue_as_string_or_float_is_prebuilt(discretize_queue: object) -> None:
    compiled = _load_prebuilt(MEAN, discretize_queue)
    assert compiled is not None
    module, variant = compiled
    assert module.SPECS[variant][1] == str(int(float(discretize_queue)))


@pytest.mark.parametrize("discretize_queue", [0.5, "0x100", "256.0", "", None, [256]])
def test_queue_that_is_not_an_integer_is_not_prebuilt(discretize_queue: object) -> None:
    assert _load_prebuilt(MEAN, discretize_queue) is None


def test_no_prebuilt_env_var_forces_jit(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("WATERZ_NO_PREBUILT", "1")
    assert _load_prebuilt(MEAN, 0) is None


def test_missing_witty_is_explained(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setitem(sys.modules, "witty", None)
    with pytest.raises(ImportError, match="discretize_queue=7 requires witty"):
        wz.agglomerate(np.zeros((3, 2, 2, 2), np.float32), [0.5], discretize_queue=7)


@requires_prebuilt
def test_prebuilt_module_records_its_parameters() -> None:
    for spec in iter_specs():
        compiled = _load_prebuilt(*spec)
        assert compiled is not None
        module, variant = compiled
        assert module.SPECS[variant] == (spec.scoring_function, str(spec.discretize_queue))
    assert len({_load_prebuilt(*spec)[1] for spec in iter_specs()}) == len(list(iter_specs()))


@requires_prebuilt
def test_prebuilt_module_compiled_for_something_else(monkeypatch: pytest.MonkeyPatch) -> None:
    module, variant = _load_prebuilt(MEAN, 0)
    specs = list(module.SPECS)
    specs[variant] = ("Something<Else>", "0")
    monkeypatch.setattr(module, "SPECS", specs)
    with pytest.raises(RuntimeError, match="was compiled for"):
        _load_prebuilt(MEAN, 0)


def test_unlisted_variant_does_not_load_the_module(monkeypatch: pytest.MonkeyPatch) -> None:
    def no_importing(name: str, *args: object) -> None:
        raise AssertionError(f"imported {name}")

    monkeypatch.setattr(_agglomerate.importlib, "import_module", no_importing)
    assert _load_prebuilt("OneMinus<MaxAffinity<RegionGraphType, ScoreValue>>", 0) is None
    assert _load_prebuilt(MEAN, 7) is None


def test_rendered_variants_line_up() -> None:
    """The table, the namespaces, and SPECS come from the same list, in order."""
    specs = list(iter_specs())
    source = build_wrapper(specs)
    for variant, spec in enumerate(specs):
        block = source[source.index(f"namespace v{variant} {{") :]
        assert f"typedef {spec.scoring_function} ScoringFunctionType;" in block.split("}")[0]
        assert f"{{v{variant}::initialize, v{variant}::mergeUntil" in source
    assert source.count("namespace v") == len(specs)
    rendered = eval(source[source.index("SPECS = ") + 8 :].split("\n")[0])
    assert rendered == [(f, str(q)) for f, q in specs]


@requires_prebuilt
def test_variant_index_is_checked() -> None:
    module, _ = _load_prebuilt(MEAN, 0)
    affs = np.zeros((3, 2, 2, 2), np.float32)
    for variant in (len(module.SPECS), -1, 10**9):
        with pytest.raises((IndexError, OverflowError)):
            next(module.agglomerate(variant, affs, [0.5], None, None, 0, 1, False, False))


@requires_jit
@requires_prebuilt
@pytest.mark.skipif(
    not env_enabled("WATERZ_TEST_ALL_VARIANTS"), reason="set WATERZ_TEST_ALL_VARIANTS=1"
)
@pytest.mark.parametrize(
    "spec", list(iter_specs()), ids=lambda s: f"{s.scoring_function}|{s.discretize_queue}"
)
def test_every_prebuilt_variant_agrees_with_jit(
    spec: Spec, monkeypatch: pytest.MonkeyPatch
) -> None:
    test_prebuilt_and_jit_agree(spec, monkeypatch)


@requires_prebuilt
def test_broken_prebuilt_module_is_not_hidden(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delitem(sys.modules, PREBUILT_MODULE, raising=False)

    def broken_import(name: str, *args: object) -> None:
        raise ModuleNotFoundError("No module named 'something_else'", name="something_else")

    monkeypatch.setattr(_agglomerate.importlib, "import_module", broken_import)
    with pytest.raises(ModuleNotFoundError, match="something_else"):
        _load_prebuilt(MEAN, 0)


@requires_prebuilt
def test_prebuilt_does_not_import_witty() -> None:
    script = (
        "import sys, numpy as np, waterz\n"
        "next(waterz.agglomerate(np.random.rand(3, 4, 8, 8).astype(np.float32), [0.5]))\n"
        "print(sorted(m for m in sys.modules if m.split('.')[0] in "
        "('witty', 'Cython', 'setuptools', 'distutils', 'nanobind')))\n"
    )
    env = {**os.environ, "WATERZ_NO_PREBUILT": "0"}
    out = subprocess.run(
        [sys.executable, "-c", script], capture_output=True, text=True, env=env
    )
    assert out.returncode == 0, out.stderr
    assert out.stdout.strip().endswith("[]"), out.stdout


@requires_jit
@requires_prebuilt
def test_force_rebuild_compiles(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[tuple] = []
    compile_ = _agglomerate._jit_compile

    def recording(*args: object) -> object:
        calls.append(args)
        return compile_(*args)

    monkeypatch.setattr(_agglomerate, "_jit_compile", recording)
    expected = _agglomerate_all(Spec(MEAN, 0))
    assert calls == []
    rebuilt = _agglomerate_all(Spec(MEAN, 0), force_rebuild=True)
    assert calls == [(MEAN, 0, True)]
    for (seg_a, *rest_a), (seg_b, *rest_b) in zip(expected, rebuilt, strict=True):
        np.testing.assert_array_equal(seg_a, seg_b)
        assert rest_a == rest_b


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
    ids=lambda s: f"{s.scoring_function}|{s.discretize_queue}",
)
def test_prebuilt_and_jit_agree(spec: Spec, monkeypatch: pytest.MonkeyPatch) -> None:
    prebuilt = _agglomerate_all(spec)
    monkeypatch.setenv("WATERZ_NO_PREBUILT", "1")
    jit = _agglomerate_all(spec)
    for (seg_a, *rest_a), (seg_b, *rest_b) in zip(prebuilt, jit, strict=True):
        np.testing.assert_array_equal(seg_a, seg_b)
        assert rest_a == rest_b
