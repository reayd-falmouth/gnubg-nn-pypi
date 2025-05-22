# gnubg/__init__.py

# pull in all of your C++ module’s symbols
from ._gnubg import *

# optional: define __all__ if you want to tidy up the namespace
__all__ = [name for name in globals().keys() if not name.startswith("_")]
