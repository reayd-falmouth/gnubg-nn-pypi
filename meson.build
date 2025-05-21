project('gnubg', 'c', 'cpp', version: '1.1.0', default_options: ['cpp_std=c++14'])

py = import('python').find_installation()
common_inc = include_directories(
    'gnubg-nn',
    'gnubg-nn/gnubg',
    'gnubg-nn/gnubg/lib',
    'gnubg-nn/analyze',
    'gnubg-nn/py',
    'src'
)

defines = [
    ['_CRT_SECURE_NO_WARNINGS', '1'],
    ['NOMINMAX', '1'],
    ['LOADED_BO', '1'],
    ['OS_BEAROFF_DB', '1'],
    ['HAVE_CONFIG_H', '1']
]

c_defs = []
foreach d : defines
    c_defs += ['-D' + d.get(0) + '=' + d.get(1)]
endforeach

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
    'gnubg-nn/gnubg/osr.cc'
)

py.extension_module(
    'gnubg.gnubg',
    sources: c_sources + cpp_sources,
    include_directories: common_inc,
    c_args: c_defs,
    dependencies: py.dependency(),
    install: true
)
