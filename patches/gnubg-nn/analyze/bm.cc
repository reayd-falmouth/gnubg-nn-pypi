/*
 * bm.cc
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

#include "minmax.h"

#include <algorithm>

#include "bgdefs.h"

#include "equities.h"
#include "bms.h"
#include "bm.h"

using std::min;
using std::sort;

#undef DEBUG
#define DEBUG
#if defined( DEBUG )
#include <iostream>
using std::cerr;
using std::endl;

long bmDebug = 0;

static void
printMove(move const& m)
{
  for(int k = 0; k < m.cMoves; ++k) {
    cerr << m.anMove[2*k] << "-" << m.anMove[2*k+1] << " ";
  }

  int b[2][25];
  PositionFromKey(b, const_cast<unsigned char*>(m.auch));
  SwapSides(b);
  
  cerr << endl;
  cerr << b[1][24] << " ";
    
  for(uint i = 0; i < 24; ++i) {
    if( b[1][23-i] ) {
      cerr << b[1][23-i] << " ";
    } else {
      cerr << -b[0][i] << " ";
    }
  }
  cerr << -b[0][24];
  
//    for(uint k = 0; k < 2; ++k) {
//      cerr << endl;
//        cerr << b[k][i] << " ";
//      }
//    }
    
  cerr << endl << "val = " << m.rScore << endl;
}

static void
printMoves(movelist const& ml)
{
  for(int k = 0; k < ml.cMoves; ++k) {
    cerr << "move " << k << ":" << endl;
    printMove(ml.amMoves[k]);
  }
}

#endif

static void
sanity(float* const p)
{
  if( p[WIN] > 1.0 ) {
    p[WIN] = 1.0;
  }
  
  if( p[WIN] < 0.0 ) {
    p[WIN] = 0.0;
  }

  if( p[WINGAMMON] > p[WIN] ) {
    p[WINGAMMON] = p[WIN];
  }

  float lose = 1.0 - p[WIN];

  if( p[LOSEGAMMON] > lose ) {
    p[LOSEGAMMON] = lose;
  }
}

///
struct Data {
  /// Match equity of @arg{p}.
  //
  float e;

  /// Position probablities.
  //
  float p[5];

  /// 21 1ply probablities.
  //
  float prr[5 * 21];

  /// 21 1ply positions.
  //  As gnu auch's.
  //
  unsigned char a[21 * 10];
};


/// Sort 21 rolls by equity
class SortX {
public:
  struct D {
    /// roll ordinal number
    int 	i;

    /// roll equity
    float 	er;
  };
  
  int operator ()(D const& i1, D const& i2) const {
    return i1.er > i2.er;
  }
};


// 1 if roll is a double, 2 if not.
//
static uint const
w[21] = {1, 2, 1, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1};

static void
adjust(Data& d, bool const direction /*, EvalLevel const level*/)
{
  SortX::D s[21];

  {
    const float* prr = d.prr;

    for(uint k = 0; k < 21; ++k) {
      s[k].i = k;
      s[k].er = Equities::normalizedEquity(prr, direction);
      prr += 5;
    }
  }
  
  sort(s, s + 21, SortX());

  uint const ad33[7] = {2, 5, 8, 10, 12, 15, 18};
//   uint const adall[21] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
//   14, 15, 16, 17, 18, 19, 20};
  
  const uint* ad = 0;
  uint nad = 0;

//   switch( level ) {
//     case PALL:
//     {
//       ad = adall;
//       nad = sizeof(adall)/sizeof(adall[0]);
//       break;
//     }

//     case P33:
//     {
      ad = ad33;
      nad = sizeof(ad33)/sizeof(ad33[0]);
//       break;
//     }

//     case PNONE:
//     {
//       assert(0);
//       break;
//     }
//   }

  uint ntote = 0;
  float tote[5];
  for(uint i = 0; i < 5; ++i) {
    tote[i] = 0.0;
  }
  
  float p[5];
  int anBoard[2][25];
  
  for(uint k = 0; k < nad; ++k) {
    uint const ti = s[ad[k]].i;

    unsigned char* const auch = d.a + ti * 10;

    PositionFromKey(anBoard, auch);

    EvaluatePosition(anBoard, p, 1, 0, direction, 0, 0, 0);
    
    const float* const pti = d.prr + 5 * ti;
    uint const we = w[ti];
    
    for(uint i = 0; i < 5; ++i) {
      tote[i] += we * (p[i] - pti[i]);
    }
    ntote += we;
  }

  for(uint i = 0; i < 5; ++i) {
    d.p[i] += tote[i] / ntote;
  }

  sanity(d.p);
  
  //d.e = Equities::normalizedEquity(d.p, direction);
  d.e = Equities::mwc(d.p, direction);
}

