# waterz

[![License](https://img.shields.io/pypi/l/waterz.svg?color=green)](https://github.com/funkey/waterz/raw/master/LICENSE)
[![PyPI](https://img.shields.io/pypi/v/waterz.svg?color=green)](https://pypi.org/project/waterz)
[![Python Version](https://img.shields.io/pypi/pyversions/waterz.svg?color=green)](https://python.org)
[![CI](https://github.com/funkey/waterz/actions/workflows/ci.yml/badge.svg)](https://github.com/funkey/waterz/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/funkey/waterz/branch/main/graph/badge.svg?token=qGnz9GXpEb)](https://codecov.io/gh/funkey/waterz)

Pronounced *water-zed*. A simple watershed and region agglomeration library for
affinity graphs.

Based on the watershed implementation of [Aleksandar
Zlateski](https://bitbucket.org/poozh/watershed) and [Chandan
Singh](https://github.com/TuragaLab/zwatershed).

## Install from PyPI

```sh
pip install waterz
```

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

The version is derived from the git tag:

```sh
git tag v0.9.7
git push upstream v0.9.7
```
