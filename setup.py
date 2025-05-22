import io
import os
import sys
from glob import glob
from setuptools import find_packages, setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext

is_windows = sys.platform == "win32"
is_macos = sys.platform == "darwin"

define_macros = [
    ('_CRT_SECURE_NO_WARNINGS', '1'),
    ('NOMINMAX', '1'),
    ("LOADED_BO", "1"),
    ("OS_BEAROFF_DB", "1"),
    ("HAVE_CONFIG_H", "1"),
]

# Platform-specific compile args
extra_compile_args_c = ["/wd4244", "/wd4305", "/wd4028", "/wd4090"] if is_windows else []
if is_windows:
    extra_compile_args_cpp = ["/std:c++14"]
elif is_macos:
    extra_compile_args_cpp = ["-std=c++14"]
else:
    extra_compile_args_cpp = ["-std=c++14"]

here = os.path.abspath(os.path.dirname(__file__))
with io.open(os.path.join(here, "README.md"), encoding="utf-8") as f:
    long_description = f.read()

class build_ext(_build_ext):
    def build_extensions(self):
        ctype = self.compiler.compiler_type

        for ext in self.extensions:
            args = []
            # Detect if the extension has any C++ files
            has_cpp = any(src.endswith(('.cpp', '.cc', '.cxx')) for src in ext.sources)

            # Use the predefined args
            if is_windows:
                # always suppress those warnings on Windows
                args.extend(extra_compile_args_c)
                if has_cpp:
                    # force C++14 mode and "treat as C++"
                    args[:0] = ["/std:c++14", "/TP"]
            # if ctype == "msvc":
            #     args = (
            #             (["/std:c++14"] if has_cpp else []) +
            #             ["/wd4244", "/wd4305", "/wd4028", "/wd4090"]
            #     )
            else:
                args = (
                        (["-std=c++14"] if has_cpp else []) +
                        []
                )

            if is_macos:
                # Remove incompatible flags
                args = [arg for arg in args if not arg.startswith("-std=")]

            ext.extra_compile_args = args
        super().build_extensions()

# Source files
c_sources = [
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
]
print(f"c_sources: {c_sources}")

cpp_sources = [
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
]
print(f"cpp_sources: {cpp_sources}")

common_includes = [
    "gnubg-nn/",
    "gnubg-nn/gnubg",
    "gnubg-nn/gnubg/lib",
    "gnubg-nn/analyze",
    "gnubg-nn/py",
]
print(f"common_includes: {common_includes}")

extensions = [
    Extension(
        "gnubg.gnubg",
        sources=c_sources + cpp_sources,
        include_dirs=common_includes,
        define_macros=define_macros,
        language="c++"
    )
]

setup(
    name="gnubg",
    version="1.1.0.dev4",
    packages=find_packages(where="src", include=["gnubg", "gnubg.data", "gnubg.tests"]),
    package_dir={"": "src"},
    ext_modules=extensions,
    cmdclass={"build_ext": build_ext},
    include_package_data=True,
    package_data={
        'gnubg': ['data/*.bd', 'data/*.weights', 'data/*.db'],
    },
    exclude_package_data={"gnubg": ["gnubgmodule.cpp"]},
    description="Python3 bindings for GNUBG neural evaluation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author='David Reay',
    install_requires=[],
    author_email='dr323090@falmouth.ac.uk',
    project_urls={
        'Homepage':       'https://www.gnu.org/software/gnubg/',
        'Documentation':  'http://www.gnubg.org/documentation/doku.php?id=gnu_backgammon_faq',
        'Mailing List':   'https://lists.gnu.org/mailman/listinfo/gnubg',
        'Source':         'https://github.com/gnubg/gnubg-nn-pypi',
        'GNUBG Project':  'https://git.savannah.gnu.org/cgit/gnubg/gnubg-nn.git',
        'Contributing':   'https://savannah.gnu.org/people/',
        'Credits':          'https://git.savannah.gnu.org/cgit/gnubg.git/tree/credits.sh',
    },
    classifiers=[
        'Programming Language :: Python :: 3',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.6',
)
