import os
from glob import glob
from setuptools import setup, Extension, find_packages

#--- choose which analyze files to exclude:
ANALYZE_EXCLUDE = {
    # "gnubg-nn/analyze/danalyze.cc",
    # add other filenames here if you need to
}

# collect your C sources but skip the excluded ones
c_sources = (
        glob("gnubg-nn/gnubg/*.c") +
        glob("gnubg-nn/gnubg/lib/*.c") +
        glob("gnubg-nn/analyze/*.cc")
)
# filter-out excluded paths
c_sources = [f for f in c_sources if f not in ANALYZE_EXCLUDE]

# your C++ sources as before
cpp_sources = [
    "py3mod.cpp",
    "gnubg-nn/gnubg/bearoffgammon.cc",
    "gnubg-nn/gnubg/racebg.cc",
    "gnubg-nn/gnubg/osr.cc",
]

gnubg_module = Extension(
    "gnubg.gnubg",
    sources=cpp_sources + c_sources,
    include_dirs=[
        "gnubg-nn/", "gnubg-nn/gnubg", "gnubg-nn/gnubg/lib",
        "gnubg-nn/analyze", "gnubg-nn/py"
    ],
    define_macros=[
        ("LOADED_BO", "1"),
        ("OS_BEAROFF_DB", "1"),
    ],
    extra_compile_args=["-std=c++11"],
)

setup(
    name="gnubg",
    version="1.1",
    packages=find_packages(),
    ext_modules=[gnubg_module],
    include_package_data=True,
    package_data={
        'gnubg': ['data/*.bd', 'data/*.weights', 'data/*.db'],
    },
    description='Python3 bindings for GNUBG neural evaluation',
    author='David Reay',
    author_email='dr323090@falmouth.ac.uk',
)
