def agglomerate(
        affs,
        thresholds,
        gt = None,
        aff_threshold_low  = 0.0001,
        aff_threshold_high = 0.9999,
        scoring_function = 'Multiply<OneMinus<MaxAffinity<AffinitiesType>>, MinSize<SizesType>>',
        force_rebuild = False):
    '''
    Compute segmentations from an affinity graph for several thresholds.

    Passed volumes need to be converted into contiguous memory arrays. This will
    be done for you if needed, but you can save memory by making sure your
    volumes are already C_CONTIGUOUS.

    Parameters
    ----------

        affs: numpy array, float32, 4 dimensional

            The affinities as an array with affs[channel][z][y][x].

        thresholds: list of float32

            The thresholds to compute segmentations for. For each threshold, one
            segmentation is returned.

        gt: numpy array, uint32, 3 dimensional (optional)

            An optional ground-truth segmentation as an array with gt[z][y][x].
            If given, metrics

        aff_threshold_low: float, default 0.0001
        aff_threshold_high: float, default 0.9999,

            Thresholds on the affinities for the initial segmentation step.

        scoring_function: string, default 'Multiply<OneMinus<MaxAffinity<AffinitiesType>>, MinSize<SizesType>>'

            A C++ type string specifying the edge scoring function to use. See

                https://github.com/funkey/waterz/blob/master/waterz/backend/MergeFunctions.hpp

            for available functions, and

                https://github.com/funkey/waterz/blob/master/waterz/backend/Operators.hpp

            for operators to combine them.

    Returns
    -------

        Segmentations (and metrics) are returned as generator objects, and only
        computed on-the-fly when iterated over. This way, you can ask for
        hundreds of thresholds while at any point only one segmentation is
        stored in memory.

        [segmentation]

            Generator object for segmentations (numpy arrays, uint64, 3 dimensional).

        [(segmentation, metrics)]

            Generator object for tuples of segmentations and metrics, if ground-truth volume was
            given. Metrics are given as a dictionary with the keys
            'V_Rand_split', 'V_Rand_merge', 'V_Info_split', and 'V_Info_merge'.
    '''

    import sys, os
    import shutil
    import glob
    import numpy
    import fcntl

    try:
        import hashlib
    except ImportError:
        import md5 as hashlib

    from distutils.core import Distribution, Extension
    from distutils.command.build_ext import build_ext
    from distutils.sysconfig import get_config_vars, get_python_inc

    import Cython
    from Cython.Compiler.Main import Context, default_options
    from Cython.Build.Dependencies import cythonize

    # compile frontend.pyx for given scoring function

    source_dir = os.path.dirname(os.path.abspath(__file__))
    source_files = [
            os.path.join(source_dir, 'frontend.pyx'),
            os.path.join(source_dir, 'c_frontend.h'),
            os.path.join(source_dir, 'c_frontend.cpp'),
            os.path.join(source_dir, 'evaluate.hpp')
    ]
    source_files += glob.glob(source_dir + '/backend/*.hpp')
    source_files.sort()
    source_files_hashes = [ hashlib.md5(open(f, 'r').read().encode('utf-8')).hexdigest() for f in source_files ]

    key = scoring_function, source_files_hashes, sys.version_info, sys.executable, Cython.__version__
    module_name = 'waterz_' + hashlib.md5(str(key).encode('utf-8')).hexdigest()
    lib_dir=os.path.expanduser('~/.cython/inline')

    # since this could be called concurrently, there is no good way to check
    # whether the directory already exists
    try:
        os.makedirs(lib_dir)
    except:
        pass

    # make sure the same module is not build concurrently
    with open(os.path.join(lib_dir, module_name + '.lock'), 'w') as lock_file:
        fcntl.lockf(lock_file, fcntl.LOCK_EX)

        try:

            if lib_dir not in sys.path:
                sys.path.append(lib_dir)
            if force_rebuild:
                raise ImportError
            else:
                __import__(module_name)

            print("Re-using already compiled waterz version")

        except ImportError:

            print("Compiling waterz in " + str(lib_dir))

            cython_include_dirs = ['.']
            ctx = Context(cython_include_dirs, default_options)

            scoring_function_include_dir = os.path.join(lib_dir, module_name)
            if not os.path.exists(scoring_function_include_dir):
                os.makedirs(scoring_function_include_dir)

            include_dirs = [
                source_dir,
                scoring_function_include_dir,
                os.path.join(source_dir, 'backend'),
                os.path.dirname(get_python_inc()),
                numpy.get_include(),
            ]

            scoring_function_header = os.path.join(scoring_function_include_dir, 'ScoringFunction.h')
            with open(scoring_function_header, 'w') as f:
                f.write('typedef %s ScoringFunctionType;'%scoring_function)

            # cython requires that the pyx file has the same name as the module
            shutil.copy(
                    os.path.join(source_dir, 'frontend.pyx'),
                    os.path.join(lib_dir, module_name + '.pyx')
            )
            shutil.copy(
                    os.path.join(source_dir, 'c_frontend.cpp'),
                    os.path.join(lib_dir, module_name + '_c_frontend.cpp')
            )

            # Remove the "-Wstrict-prototypes" compiler option, which isn't valid 
            # for C++.
            cfg_vars = get_config_vars()
            if "CFLAGS" in cfg_vars:
                cfg_vars["CFLAGS"] = cfg_vars["CFLAGS"].replace("-Wstrict-prototypes", "")

            extension = Extension(
                    module_name,
                    sources = [
                        os.path.join(lib_dir, module_name + '.pyx'),
                        os.path.join(lib_dir, module_name + '_c_frontend.cpp')
                    ],
                    include_dirs=include_dirs,
                    language='c++',
                    extra_link_args=['-std=c++11'],
                    extra_compile_args=['-std=c++11', '-w']
            )
            build_extension = build_ext(Distribution())
            build_extension.finalize_options()
            build_extension.extensions = cythonize([extension], quiet=True, nthreads=2)
            build_extension.build_temp = lib_dir
            build_extension.build_lib  = lib_dir
            build_extension.run()

    return __import__(module_name).agglomerate(affs, thresholds, gt, aff_threshold_low, aff_threshold_high)
