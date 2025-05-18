/*
 * bms.cc
 *
 * by Joseph Heled <joseph@gnubg.org>, 2000
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <assert.h>
#include <stdlib.h>

#include <algorithm>

#include "equities.h"
#include "bgdefs.h"

#include "bms.h"

void
scoreMoves(movelist&   ml,
	   int const   nPlies,
	   bool const  xOnPlay)
{
  int temp[2][25];
  float ar[NUM_OUTPUTS];

  ml.rBestScore = -99999.9;
  
  for(int i = 0; i < ml.cMoves; ++i) {
    PositionFromKey(temp, ml.amMoves[i].auch);

    SwapSides(temp);
	
    EvaluatePosition(temp, ar, nPlies, 0, !xOnPlay, 0, 0, 0);
	
    InvertEvaluation(ar);
	
    if( ml.amMoves[i].pEval ) {
      memcpy(ml.amMoves[i].pEval, ar, sizeof(ar));
    }

    float& r = ml.amMoves[i].rScore;

    r = Equities::mwc(ar, xOnPlay);

    if( r >= ml.rBestScore ) {
      ml.iMoveBest = i;
      ml.rBestScore = r;
    }
  }
}



// Find best moves
// Return results in pml.
// Do an nPlies level evaluation.
// nDice0, nDice1, anBoard are dice and board.
// moveAuch (when not 0), is the actual move made. Always include it in moves
// list.
//
// Report up to maxMoves moves.
//
// Ignore moves whose equity difference from best move is greater than maxDiff.
//
void
findBestMoves(movelist&            pml,
	      int const            nPlies,
	      int const            nDice0,
	      int const            nDice1,
	      int                  anBoard[2][25],
	      unsigned char* const moveAuch,
	      bool const           xOnPlay,
	      uint const           maxMoves,
	      float const          maxDiff)
{
  {                                                   assert( maxMoves > 0 ); }

  GenerateMoves(&pml, anBoard, nDice0, nDice1);
  {                                                assert( pml.cMoves >= 0 ); }

  if( pml.cMoves == 0 ) {
    return;
  }

  uint nMoves = pml.cMoves;
  
  // resize results array, if necessary.
  
  static float** ar = 0;
  static unsigned int nAr = 0;

  if( nAr < nMoves ) {
    float** nar = new float* [nMoves];

    for(uint i = 0; i < nAr; ++i) {
      nar[i] = ar[i];
    }
    
    for(uint i = nAr; i < nMoves; ++i) {
      nar[i] = new float [NUM_OUTPUTS];
    }
    delete [] ar;
    ar = nar;
  }

  if( nPlies == 0 ) {
    for(uint i = 0; i < nMoves; ++i) {
      pml.amMoves[i].pEval = ar[i];
    }
  }
    
  // Do an initial prunning with ply 0
  
  scoreMoves(pml, 0, xOnPlay);


  // Number of moves that pass requirements
  //
  uint nMovesAboveTh = 0;

  // Insure that threshold is wide enough for first pass 0 ply evaluation
  //
  float const initialD = nPlies > 0 ? std::max(maxDiff, float(0.25)) : maxDiff;

  // Remove all moves with equity less than threshold of best move
  
  for(uint i = 0; i < nMoves; ++i) {
    bool const mustHave =
      (moveAuch && EqualKeys(moveAuch, pml.amMoves[i].auch));
		       
    if( mustHave || (pml.amMoves[i].rScore >= pml.rBestScore - initialD) ) {
      if( i != nMovesAboveTh ) {
	pml.amMoves[nMovesAboveTh] = pml.amMoves[i];
      }

      // ply 0 already done, for plies > 1 no need for prob yet
      
      if( nPlies == 1 ) {    
	pml.amMoves[nMovesAboveTh].pEval = ar[nMovesAboveTh];
      }
      
      ++nMovesAboveTh;
    }
  }

  pml.cMoves = nMoves = nMovesAboveTh;

  if( nPlies == 0 ) {
    std::sort(pml.amMoves, pml.amMoves + nMoves, SortMoves());
  } else {

    static move* amCandidates = 0;
    static uint nAMcandidates = 0;
  
    if( nAMcandidates < nMoves ) {
      amCandidates = static_cast<move*>
	(realloc(amCandidates, nMoves * sizeof(*amCandidates)));
    
      nAMcandidates = nMoves;
    }

    // save moves in a safe place. ScoreMoves destroys them.
  
    memcpy(amCandidates, pml.amMoves, nMoves * sizeof(move));    
    pml.amMoves = amCandidates;

    // If high nPlies, eliminate more moves using a ply 1 evaluation.
    // This is a speedup, performed since high plies are very expensive.
    //
    // We assume that 1 ply evaluations are good enough and will not
    // eliminate moves that might prove much better at higher plies.
  
    if( nPlies > 1 ) {
      // score first pass moves with ply 1
    
      scoreMoves(pml, 1, xOnPlay);

      std::sort(pml.amMoves, pml.amMoves + nMoves, SortMoves());

      // Base threshold, as given by caller
      //
      float th = pml.rBestScore - maxDiff;
    
      // We require only maxMoves, so (assuming 1ply is good enough) eliminate
      // moves whose equity differs by more than 150% than difference between
      // best move and the maxMoves's move.

      if( nMoves > maxMoves ) {
	float const lastScore = pml.amMoves[maxMoves-1].rScore;
      
	float const t = 1.5 * lastScore - 0.5 * pml.rBestScore;
      
	th = std::max(th, t);
      }
    
      nMovesAboveTh = 0;
    
      for(uint i = 0; i < nMoves; ++i) {
	bool const mustHave =
	  (moveAuch && EqualKeys(moveAuch, pml.amMoves[i].auch));
		       
	if( mustHave || (pml.amMoves[i].rScore >= th) ) {
	  if( i != nMovesAboveTh ) {
	    pml.amMoves[nMovesAboveTh] = pml.amMoves[i];
	  }

	  // Now set for final evaluation
	
	  pml.amMoves[nMovesAboveTh].pEval = ar[nMovesAboveTh];
      
	  ++nMovesAboveTh;
	}
      }

      pml.cMoves = nMoves = nMovesAboveTh;
    } else {
      // ply 1, sort ply 0 moves
    
      std::sort(pml.amMoves, pml.amMoves + nMoves, SortMoves());
    }

    pml.iMoveBest = 0;

    if( nPlies ) {
      scoreMoves(pml, nPlies, xOnPlay);
	
      std::sort(pml.amMoves, pml.amMoves + nMoves, SortMoves());
    }

  }
  
  // final pass, keep only up to maxMoves, each with equity difference
  // less than maxDiff from best move
  
  pml.iMoveBest = 0;
  nMovesAboveTh = 0;
    
  for(uint i = 0; i < nMoves; ++i) {
    bool const mustHave =
      (moveAuch && EqualKeys(moveAuch, pml.amMoves[i].auch));
		       
    if( mustHave || (pml.amMoves[i].rScore >= pml.rBestScore - maxDiff) ) {
      if( nMovesAboveTh < maxMoves ) {
	pml.amMoves[nMovesAboveTh] = pml.amMoves[i];
	++nMovesAboveTh;
	
      } else if( mustHave ) {
	// list is full, and actual move is not among them. Throw move
	// with lowest ranking.
	
	pml.amMoves[maxMoves-1] = pml.amMoves[i];
	break;
      }
    }
  }

  pml.cMoves = nMovesAboveTh;
}
