# meson.build
project(
    'gnubg',                      # Your project name
    ['cpp', 'c'],                 # Languages used
    version     : '1.1.0.dev4',    # Match your setup.py version
    meson_version : '>=1.7.0',     # Require a recent Meson
)

python3 = import('python').find_installation('python3', pure: false)
python_dep = python3.dependency()
pkgdir = python3.get_install_dir()

install_data(
    'src/gnubg/__init__.py',
    install_dir: join_paths(pkgdir, 'gnubg')
)
install_data(
    'src/gnubg/data/gnubg.weights',
    'src/gnubg/data/gnubg_ts0.bd',
    'src/gnubg/data/gnubg_os0.bd',
    'src/gnubg/data/gnubg_os.db',
    install_dir: join_paths(pkgdir, 'gnubg', 'data')
)

conf_data = configuration_data()
conf_data.set('_CRT_SECURE_NO_WARNINGS', 1)  # or 0 to disable
conf_data.set('NOMINMAX', 1)
conf_data.set('LOADED_BO', 1)
conf_data.set('OS_BEAROFF_DB', 1)
conf_data.set('HAVE_CONFIG_H', 1)

config_h = configure_file(
    output: 'config.h',
    configuration: conf_data
)

add_project_arguments(
    '-include', 'config.h',
    language: ['c', 'cpp']
)

# Include directories
gnubg_inc = include_directories(
    'gnubg-nn',
    'gnubg-nn/gnubg',
    'gnubg-nn/gnubg/lib',
    'gnubg-nn/analyze',
    'gnubg-nn/py',
)

# C sources
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

# C++ sources
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

# Build static library bundling all GNUBG engine code
libgnubg_nn = static_library(
    'gnubg_nn',
    c_sources + cpp_sources,
    include_directories : gnubg_inc,
    cpp_args            : ['-std=c++14'],    # match your C++14 requirement
    install             : false,
    dependencies:       [python_dep],  # ✅ use the Python headers!
)

# Force-link every object to avoid missing-symbol errors
gnubg_dep = declare_dependency(
    include_directories : gnubg_inc,
    link_with           : libgnubg_nn,
).as_link_whole()

# Build the Python extension
py_ext = python3.extension_module(
    '_gnubg',
    [],                          # no extra sources; they live in the static lib
    dependencies : [gnubg_dep],
    install      : true,
    subdir       : 'gnubg',
)
