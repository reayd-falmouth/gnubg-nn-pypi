# gnubg-nn-pypi
GNUBG NeuralNet python2.7 updated to python3 module

This package provides full Python bindings to the GNUBG backgammon neural-network engine, embedding its evaluation and move-generation capabilities directly in Python:

Engine initialization and data loading
On import, the extension locates its shipped data directory (containing gnubg.weights, opening-book and bear-off tables) and calls Analyze::init(...) to load all six neural nets. It then enables optional SSE optimizations via useSSE(1) .

Rich Python API (“gnubg” module)
The core gnubg module exports a wide array of functions (see GnubgMethods), including:

Position classification (classify) and public-evaluation best move (pubbestmove)

Board↔ID conversions (boardfromid, boardfromkey, keyofboard, id)

Dice utilities (roll) and cube utilities (bestmove, pubevalscore)

Bear-off tools (bearoffid2pos, bearoffprobs)

Legal-move enumeration (moves) and probabilistic evaluation (probs)

Monte-Carlo rollouts (rollout, crollout)
All of these hooks map straight through to the underlying C++ analyzer for blazing-fast performance .

“equities” and “set” submodules

The equities submodule exposes a single method, equities.value(xAway, oAway), to look up precomputed equity tables.

The set submodule lets you tweak engine parameters at runtime (random seed, OS-database on/off, public-evaluation shortcuts, match score and cube settings, etc.) py3modpy3mod.

Core C API under the hood
Every Python call dispatches to the GNUBG C library (declared in eval.h), which offers routines like EvaluatePositionFast, EvaluatePosition, EvalBearoffOneSided, FindBestMove, GenerateMoves, and Rollout for high-performance position analysis and simulation. Input-feature computation (up to 250 real-valued inputs per position) is handled by inputs.c, ensuring the neural nets see the right board descriptors py3mod.

In short, this library turns the standalone GNUBG neural-network backgammon engine into a first-class Python module, letting you classify positions, evaluate equities, generate or rate moves, simulate games, and control engine parameters—all without leaving Python.