Overview
========

**`gnubg-nn-pypi`** is a Python package that provides bindings to the neural network evaluation engine from GNU Backgammon (GNUBG). It allows developers and researchers to evaluate backgammon positions using the same logic and weights that power one of the strongest backgammon AIs in existence.

This package is ideal for:

- AI/ML researchers working with game engines
- Developers building backgammon apps or bots
- Hobbyists analyzing positions and match play
- Students exploring neural networks in classical games

Features
--------

- Evaluate positions using GNUBG-trained neural networks
- Load and use official GNUBG weights
- Generate GNUBG-style 250-feature input vectors from Position IDs
- Access low-level evaluation scores: win/gammon/backgammon probabilities
- Python 3 bindings to native C++ GNUBG library

Why GNUBG?
----------

GNU Backgammon is a free and open-source world-class backgammon engine. Its evaluation pipeline combines neural nets, equity lookups, and rollout simulations. With `gnubg-nn-pypi`, you get the core NN evaluation logic in Python, making it easy to integrate into modern workflows without running the full GUI or CLI.

About Backgammon
-----------------

Backgammon is a two-player strategy game combining tactics, probability, and positional play. Each player moves 15 checkers across a 24-point board based on dice rolls. The goal is to bear off all checkers before your opponent. Advanced play involves doubling stakes, calculating equity, and leveraging game-theoretic principles — making it an ideal testbed for AI.

🛈 See :doc:`rules` for a summary of backgammon rules, or the `GNU Backgammon Manual <https://www.gnu.org/software/gnubg/manual/>`_ for full detail.
