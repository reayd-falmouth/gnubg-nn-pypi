# gnubg/__init__.py
import os
from pathlib import Path

if os.name == "nt" and hasattr(os, "add_dll_directory"):
    pkgdir = Path(__file__).parent
    # Only add if the directory exists to avoid WinError 2
    if pkgdir.is_dir():  # or use try/except FileNotFoundError
        os.add_dll_directory(str(pkgdir))

# Import your compiled extension module
from .gnubg import *

# Optional: tidy namespace
__all__ = [name for name in globals().keys() if not name.startswith("_")]
