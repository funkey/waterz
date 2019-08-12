from setuptools import setup, find_packages
from setuptools.extension import Extension
from Cython.Build import cythonize
import numpy
import os

version = '0.8.2'

with open('requirements.txt') as f:
    requirements = f.read().splitlines()
    requirements = [l for l in requirements if not l.startswith('#')]


with open("README.md", "r") as fh:
    long_description = fh.read()

source_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'waterz')
include_dirs = [
    source_dir,
    os.path.join(source_dir, 'backend'),
    # os.path.dirname(get_python_inc()),
    numpy.get_include(),
]
extensions = [
    Extension(
        'waterz.evaluate',
        sources=['waterz/evaluate.pyx', 'waterz/frontend_evaluate.cpp'],
        include_dirs=include_dirs,
        language='c++',
        extra_link_args=['-std=c++11'],
        extra_compile_args=['-std=c++11', '-w'])
]

setup(
    name='waterzed',
    version=version,
    description='Simple watershed and agglomeration for affinity graphs.',
    long_description=long_description,
    long_description_content_type="text/markdown",
    url='https://github.com/funkey/waterz',
    author='Jan Funke, Jingpeng Wu',
    author_email='jfunke@iri.upc.edu',
    license='MIT',
    install_requires=requirements,
    tests_require=['pytest'],
    packages=find_packages(),
    package_data={
	'': [
	    'waterz/*.h',
	    'waterz/*.hpp',
	    'waterz/*.cpp',
	    'waterz/*.cpp',
	    'waterz/*.pyx',
	    'waterz/backend/*.hpp',
	]
    },
    include_package_data=True,
    zip_safe=False,
    ext_modules=cythonize(extensions),
    classifiers=[
        "Programming Language :: Python :: 3",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ]
)
