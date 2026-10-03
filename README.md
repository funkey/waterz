# waterz

[![License](https://img.shields.io/pypi/l/waterz.svg?color=green)](https://github.com/funkey/waterz/raw/main/LICENSE)
[![PyPI](https://img.shields.io/pypi/v/waterz.svg?color=green)](https://pypi.org/project/waterz)
[![Python Version](https://img.shields.io/pypi/pyversions/waterz.svg?color=green)](https://python.org)
[![CI](https://github.com/funkey/waterz/actions/workflows/ci.yml/badge.svg)](https://github.com/funkey/waterz/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/funkey/waterz/branch/main/graph/badge.svg?token=qGnz9GXpEb)](https://codecov.io/gh/funkey/waterz)

Pronounced *water-zed*. A simple watershed and region agglomeration library for
affinity graphs.

Based on the watershed implementation of [Aleksandar
Zlateski](https://bitbucket.org/poozh/watershed) and [Chandan
Singh](https://github.com/TuragaLab/zwatershed).

## Install

```sh
pip install waterz
```

The wheels on PyPI contain `waterz.evaluate` and `waterz.agglomerate`,
precompiled for the most common scoring functions (see below). No compiler is
needed to use those.

### Scoring functions

The `scoring_function` and `discretize_queue` arguments of `agglomerate` are
C++ template parameters, such that each combination is a separate compiled
module. The following are precompiled, each with `discretize_queue=0` and
`discretize_queue=256`:

- `OneMinus<MeanAffinity<RegionGraphType, ScoreValue>>` (the default)
- `OneMinus<HistogramQuantileAffinity<RegionGraphType, Q, ScoreValue, 256, B>>`
  for `Q` in 10, 15, 20, ..., 95 and `B` in `true`, `false` (and
  `OneMinus<HistogramQuantileAffinity<RegionGraphType, Q, ScoreValue, 256>>`,
  which is the same as `B = true`)
- `Divide<Subtract<MaxSize<RegionGraphType>, MinSize<RegionGraphType>>, Add<MaxSize<RegionGraphType>, MinSize<RegionGraphType>>>`
  (the relative difference of the region sizes, ignoring the affinities)

Any other combination is compiled the first time it is used, which requires a
C++ compiler and the boost headers (see below). The compiled module is cached
(in the cache directory of [witty](https://github.com/funkelab/witty), which can
be set with `WITTY_CACHE_DIR`).

Set `WATERZ_NO_PREBUILT=1` to compile all scoring functions on first use,
ignoring the precompiled ones.

## Install locally

Install c++ dependencies:

```sh
# linux
sudo apt install libboost-dev
# macos
brew install boost
# windows
vcpkg install boost-multi-array:x64-windows
```

If boost is installed elsewhere, point `BOOST_ROOT` (or `BOOST_INCLUDEDIR`) to it.

Then

```sh
# make and activate env then:
pip install -e .
```

or

```sh
uv sync
```

Set `WATERZ_NO_PREBUILT=1` to skip precompiling the scoring functions (all of
them are then compiled on first use), which makes for a faster build. After
changing the C++, run `uv sync` (or `pip install -e .`) again: the precompiled
modules do not notice, the compiled-on-first-use ones do.

## Usage

```python
import waterz
import numpy as np

# affinities is a [3,depth,height,width] numpy array of float32
affinities = ...

thresholds = [0, 100, 200]

segmentations = waterz.agglomerate(affinities, thresholds)
```

## Development

### Release to pypi

The version is derived from the git tag. Pushing a tag builds the wheels,
tests them, and publishes them to PyPI:

```sh
git tag -a v0.9.7 -m v0.9.7
git push upstream --follow-tags
```
