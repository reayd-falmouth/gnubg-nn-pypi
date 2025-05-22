project(
    'gnubg',
    ['cpp', 'python'],
    version : '1.1.0.dev4',
    default_options : ['cpp_std=c++14']
)

# 1. Include dirs
gnubg_inc = include_directories(
    'gnubg-nn',
    'gnubg-nn/gnubg',
    'gnubg-nn/gnubg/lib',
    'gnubg-nn/analyze',
    'gnubg-nn/py'
)

# 2. Compiler definitions (macros)
common_defines = [
    '_CRT_SECURE_NO_WARNINGS=1',
    'NOMINMAX=1',
    'LOADED_BO=1',
    'OS_BEAROFF_DB=1',
    'HAVE_CONFIG_H=1',
]
# Apply to both C and C++ compilations
foreach m : common_defines
    add_project_arguments('-D' + m, language: ['c', 'cpp'])
endforeach

# 3. C sources (GNUBG core)
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
    'gnubg-nn/gnubg/lib/nsse.c'
)

# 4. C++ sources (GNUBG analysis) minus the Python binding entrypoint
cpp_lib_sources = files(
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
    'gnubg-nn/gnubg/osr.cc'
)

# 5. Static library combining C + C++ analysis code
gnubg_lib = static_library(
    'gnubg_nn',
    c_sources + cpp_lib_sources,
    include_directories : gnubg_inc
)

# 6. Wrap as a dependency and link everything (“link_whole” avoids missing symbols)
gnubg_dep = declare_dependency(
    link_whole : gnubg_lib,
    include_directories : gnubg_inc
)

# 7. Python extension module
python = import('python')
python.extension_module(
    'gnubg.gnubg',
    sources : ['src/gnubg/gnubgmodule.cpp'],
    dependencies : [gnubg_dep],
    include_directories : gnubg_inc
)

# 8. Install Python package data
python.install_sources('src/gnubg')
install_data(
    files : [
        'src/gnubg/data/bearoffdb.bd',
        'src/gnubg/data/*.weights',
        'src/gnubg/data/*.db'
    ],
    install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'python' + python.get_major_version(), 'site-packages', 'gnubg', 'data')
)
