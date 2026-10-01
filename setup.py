"""Compile `evaluate` and the agglomerate variants into a stable-ABI wheel.

Renders the same pyx wrapper the runtime would JIT compile (via
`waterz._codegen`) for every variant in `iter_specs()`, so the two paths are
generated from one source.

Set `WATERZ_NO_PREBUILT=1` to skip the agglomerate variants (they are then
compiled on first use, as are all variants that are not prebuilt).
"""

from __future__ import annotations

import importlib.util
import os
import sys
import tempfile
from pathlib import Path

from setuptools import Extension, setup

ROOT = Path(__file__).parent
SRC = ROOT / "src" / "waterz"
BUILD = Path("build")  # extension sources have to be relative paths

# Typed memoryviews compile to PyObject_GetBuffer/PyBuffer_Release, which
# entered the limited API in 3.11.
ABI3_TAG = "cp311"
ABI3_HEX = "0x030B0000"

WIN = sys.platform == "win32"

# Rendering and cythonizing every variant is wasted work for commands that
# only want metadata.
METADATA_ONLY = {"egg_info", "dist_info", "sdist"}
NEEDS_EXTENSIONS = {
    "bdist_egg",
    "bdist_wheel",
    "build",
    "build_ext",
    "build_py",
    "develop",
    "editable_wheel",
    "install",
}


def load_codegen():
    """Load `waterz._codegen` by path, the package is not importable yet."""
    spec = importlib.util.spec_from_file_location("_codegen", SRC / "_codegen.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def metadata_only() -> bool:
    """Whether this invocation asks for nothing that needs the extensions."""
    seen = {arg for arg in sys.argv[1:] if arg in METADATA_ONLY | NEEDS_EXTENSIONS}
    return bool(seen) and seen <= METADATA_ONLY


def check_boost(include_dirs: list[str]) -> None:
    """Fail with a readable message if the boost headers cannot be found.

    Without this, a missing boost is a compiler error for every one of the
    variants, at once.
    """
    from setuptools._distutils.ccompiler import new_compiler
    from setuptools._distutils.sysconfig import customize_compiler

    with tempfile.TemporaryDirectory() as tmp:
        probe = Path(tmp, "probe.cpp")
        probe.write_text("#include <boost/multi_array.hpp>\n", encoding="utf-8")
        compiler = new_compiler()
        customize_compiler(compiler)
        try:
            compiler.compile([str(probe)], output_dir=tmp, include_dirs=include_dirs)
        except Exception as e:  # no compiler, or no boost: either way
            raise SystemExit(
                "waterz needs the boost headers (boost/multi_array.hpp) to build:\n"
                "  linux:   apt install libboost-dev\n"
                "  macos:   brew install boost\n"
                "  windows: vcpkg install boost-multi-array:x64-windows\n"
                "or point BOOST_ROOT (or BOOST_INCLUDEDIR) to where they are.\n"
                f"({e})"
            ) from None


def extensions() -> list[Extension]:
    """Declare `evaluate` and every prebuilt agglomerate variant."""
    from Cython.Build import cythonize

    codegen = load_codegen()
    if codegen.env_enabled("WATERZ_NO_PREBUILT") and codegen.env_enabled(
        "WATERZ_REQUIRE_PREBUILT"
    ):
        raise SystemExit("WATERZ_NO_PREBUILT and WATERZ_REQUIRE_PREBUILT are both set")
    check_boost(codegen.include_dirs())
    common = {
        # the wrappers `#include` these, so they never appear in `sources`
        "depends": codegen.depends(),
        "include_dirs": codegen.include_dirs(),
        "language": "c++",
        # no debug info: it makes for linux wheels of ten times the size
        "extra_compile_args": [*codegen.COMPILE_ARGS, *([] if WIN else ["-g0"])],
        "define_macros": [*codegen.DEFINE_MACROS, ("Py_LIMITED_API", ABI3_HEX)],
        "py_limited_api": True,
    }
    evaluate_sources = [SRC / "evaluate.pyx", SRC / "frontend_evaluate.cpp"]
    sources = [str(s.relative_to(ROOT)) for s in evaluate_sources]
    modules = [Extension("waterz.evaluate", sources=sources, **common)]

    # setuptools packages whatever an earlier build left in `build/lib*`
    prebuilt = not codegen.env_enabled("WATERZ_NO_PREBUILT")
    for stale in BUILD.glob("lib*/waterz/_prebuilt/*"):
        keep = prebuilt and stale.name.split(".")[0] == "agglomerate"
        if stale.suffix in (".so", ".pyd") and not keep:
            stale.unlink()
    if prebuilt:
        pyx_dir = BUILD / "prebuilt-pyx"
        pyx_dir.mkdir(parents=True, exist_ok=True)
        source = codegen.build_wrapper(codegen.iter_specs())
        path = pyx_dir / "agglomerate.pyx"
        # only rewrite when changed, so cythonize can skip it
        if not path.is_file() or path.read_text(encoding="utf-8") != source:
            path.write_text(source, encoding="utf-8")
        modules.append(Extension(codegen.PREBUILT_MODULE, sources=[str(path)], **common))

    return cythonize(
        modules,
        language_level=3,
        quiet=True,
        build_dir=str(BUILD),
        nthreads=0 if WIN else os.cpu_count(),
    )


if __name__ == "__main__":
    if metadata_only():
        setup()
    else:
        setup(
            ext_modules=extensions(),
            options={
                "bdist_wheel": {"py_limited_api": ABI3_TAG},
                "build_ext": {"parallel": os.cpu_count()},
            },
        )
