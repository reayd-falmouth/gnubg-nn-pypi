project(
    'gnubg',                      # project name
    ['c', 'cpp'],                 # languages
    version: '1.1',
    default_options: [
        'cpp_std=c++14',            # C++ standard
    ],
)

# -----------------------------------------------------------------------------
# 1) Find Python
# -----------------------------------------------------------------------------
py_mod = import('python')
py3 = py_mod.find_installation('python3')

# -----------------------------------------------------------------------------
# 2) Common includes & compiler flags
# -----------------------------------------------------------------------------
common_inc = include_directories(
    'gnubg-nn',
    'gnubg-nn/gnubg',
    'gnubg-nn/gnubg/lib',
    'gnubg-nn/analyze',
    'gnubg-nn/py',
    'src'                         # for src/gnubg/**
)

# platform-specific defines
defines = [
    ['_CRT_SECURE_NO_WARNINGS', '1'],
    ['NOMINMAX',               '1'],
    ['LOADED_BO',              '1'],
    ['OS_BEAROFF_DB',          '1'],
    ['HAVE_CONFIG_H',          '1'],
]
# turn them into -D flags:
c_defs = []
foreach d : defines
    c_defs += ['-D' + d.get(0) + '=' + d.get(1)]
endforeach


# windows extra warnings-suppressors, else empty
c_warnings = []
cpp_warnings = []
if host_machine.system() == 'windows'
    c_warnings = ['/wd4244','/wd4305','/wd4028','/wd4090']
endif

# -----------------------------------------------------------------------------
# 3) Gather sources
# -----------------------------------------------------------------------------
c_srcs = files(
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

cpp_srcs = files(
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
    'gnubg-nn/gnubg/osr.cc'
)

all_srcs = c_srcs + cpp_srcs

# -----------------------------------------------------------------------------
# 4) Build the Python extension
# -----------------------------------------------------------------------------
py_ext = py_mod.extension_module(
    module_name:    'gnubg.gnubg',
    sources:        all_srcs,
    include_directories: common_inc,
    c_args:         c_defs + c_warnings,
    cpp_args:       ['-std=c++14'] + cpp_warnings,
    dependencies:   py3.dependency(),
    install:        true,
)

# -----------------------------------------------------------------------------
# 5) Install package data (the .bd/.weights/.db files)
# -----------------------------------------------------------------------------
data_files = files(
    meson.glob('src/gnubg/data/*.bd')     +
    meson.glob('src/gnubg/data/*.weights')+
    meson.glob('src/gnubg/data/*.db')
)

install_data(
    data_files,
    install_dir: py3.get_install_dir() + '/gnubg/data'
)
