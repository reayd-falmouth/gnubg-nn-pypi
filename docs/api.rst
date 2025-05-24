
API Reference
=============

This section documents the public Python API provided by the `gnubg-nn-pypi` package. It includes functions for evaluating positions, loading neural nets, and accessing internal neural net state.

gnubg.load_network
------------------

.. function:: load_network(index: int) -> None

   Loads the neural network with the specified index (0–5).

   :param index: Index of the network to load:
                 - 0: Race
                 - 1: Contact
                 - 2: Crashed
                 - 3: Prune Race
                 - 4: Prune Contact
                 - 5: Prune Crashed

   This must be called before evaluating a position with that network.

gnubg.evaluate(inputs: List[float]) -> Tuple[float, float, float]
------------------------------------------------------------------

.. function:: evaluate(inputs)

   Evaluates a position given its 250-element input vector.

   :param inputs: A list of 250 normalized floats describing the position.
   :returns: A tuple of (win, win_gammon, win_backgammon)

   Each value is a probability in the range 0.0–1.0.

gnubg.classify_position(position) -> int
----------------------------------------

.. function:: classify_position(position)

   Classifies a position as Race, Contact, or Crashed.

   :param position: A structured representation of the board state.
   :returns: An integer (0=Race, 1=Contact, 2=Crashed)

gnubg.get_inputs(position) -> List[float]
-----------------------------------------

.. function:: get_inputs(position)

   Converts a position to a 250-feature input vector.

   :param position: Position object or ID
   :returns: A list of 250 floats for neural network evaluation

gnubg.position_id_to_board(pid: str) -> Board
---------------------------------------------

.. function:: position_id_to_board(pid)

   Converts a GNU Backgammon Position ID string into a board object.

   :param pid: A 14-character Position ID string
   :returns: A board structure usable by other gnubg-nn functions

gnubg.evaluate_position_id(pid: str, network: int) -> Tuple[float, float, float]
--------------------------------------------------------------------------------

.. function:: evaluate_position_id(pid, network)

   Evaluates a position given its Position ID and selected network.

   :param pid: Position ID string
   :param network: Index of neural network to use (see `load_network`)
   :returns: (win, gammon, backgammon) probabilities

