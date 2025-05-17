import io
import os
import sysconfig
from glob import glob
from setuptools import find_packages, setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext

# 1) initialize
define_macros = [
    ('_CRT_SECURE_NO_WARNINGS', '1'),
    ('NOMINMAX', '1'),
    ("LOADED_BO", "1"),
    ("OS_BEAROFF_DB", "1"),
    ("HAVE_CONFIG_H", "1"),
]

# ----------------------------------------------------------
# read the long description from README.md
# ----------------------------------------------------------
here = os.path.abspath(os.path.dirname(__file__))
with io.open(os.path.join(here, "README.md"), encoding="utf-8") as f:
    long_description = f.read()


class build_ext(_build_ext):
    def build_extensions(self):
        for ext in self.extensions:
            cxx_args = []
            if any(src.endswith(('.cc', '.cpp')) for src in ext.sources):
                cxx_args.append('-std=c++11')
            ext.extra_compile_args = cxx_args
        super().build_extensions()


EXCLUDES = {
    # Add specific files to exclude from build here if needed
}

c_sources = (
        glob("gnubg-nn/gnubg/*.c") +
        glob("gnubg-nn/gnubg/lib/*.c") +
        glob("gnubg-nn/analyze/*.cc")
)
c_sources = [f for f in c_sources if f not in EXCLUDES]

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
    define_macros=define_macros,
    language="c++"
)

setup(
    name="gnubg",
    version="1.1",
    packages=find_packages(where="src", include=["gnubg", "gnubg.data", "gnubg.tests"]),
    package_dir={"": "src"},
    ext_modules=[gnubg_module],
    cmdclass={"build_ext": build_ext},
    include_package_data=True,
    package_data={
        'gnubg': ['data/*.bd', 'data/*.weights', 'data/*.db', 'tests/*.py'],
    },
    exclude_package_data={"gnubg": ["py3mod.cpp"]},
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
