from setuptools import setup

setup(
        name='waterz',
        version='0.3',
        description='Simple watershed and agglomeration for affinity graphs.',
        url='https://github.com/funkey/waterz',
        author='Jan Funke',
        author_email='jfunke@iri.upc.edu',
        license='MIT',
        requires=['cython','numpy'],
        packages=['waterz'],
        package_data={
            '': [
                'waterz/*.h',
                'waterz/*.hpp',
                'waterz/*.cpp',
                'waterz/*.pyx',
                'waterz/backend/*.hpp',
            ]
        },
        include_package_data=True,
        zip_safe=False
)
