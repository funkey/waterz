import sys
import tempfile
from pathlib import Path

import numpy
from Cython.Build import cythonize
from setuptools import Extension, setup

include_dirs = [
    "src/waterz",
    "src/waterz/backend",
    numpy.get_include(),
]
if sys.platform == "darwin":
    include_dirs.append("/opt/homebrew/include")  # homebrew ... for boost

evaluate = Extension(
    name="waterz.evaluate",
    sources=[
        "src/waterz/evaluate.pyx",
        "src/waterz/frontend_evaluate.cpp",
    ],
    include_dirs=include_dirs,
    language="c++",
    extra_link_args=["-std=c++11"],
    extra_compile_args=["-std=c++11", "-w"],
)

# Pre-compile the agglomeration modules for the (scoring, queue) combinations
# used in production so they never need runtime JIT (witty) compilation on
# workers -- which avoids the concurrent-compile races that can corrupt the
# witty cache. Each variant bakes its template parameters in via generated
# headers and gets a uniquely-named .pyx copy so cythonize emits distinct
# modules (the same .pyx compiled twice would collide on its PyInit symbol).
_MEAN_SCORING = "typedef OneMinus<MeanAffinity<RegionGraphType, ScoreValue>> ScoringFunctionType;"
_PRIORITY_QUEUE = "template<typename T, typename S> using QueueType = PriorityQueue<T, S>;"
_BIN_QUEUE_256 = "template<typename T, typename S> using QueueType = BinQueue<T, S, 256>;"

_agglomerate_pyx_src = Path("src/waterz/agglomerate.pyx").read_text()
# Per-variant .pyx copies must live at a relative path (setuptools rejects
# absolute paths in Extension.sources); generated headers may stay absolute.
_gen_pyx_dir = Path("_agglomerate_gen")
_gen_pyx_dir.mkdir(exist_ok=True)


def _agglomerate_ext(module_name, queue_decl):
    headers = Path(tempfile.mkdtemp())
    (headers / "ScoringFunction.h").write_text(_MEAN_SCORING)
    (headers / "Queue.h").write_text(queue_decl)
    pyx_path = _gen_pyx_dir / f"{module_name}.pyx"
    pyx_path.write_text(_agglomerate_pyx_src)
    return Extension(
        name=f"waterz.{module_name}",
        sources=[str(pyx_path), "src/waterz/frontend_agglomerate.cpp"],
        include_dirs=include_dirs + [str(headers)],
        language="c++",
        extra_link_args=["-std=c++11"],
        extra_compile_args=["-std=c++11", "-w", "-O3"],
    )


# (MeanAffinity, PriorityQueue)  -> discretize_queue == 0
agglomerate_default = _agglomerate_ext("_agglomerate_default", _PRIORITY_QUEUE)
# (MeanAffinity, BinQueue<256>) -> discretize_queue == 256 (production agglomeration)
agglomerate_bin256 = _agglomerate_ext("_agglomerate_mean_bin256", _BIN_QUEUE_256)

setup(ext_modules=cythonize([evaluate, agglomerate_default, agglomerate_bin256]))
