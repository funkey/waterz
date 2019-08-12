[![Build Status](https://travis-ci.org/jingpengw/waterz.svg?branch=master)](https://travis-ci.org/jingpengw/waterz)
[![PyPI version](https://badge.fury.io/py/waterzed.svg)](https://badge.fury.io/py/waterzed)

<!--
    the coerage is not working on c++ code, so the coverage is 0 now!
    [![Coverage Status](https://coveralls.io/repos/github/jingpengw/waterz/badge.svg?branch=master)](https://coveralls.io/github/jingpengw/waterz?branch=master)
-->

# waterz

Pronounced *water-zed*. A simple watershed and region agglomeration library for
affinity graphs.

Based on the watershed implementation of [Aleksandar Zlateski](https://bitbucket.org/poozh/watershed) and [Chandan Singh](https://github.com/TuragaLab/zwatershed).

# Installation

Install c++ dependencies:
```
sudo apt install libboost-dev
```

Install from pipy
```
pip install waterzed
```

install manually
```
pip install -r requirements.txt
python setup.py install
```

# Usage

```
import waterz
import numpy as np

# affinities is a [3,depth,height,width] numpy array of float32
affinities = ...

thresholds = [0, 100, 200]

segmentations = waterz.agglomerate(affinities, thresholds)
```

# Development
## Release to pypi

We can only release code and compile in the installation machine.
```
python setup.py sdist
twine upload dist/my-new-wheel
```
