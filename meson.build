# Minimum required Meson version
meson_version = '>=1.7.0'

project(
    'gnubg',
    ['cpp', 'c'],
    version : '1.1.0.dev4',
)

# -- 1) Include directories
gnubg_inc = include_directories(
    'gnubg-nn',
    'gnubg-nn/gnubg',
    'gnubg-nn/gnubg/lib',
    'gnubg-nn/analyze',
    'gnubg-nn/py',
)

# -- 2) Source files
c_sources = files(
    'gnubg-nn/gnubg/bearoffdb.c',
    'gnubg-nn/gnubg/eggmoveg.c',
    'gnubg-nn/gnubg/eval.c',
    'gnubg-nn/gnubg/inputs.c',
    'gnubg-nn/gnubg/mt19937int.c',
    'gnubg-nn/gnubg/positionid.c',
    'gnubg-nn/gnubg/pub_eval.c',
    'gnubg-nn/gnubg/lib/hash.c',
    'gnubg-nn/gnubg/lib/neuralnet.c',
    'gnubg-nn/gnubg/lib/nsse.c',
)

cpp_sources = files(
    'src/gnubg/gnubgmodule.cpp',
    'gnubg-nn/analyze/analyze.cc',
    'gnubg-nn/analyze/bm.cc',
    'gnubg-nn/analyze/bms.cc',
    'gnubg-nn/analyze/danalyze.cc',
    'gnubg-nn/analyze/dice_gen.cc',
    'gnubg-nn/analyze/equities.cc',
    'gnubg-nn/analyze/mec.cc',
    'gnubg-nn/analyze/player.cc',
    'gnubg-nn/gnubg/bearoffgammon.cc',
    'gnubg-nn/gnubg/racebg.cc',
    'gnubg-nn/gnubg/osr.cc',
)

# -- 3) Build a static library for all GNUBG code
libgnubg_nn = static_library(
    'gnubg_nn',
    c_sources + cpp_sources,
    include_directories : gnubg_inc,
    cpp_args : ['-std=c++14'],    # match your C++14 requirement
    install : false,
)

# -- 4) Declare a dependency that links the entire static library
gnubg_dep = declare_dependency(
    include_directories : gnubg_inc,
    link_with : libgnubg_nn,
).as_link_whole()  # pull in all objects to prevent unresolved externals :contentReference[oaicite:5]{index=5}

# -- 5) Build the Python extension module
python = import('python').find_installation()
py_ext = python.extension_module(
    'gnubg.gnubg',
    [],                          # sources are in the static lib
    dependencies : [gnubg_dep],  # link GNUBG engine into the .pyd/.so
    install : true,
)