#if 0

static float const
SEARCH_TOLERANCE = 0.30;

static uint const
MAX_SEARCH_CANDIDATES = 30;

static move
amCandidates[MAX_SEARCH_CANDIDATES];

uint
SEARCH_CANDIDATES = 12;

int
findBestMove_1_33(int        anMove[8],
		  int const  nDice0,
		  int const  nDice1,
		  int        anBoard[2][25],
		  bool const direction,
		  EvalLevel const level)
{
  
  movelist ml;
  
  GenerateMoves(&ml, anBoard, nDice0, nDice1);
    
#if defined( DEBUG )
      if( bmDebug & 0x1 ) {
	cerr << "moves " << endl;
	printMoves(ml);
      }
#endif
      
  if( !ml.cMoves ) {
    return 0;
  }

  if( ml.cMoves == 1 ) {
    ml.iMoveBest = 0;
  } else {
    scoreMoves(ml, 0, direction);

    uint j = 0;

    {
      float const th = ml.rBestScore - SEARCH_TOLERANCE;
    
      for(uint i = 0; i < uint(ml.cMoves); i++) {
	if( ml.amMoves[i].rScore >= th ) {
	  if( i != j ) {
	    ml.amMoves[j] = ml.amMoves[i];
	  }
	  ++j;
	}
      }
    }
    
    if( j == 1 ) {
      // Do not rely on 0ply. open up the threshold to get more moves for
      // 1ply considerations
      
      float const th = ml.rBestScore - 2 * SEARCH_TOLERANCE;
      
      for(uint i = 1; i < uint(ml.cMoves); i++) {
	if( int(i) != ml.iMoveBest && ml.amMoves[i].rScore >= th ) {
	  if( i != j ) {
	    ml.amMoves[j] = ml.amMoves[i];
	  }
	  ++j;
	}
      }
    }
      
    if( j > 1 ) {
      sort(ml.amMoves, ml.amMoves + j, SortMoves());

#if defined( DEBUG )
      if( bmDebug & 0x1 ) {
	cerr << "after first pass" << endl;
	printMoves(ml);
      }
#endif
      
      ml.iMoveBest = 0;
	    
      ml.cMoves = (j < SEARCH_CANDIDATES ? j : SEARCH_CANDIDATES);
      
      {                                 assert( ml.amMoves != amCandidates ); }

#if defined( DEBUG )
      if( bmDebug & 0x1 ) {
	cerr << "keep first " << ml.cMoves << endl;
      }
#endif
      
      memcpy(amCandidates, ml.amMoves, ml.cMoves * sizeof(move));
      ml.amMoves = amCandidates;

      {
	Data eval;
      
	uint const nCan = min(4U, uint(ml.cMoves));
	
	Data candidates[nCan];
	int  iCandidates[nCan];
	
	for(uint k = 0; k < nCan; ++k) {
	  iCandidates[k] = -1;
	}
	
	float* const arEval = eval.p;
	
	for(int i = 0; i < ml.cMoves; ++i) {
	  PositionFromKey(anBoard, ml.amMoves[i].auch);

	  SwapSides(anBoard);

	  EvaluatePosition(anBoard, arEval, 1, 0,
			   !direction, eval.prr, 1, eval.a);
	
	  InvertEvaluation(arEval);

	  float const score = Equities::mwc(arEval, direction);

	  eval.e = score;
	  
	  ml.amMoves[i].rScore = score;

	  for(uint k = 0; k < nCan; ++k) {
	    if( iCandidates[k] == -1 ) {
	      candidates[k] = eval;
	      iCandidates[k] = i;
	      break;
	    }

	    if( score > candidates[k].e ) {
	      for(int j = int(nCan)-2; j >= int(k); --j) {
		// copy only if anything in j
		
		if( iCandidates[j] != -1 ) {
		  candidates[j+1] = candidates[j];
		  iCandidates[j+1] = iCandidates[j];
		}
	      }
	      candidates[k] = eval;
	      iCandidates[k] = i;
	      break;
	    }
	  }
	}

#if defined( DEBUG )
	if( bmDebug & 0x1 ) {
	  cerr << "top " << nCan << " are:" << endl;
	  for(uint k = 0; k < nCan; ++k) {
	    printMove(ml.amMoves[iCandidates[k]]);
	  }
	}
#endif

	if( level != PNONE ) {
	  for(uint k = 0; k < nCan; ++k) {
	    adjust(candidates[k], direction, level);
	  }
	}

  	ml.rBestScore = -99999.9;
  	ml.iMoveBest = -1;

	for(uint k = 0; k < nCan; ++k) {
	  if( candidates[k].e > ml.rBestScore ) {
	    ml.iMoveBest = iCandidates[k];
	    ml.rBestScore = candidates[k].e;
	  }
	}

#if defined( DEBUG )
	if( bmDebug & 0x1 ) {
	  cerr << "best is "  << ml.iMoveBest << endl;
	  printMove(ml.amMoves[ml.iMoveBest]);
	}
#endif
      }
    }
  }

  uint const nm = 2 * ml.amMoves[ml.iMoveBest].cMoves;
  // ml.cMaxMoves
  if( anMove ) {
    memcpy(anMove, ml.amMoves[ml.iMoveBest].anMove, nm * sizeof(anMove[0]));

    for(uint i = nm; i < 8; ++i) {
      anMove[i] = -1;
    }
  }
	
  PositionFromKey(anBoard, ml.amMoves[ml.iMoveBest].auch);

  return nm;
}
#endif


