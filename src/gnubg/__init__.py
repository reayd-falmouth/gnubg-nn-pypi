# gnubg/__init__.py
import os
from pathlib import Path

if os.name == "nt" and hasattr(os, "add_dll_directory"):
    pkgdir = Path(__file__).parent
    libdir = pkgdir / ".gnubg.mesonpy.libs"
    if libdir.is_dir():  # only add if it really exists
        os.add_dll_directory(str(libdir))

# pull in all of your C++ module’s symbols
from .gnubg import *

# optional: define __all__ if you want to tidy up the namespace
__all__ = [name for name in globals().keys() if not name.startswith("_")]
