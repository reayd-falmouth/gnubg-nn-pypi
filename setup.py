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
        for ext in self.extensions:
            args = []
            for src in ext.sources:
                if src.endswith((".cc", ".cpp")):
                    args += extra_compile_args_cpp
                elif src.endswith(".c"):
                    args += extra_compile_args_c
            if is_macos:
                # Remove incompatible flags
                args = [arg for arg in args if not arg.startswith("-std=")]
            ext.extra_compile_args = args
        super().build_extensions()

# Source files
c_sources = (
        glob("gnubg-nn/gnubg/*.c") +
        glob("gnubg-nn/gnubg/lib/*.c")
)
cpp_sources = (
        glob("gnubg-nn/analyze/*.cc") +
        [
            "src/gnubg/gnubgmodule.cpp",
            "gnubg-nn/gnubg/bearoffgammon.cc",
            "gnubg-nn/gnubg/racebg.cc",
            "gnubg-nn/gnubg/osr.cc",
        ]
)

common_includes = [
    "gnubg-nn/", "gnubg-nn/gnubg", "gnubg-nn/gnubg/lib",
    "gnubg-nn/analyze", "gnubg-nn/py"
]

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
    version="1.1",
    packages=find_packages(where="src", include=["gnubg", "gnubg.data", "gnubg.tests"]),
    package_dir={"": "src"},
    ext_modules=extensions,
    cmdclass={"build_ext": build_ext},
    include_package_data=True,
    package_data={
        'gnubg': ['data/*.bd', 'data/*.weights', 'data/*.db', 'tests/*.py'],
    },
    exclude_package_data={"gnubg": ["gnubgmodule.cpp"]},
    description="Python3 bindings for GNUBG neural evaluation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author='David Reay',
    author_email='dr323090@falmouth.ac.uk',
    project_urls={
        'Homepage':       'https://www.gnu.org/software/gnubg/',
        'Documentation':  'http://www.gnubg.org/documentation/doku.php?id=gnu_backgammon_faq',
        'Mailing List':   'https://lists.gnu.org/mailman/listinfo/gnubg',
        'Source':         'https://github.com/gnubg/gnubg-nn-pypi',
        'Contributing':   'https://github.com/gnubg/gnubg-nn-pypi/blob/main/CONTRIBUTING.md',
        'Bug Tracker':    'https://github.com/gnubg/gnubg-nn-pypi/issues',
    },
    classifiers=[
        'Programming Language :: Python :: 3',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.6',
)