static void
scoreMovesPly2r33(movelist&   ml,
		  bool const  xOnPlay)
{
  Data eval;
  int        anBoard[2][25];
  
  float* const arEval = eval.p;
  ml.rBestScore = -99999.9;
	
  for(int i = 0; i < ml.cMoves; ++i) {
    PositionFromKey(anBoard, ml.amMoves[i].auch);

    SwapSides(anBoard);

    EvaluatePosition(anBoard, arEval, 1, 0,
		     !xOnPlay, eval.prr, 1, eval.a);
	
    InvertEvaluation(arEval);

    adjust(eval, xOnPlay /*, P33*/);

    ml.amMoves[i].rScore = eval.e;

    if( eval.e >= ml.rBestScore ) {
      ml.iMoveBest = i;
      ml.rBestScore = eval.e;
    }
  }
}

struct plySearch {
  uint	nMoves;
  uint	nAdditional;
  float th;
};

static plySearch
p[4] = {
  {0,0,0.0} ,
  {8, 0, 0.00},
  {2, 3, 0.1},
  {2, 3, 0.1},
};

static uint const
sizep = sizeof(p)/sizeof(p[0]);

void
setPlyBounds(uint np, uint nm, uint na, float th)
{
  if( np > 0 && np < sizep ) {
      p[np] = plySearch{nm, na, th};
  }
}

static void
preset(movelist& ml)
{
  for(uint k = 0; k < uint(ml.cMoves); ++k) {
    ml.amMoves[k].pEval = new float [5];
  }
}

