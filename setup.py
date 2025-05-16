from glob import glob
from setuptools import find_packages
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext

class build_ext(_build_ext):
    def build_extension(self, ext):
        # Remove any blanket -std=c++11 for C files
        if ext.language == "c":
            ext.extra_compile_args = []
        super().build_extension(ext)

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
    "src/gnubg/py3mod.cpp",
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
    packages=find_packages(where="src"),
    ext_modules=[gnubg_module],
    include_package_data=True,
    package_dir={"": "src"},
    package_data={
        'gnubg': ['data/*.bd', 'data/*.weights', 'data/*.db'],
    },
    exclude_package_data={"gnubg": ["py3mod.cpp"]},
    description='Python3 bindings for GNUBG neural evaluation',
    author='David Reay',
    author_email='dr323090@falmouth.ac.uk',
    project_urls = {
        'GnuBG official website': 'http://www.gnubg.org/xml-rss2.php?catid=10',
        'GitHub': 'https://github.com/reayd-falmouth/gnubg-nn-pypi',
        'Official GnuBG Source': 'https://savannah.gnu.org/git/?group=gnubg',
    }
)
