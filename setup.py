import importlib.util
from pathlib import Path

from Cython.Build import cythonize
from setuptools import Extension, setup

SRC = Path("src/waterz")


def load_codegen():
    """Load `waterz._codegen` by path, the package is not importable yet."""
    spec = importlib.util.spec_from_file_location("_codegen", SRC / "_codegen.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


if __name__ == "__main__":
    codegen = load_codegen()
    evaluate = Extension(
        name="waterz.evaluate",
        sources=[str(SRC / "evaluate.pyx"), str(SRC / "frontend_evaluate.cpp")],
        depends=codegen.depends(),
        include_dirs=codegen.include_dirs(),
        language="c++",
        extra_compile_args=codegen.COMPILE_ARGS,
        define_macros=codegen.DEFINE_MACROS,
    )
    setup(ext_modules=cythonize([evaluate], language_level=3, build_dir="build"))