static void
set(std::vector<MoveRecord>& mRecord, movelist& ml)
{
  int tmp[2][25];
  mRecord.resize(ml.cMoves);
	
  for(uint k = 0; k < uint(ml.cMoves); ++k) {
    MoveRecord& r = mRecord[k];
    move& mk = ml.amMoves[k];
    
    PositionFromKey(tmp, mk.auch);
    SwapSides(tmp);
    PositionKey(tmp, r.pos);

    memcpy(r.probs, mk.pEval, 5 * sizeof(r.probs[0]));
    InvertEvaluation(r.probs);

    delete [] mk.pEval;
    
    r.matchScore = mk.rScore;
    memcpy(r.move, mk.anMove, sizeof(r.move));
  }
}

int
findBestMove(int        anMove[8],
	     int const  nDice0,
	     int const  nDice1,
	     int        anBoard[2][25],
	     bool const direction,
	     uint const nPlies,
	     std::vector<MoveRecord>*	mRecord,
	     bool       reduced)
{
  movelist ml;
  
  GenerateMoves(&ml, anBoard, nDice0, nDice1);
    
  if( !ml.cMoves ) {
    return 0;
  }

  // If mRecord at ply 0 need probs regardless of number of moves
  if( mRecord && nPlies == 0 ) {
    preset(ml);
  }

  if( ml.cMoves == 1 ) {
    ml.iMoveBest = 0;
  } else {
      
    scoreMoves(ml, 0, direction);

    // why sort if 0 ply?
    sort(ml.amMoves, ml.amMoves + ml.cMoves, SortMoves());
    ml.iMoveBest = 0;
    
#if defined( DEBUG )
      if( bmDebug & 0x1 ) {
	cerr << "moves after 0ply" << endl;
	printMoves(ml);
      }
#endif
    
    if( nPlies > 0 ) {
      plySearch const& s = p[nPlies < sizep ? nPlies : sizep - 1];
      
      uint const k = ml.cMoves;
      ml.cMoves = min(s.nMoves, k);

      int const limit = min(k, ml.cMoves + s.nAdditional);
      
      for( /**/; ml.cMoves < limit; ++ml.cMoves) {
	if( ml.amMoves[ml.cMoves].rScore < ml.amMoves[0].rScore - s.th ) {
	  break;
	}
      }

      static move* lamCandidates = 0;
      static uint nAMcandidates = 0;
  
      if( nAMcandidates < uint(ml.cMoves) ) {
	lamCandidates = static_cast<move*>
	  (realloc(lamCandidates, ml.cMoves * sizeof(*lamCandidates)));
    
	nAMcandidates = ml.cMoves;
      }
      
      memcpy(lamCandidates, ml.amMoves, ml.cMoves * sizeof(move));
      ml.amMoves = lamCandidates;

      if( mRecord ) {
	preset(ml);
      }

      if( reduced && nPlies == 2 ) {
	scoreMovesPly2r33(ml, direction);
      } else {
	scoreMoves(ml, nPlies, direction);
      }

#if defined( DEBUG )
      if( bmDebug & 0x1 ) {
	cerr << "moves after " << nPlies << "ply, best is "
	     << ml.iMoveBest << endl;
	printMoves(ml);
      }
#endif
      
    }
  }

  if( mRecord ) {
    if( nPlies > 0 ) {
      sort(ml.amMoves, ml.amMoves + ml.cMoves, SortMoves());
      ml.iMoveBest = 0;
    }
      
    set(*mRecord, ml);
  }

  uint const nm = 2 * ml.amMoves[ml.iMoveBest].cMoves;

  if( anMove ) {
    memcpy(anMove, ml.amMoves[ml.iMoveBest].anMove, nm * sizeof(anMove[0]));

    for(uint i = nm; i < 8; ++i) {
      anMove[i] = -1;
    }
  }
	
  PositionFromKey(anBoard, ml.amMoves[ml.iMoveBest].auch);

  return nm;
}
