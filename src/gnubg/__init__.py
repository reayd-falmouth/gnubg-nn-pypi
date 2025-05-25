import os
from pathlib import Path

if os.name == "nt" and hasattr(os, "add_dll_directory"):
    pkgdir = Path(__file__).parent
    if pkgdir.is_dir():
        os.add_dll_directory(str(pkgdir))

# Import your compiled extension module
from .gnubg import *

# Import version info
from .__version__ import version, __version__, full_version, git_revision, release, short_version

# Optional: Expose a namespace
__all__ = [name for name in globals().keys() if not name.startswith("_")]
