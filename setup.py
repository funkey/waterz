from setuptools import setup, find_packages
from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext as _build_ext
#from Cython.Build import cythonize
import os

version = '0.9.0'

PACKAGE_DIR = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(PACKAGE_DIR, "requirements.txt")) as f:
    requirements = f.read().splitlines()
    requirements = [l for l in requirements if not l.startswith('#')]


with open(os.path.join(PACKAGE_DIR, "README.md"), "r") as fh:
    long_description = fh.read()

class build_ext(_build_ext):
    """We assume no numpy at the begining
    https://stackoverflow.com/questions/19919905/how-to-bootstrap-numpy-installation-in-setup-py
    """
    def finalize_options(self):
        _build_ext.finalize_options(self)
        # Prevent numpy from thinking it is still in its setup process:
        __builtins__.__NUMPY_SETUP__ = False
        import numpy
        self.include_dirs.append(numpy.get_include())

source_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'waterz')
include_dirs = [
    source_dir,
    os.path.join(source_dir, 'backend'),
    # os.path.dirname(get_python_inc()),
    # numpy.get_include(),
]
extensions = [
    Extension(
        'waterz.evaluate',
        sources=['waterz/evaluate.cpp', 'waterz/frontend_evaluate.cpp'],
        include_dirs=include_dirs,
        language='c++',
        extra_link_args=['-std=c++11'],
        extra_compile_args=['-std=c++11', '-w'])
]

setup(
    name='waterz',
    version=version,
    description='Simple watershed and agglomeration for affinity graphs.',
    long_description=long_description,
    long_description_content_type="text/markdown",
    url='https://github.com/funkey/waterz',
    author='Jan Funke, Jingpeng Wu',
    author_email='jfunke@iri.upc.edu',
    license='MIT',
    cmdclass={'build_ext': build_ext},
    setup_requires=['numpy'],
    install_requires=requirements,
    tests_require=['pytest'],
    packages=find_packages(),
    zip_safe=False,
    ext_modules=extensions,
    classifiers=[
        "Programming Language :: Python :: 3",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ]
)
