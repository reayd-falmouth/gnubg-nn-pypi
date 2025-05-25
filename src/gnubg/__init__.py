import os
from pathlib import Path

if os.name == "nt" and hasattr(os, "add_dll_directory"):
    pkgdir = Path(__file__).parent
    if pkgdir.is_dir():
        os.add_dll_directory(str(pkgdir))

# Import your compiled extension module
from .gnubg import *

# Optional: Expose a namespace
__all__ = [name for name in globals().keys() if not name.startswith("_")]
