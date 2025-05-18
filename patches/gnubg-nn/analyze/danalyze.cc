/*
 * danalyze.cc
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
#if defined( DEBUG )
#include <iostream>
using std::cout;
using std::endl;

#include <cstdio>
#endif
#include <map>
using std::map;

#include "bgdefs.h"
#include "misc.h"

#include "equities.h"

#include "analyze.h"

#include "danalyze.h"

#include "bm.h"

#include <cstdio>
#include <iostream>
using std::cout;
using std::endl;

static unsigned int const roll2dice1[21] =
{ 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6};

static unsigned int const roll2dice2[21] =
{ 1, 1, 2, 1, 2, 3, 1, 2, 3, 4, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 6};


static inline bool
rollIsDouble(uint const nr)
{
  return roll2dice1[nr] == roll2dice2[nr];
}

// Round 'f' to 4 significant digits
static inline float
roundFloat(float const f)
{
  float const factor = 10000.0;
  
  return long(f * factor + 0.5) / factor;
}

// Round 'f' to 4 significant digits
static inline void
roundFloatVar(float& f)
{
  f = roundFloat(f);
}

static float
estimateNR(const unsigned char* const auch)
{
  Analyze::GNUbgBoard b;
  
  PositionFromKey(b, const_cast<unsigned char*>(auch));

  uint xpip = 0, opip = 0;
  
  for(uint i = 0; i < 25; ++i) {
    if( int const c = b[0][i] ) {
      opip += (i+1) * c;
    }
    if( int const c = b[1][i] ) {
      xpip += (i+1) * c;
    }
  }

  xpip *= 2;
  opip *= 2;  
  for(uint i = 0; i < 5; ++i) {
    if( int const c = b[0][i] ) {
      opip += (5 - i) * c;
    }
    if( int const c = b[1][i] ) {
      xpip += (5 - i) * c;
    }
  }

  int pip = std::min(xpip, opip);

  return (pip / (2.0 * 8.16));
}

float centeredLDweight = 0.65;
float ownedLDweight = 0.7;
bool useRaceMagic = true;

static float
positionFdt(uint const                  xAway,
	    uint const                  oAway,
	    uint const                  cube,
	    float const                 xgr,
	    float const                 ogr,
	    const unsigned char* const  auch)
{
  // For contact positions, use a fixed number for now.
  float l = ownedLDweight;

  if( useRaceMagic && posIsRace(auch) ) {
    Equities::Es c;
    Equities::Es d;

    Equities::get(c, &d, oAway, xAway, 2*cube, ogr, xgr
#if defined( DEBUG )
		  , 0
#endif
      );
    
    float const lp = std::min(c.xLow, d.xLow);
    float const gain = d.yHigh - c.yHigh;
    float const lose = c.e(lp) - d.e(lp);
    if( lose != 0.0 ) {
      float const insentive = gain/lose;
      float const magic = 0.060658192655657979;
      l = magic * estimateNR(auch) * ((2 * insentive)/(insentive + 1));
    } else {
      l = 1.0;
    }
  }

  return l;
}
    

static float
mwcStatic(Equities::Es const&  e,
	  Equities::Es const&  ed,
	  float const          xWins,
	  float const          xgr,
	  float const          ogr,
	  uint const           cube,
	  bool const           xHoldsCube,
	  uint const           xAway,
	  uint const           oAway,
	  float const          /*cubeLife*/,
	  const unsigned char*   auch,
	  Analyze::R1* const	 result
#if defined( DEBUG )
	  ,bool const          debug
#endif   
  )
{
  float e1;

  // we assume cube access
  {                                        assert( cube == 1 || xHoldsCube ); }

  float nd;

  // If outside window, code below will handle
    
  if( cube == 1 ) {
    {
      Equities::Es dead;

      dead.xLow = 0;
      dead.yLow = Equities::eWhenLose(ogr, xAway, oAway, cube);
      dead.xHigh = 1.0;
      dead.yHigh = Equities::eWhenWin(xgr, xAway, oAway, cube);
      
      float ld = 0.0;
      if( xWins >= e.xLow ) {
	Equities::Es tmp1 = dead;
	tmp1.xLow = e.xLow; tmp1.yLow = e.yLow;
      
	ld = tmp1.y(xWins);
      }

      float dl = 0.0;
      if( xWins <= e.xHigh ) {
	Equities::Es tmp1 = dead;
	tmp1.xHigh = e.xHigh; tmp1.yHigh = e.yHigh;

	dl = tmp1.y(xWins);
      }

      float ll;
      if( xWins > e.xHigh ) {
	Equities::Es tmp1 = dead;
	tmp1.xLow = e.xHigh; tmp1.yLow = e.yHigh;
	  
	ll = tmp1.y(xWins);

	dl = ll;
      } else if( xWins < e.xLow ) {
	Equities::Es tmp1 = dead;
	tmp1.xHigh = e.xLow; tmp1.yHigh = e.yLow;
	  
	ll = tmp1.y(xWins);

	ld = ll;
      } else {
	ll = e.y(xWins);
      }

      //nd = (cubeLife * ll + (1-cubeLife)/2 * (dl + ld));
      nd = (centeredLDweight * ll + (1-centeredLDweight)/2 * (dl + ld)); 
    }
  } else {
    Equities::Es tmp = e;
    tmp.xHigh = 1.0;
    tmp.yHigh = Equities::eWhenWin(xgr, xAway, oAway, cube);

    float const ndd = tmp.y(xWins);
	
    float ndl;
    if( xWins <= e.xHigh ) {
      ndl = e.y(xWins);
    } else {
      tmp.xLow = e.xHigh; tmp.yLow = e.yHigh;
	
      ndl = tmp.y(xWins);
    }
      
    float const l = positionFdt(xAway, oAway, cube, xgr, ogr, auch);
      
    nd = l * ndl + (1-l) * ndd;
  }

  Equities::Es tmp = ed;
  tmp.xLow = 0; tmp.yLow = Equities::eWhenLose(ogr, xAway, oAway, 2*cube);
  float const dtd = tmp.y(xWins);
  float dtl;

  if( xWins < ed.xLow ) {
    tmp.xHigh = ed.xLow; tmp.yHigh = ed.yLow;
    dtl = tmp.y(xWins);
  } else {
    dtl = ed.y(xWins);
  }

  roundFloatVar(nd);

  float const l = positionFdt(xAway, oAway, cube, xgr, ogr, auch);
    
  // if automatic redouble, full cube efficiency
  float const dbtk =
    roundFloat((xAway <= 2*cube) ? dtl : (l * dtl + (1-l) * dtd));

  float const dbdr = roundFloat(e.yHigh);

  {
#if defined( DEBUG )
    if( debug ) {
      // FIXME
//       cout.form("N/D %.2lf D/T %.2lf D/D %.2lf ==> ",
// 			    100.0*Equities::equityToProb(nd),
// 			    100.0*Equities::equityToProb(dbtk),
// 			    100.0*Equities::equityToProb(dbdr)); }
      printf("N/D %.2lf D/T %.2lf D/D %.2lf ==> ",
	     100.0*Equities::equityToProb(nd),
	     100.0*Equities::equityToProb(dbtk),
	     100.0*Equities::equityToProb(dbdr)); }
#endif
    if( result ) {
      result->actionDouble = false;
      result->actionTake = true;
      result->tooGood = false;

      result->matchProbNoDouble = Equities::equityToProb(nd);
      result->matchProbDoubleTake = Equities::equityToProb(dbtk);
      result->matchProbDoubleDrop = Equities::equityToProb(dbdr);
    }
	
    if( dbtk < dbdr ) {
      // take
      if( nd < dbtk ) {
	// double/take
#if defined( DEBUG )
	if( debug ) { cout  << "double/take"; }
#endif
	if( result ) {
	  result->actionDouble = true;
	}
	  
	e1 = dbtk;
      } else {
#if defined( DEBUG )
	if( debug ) { cout  << "no double"; }
#endif
	e1 = nd;
      }
    } else {
      // drop
      if( nd > dbdr ) {
	// too good
#if defined( DEBUG )
	if( debug ) { cout  << "too good"; }
#endif
	e1 = nd;

	if( result ) {
	  result->tooGood = true;
	  result->actionTake = false;
	}
	  
      } else {
#if defined( DEBUG )
	if( debug ) { cout  << "double/drop"; }
#endif
	if( result ) {
	  result->actionDouble = true;
	  result->actionTake = false;
	}
	  
	e1 = dbdr;
      }
    }
  }

#if defined( DEBUG )
  if( debug ) { cout  << " e = " << 100*Equities::equityToProb(e1) << endl; }
#endif
  
  return e1;
}


void
Analyze::R1::get(float* const p) const
{
  p[WINBACKGAMMON] = probs[0];
  p[WINGAMMON] = probs[1];
  p[WIN] = probs[2];
  p[LOSEGAMMON] = probs[4];
  p[LOSEBACKGAMMON] = probs[5];
}

void
Analyze::R1::setProbs(const float* const p)
{
  probs[0] = p[WINBACKGAMMON];
  probs[1] = p[WINGAMMON];
  probs[2] = p[WIN];
  probs[3] = 1 - p[WIN];
  probs[4] = p[LOSEGAMMON];
  probs[5] = p[LOSEBACKGAMMON];
}

float
Analyze::R1::match(void) const
{
  float p[NUM_OUTPUTS];

  get(p);

  return Equities::mwc(p, xOnPlay);
}






float
Analyze::R1::cubefulEquity0(GNUbgBoard const  anBoard,
			    bool const        xOnPlay_,
			    uint const        onPlayAway,
			    uint const        opAway,
			    uint const        cube,
			    bool const        onPlayHoldsCube)
{
  float p[NUM_OUTPUTS];
  //EvaluatePosition(anBoard, p, nPliesEval, 0, xOnPlay_, 0, 0, 0);
  if( nPlies == 0 ) {
    get(p);
  } else {
    EvaluatePosition(anBoard, p, 0, 0, xOnPlay_, 0, 0, 0);
  }

  float const xWins = p[WIN];
  float const xWinsGammon = p[WINGAMMON];
  float const oWins = 1 - xWins;
  float const oWinsGammon = p[LOSEGAMMON];

  float const oGammonRatio = (oWins > 0) ? oWinsGammon / oWins : 0.0;
  float const xGammonRatio = (xWins > 0) ? xWinsGammon / xWins : 0.0;
    
  unsigned char a[10];
  PositionKey(anBoard, a);
  
  Equities::Es e;
  Equities::Es ed;

  float ed1;
  
  if( cube == 1 || onPlayHoldsCube ) {
    Equities::get(e, &ed, onPlayAway, opAway, cube,
		  xGammonRatio, oGammonRatio
#if defined( DEBUG )
		  ,Analyze::debugOn(Analyze::DOUBLE,1) ? 1 : 0
#endif
      );


    ed1 = mwcStatic(e, ed, xWins, xGammonRatio, oGammonRatio, cube,
		    true, onPlayAway, opAway, cubeLife, a, this
#if defined( DEBUG )
	       ,Analyze::debugOn(Analyze::DOUBLE1,2)
#endif
      );
  } else {

    Equities::get(e, &ed, onPlayAway, opAway, cube/2,
		  xGammonRatio, oGammonRatio
#if defined( DEBUG )
		  ,Analyze::debugOn(Analyze::DOUBLE,1) ? 1 : 0
#endif
      );
  
    Equities::Es tmp = ed;

    tmp.xLow = 0;
    tmp.yLow = Equities::eWhenLose(oGammonRatio, onPlayAway, opAway, cube);

    float const edd = tmp.y(xWins);

    float edl;
	      
    if( xWins < ed.xLow ) {
      tmp.xHigh = ed.xLow; tmp.yHigh = ed.yLow;
	      
      edl = tmp.y(xWins);
    } else {
      edl = ed.y(xWins);
    }

    float const l = positionFdt(onPlayAway, opAway, cube/2,
				xGammonRatio, oGammonRatio, a);
	    
    ed1 = (1 - l) * edd + l * edl;
  }
  
  return ed1;
}


#if defined( DEBUG )
static void
dindent(int k)
{
  while( k > 0 ) {
    cout << ' ';
    --k;
  }
}
#endif

typedef Analyze::GNUbgBoard AllMoves[21];

struct MCkey {
  Analyze::GNUbgBoard 	b;
  bool			xOnPlay;
  uint			cube;
};

struct MCval {
  AllMoves m;
};

typedef  map<MCkey, MCval> MovesCache;
static MovesCache locC;

bool
operator <(MCkey const& k1, MCkey const& k2)
{
  if( k1.cube < k2.cube ) {
    return true;
  }
  if( k1.xOnPlay < k2.xOnPlay ) {
    return true;
  }
  return memcmp(&k1.b[0][0], &k2.b[0][0], sizeof(k1.b)) < 0;
}


static AllMoves*
get(Analyze::GNUbgBoard const  anBoard,
    bool const                 xOnPlay,
    unsigned int const         cube,
    bool const                 cbf)
{
  MCkey k;
  memcpy(&k.b[0][0], &anBoard[0][0], sizeof(k.b));
  k.xOnPlay = xOnPlay;
  k.cube = cube;
  
  MovesCache::iterator i = locC.find(k);
  
  if( i == locC.end() ) {
    Analyze::GNUbgBoard tmpBoard;

    MCval m;

    if( ! Analyze::gameOn(anBoard) ) {
      memcpy(&tmpBoard[0][0], &anBoard[0][0], sizeof(tmpBoard));
      SwapSides(tmpBoard);
      for(uint nr = 0; nr < 21; ++nr) {
	memcpy(&m.m[nr][0][0], &tmpBoard[0][0], sizeof(tmpBoard));
      }
    } else {
    
      for(uint nr = 0; nr < 21; ++nr) {
	memcpy(&tmpBoard[0][0], &anBoard[0][0], sizeof(tmpBoard));

	if( cbf ) {
	  findBestMove(0,roll2dice1[nr], roll2dice2[nr], tmpBoard, xOnPlay, 0);
	} else {
	  FindBestMove(0,0,roll2dice1[nr], roll2dice2[nr],tmpBoard,0, xOnPlay);
	}
	SwapSides(tmpBoard);
	memcpy(&m.m[nr][0][0], &tmpBoard[0][0], sizeof(tmpBoard));
      }
    }
    i = locC.insert( MovesCache::value_type(k, m) ).first;
  }
  return &(*i).second.m;
}

float
Analyze::R1::cubefulEquity(GNUbgBoard const  anBoard,
			   bool const        xOnPlay_,
			   uint const        nPlies_,
			   uint const        onPlayAway,
			   uint const        opAway,
			   bool 	     onPlayHoldsCube,
			   uint              cube)
{
#if defined( DEBUG )
  if( Analyze::debugOn(Analyze::DOUBLE1,1) ) {
    dindent(nPlies - nPlies_);
    cout << "R1::cubefulEquity: np " <<  nPlies_ << ", cube " << cube
	 << ", xon " << xOnPlay_ << endl;
  }
#endif
    
  bool const hasCubeAccess = cube == 1 || onPlayHoldsCube;
  
  if( nPlies_ > 0 ) {
    bool resetCube = false;
    
    if( hasCubeAccess && nPlies_ != nPlies ) {
      float const e =
	cubefulEquity0(anBoard, xOnPlay_, onPlayAway, opAway, cube, true);
      
      if( actionDouble ) {
	if( actionTake ) {
	  cube *= 2;
	  onPlayHoldsCube = true;
	  resetCube = true;
	  Equities::match.set(0, 0, cube, !xOnPlay_, -1);
	  
#if defined( DEBUG )
	  if( Analyze::debugOn(Analyze::DOUBLE1,1) ) {
	    dindent(nPlies - nPlies_);
	    cout << (xOnPlay_ ? 'X' : 'O')
		 << " doubles to " << cube << ", take" << endl;
	  }
#endif
	} else {
#if defined( DEBUG )
	  if( Analyze::debugOn(Analyze::DOUBLE1,1) ) {
	    dindent(nPlies - nPlies_);
	    cout << (xOnPlay_ ? 'X' : 'O')
		 << "doubles to " << cube << ", pass ==> equity "
		 << e << " (" <<  Equities::equityToProb(e) << ")"
		 << endl;
	  }
#endif
	  return e;
	}
      } else {
	// no double, prepare for recurse
	onPlayHoldsCube = !onPlayHoldsCube;
      }
    } else {
      // no access, prepare for recurse
      onPlayHoldsCube = !onPlayHoldsCube;
    }

    float eq = 0;
    
    AllMoves const& pm = *::get(anBoard, xOnPlay_, fullEval ? cube : 1,
				cbfMoves);

    if( advantage ) {
      Equities::push(xOnPlay == xOnPlay_ ?
		     advantage->perOp : advantage->perDoubler);
    }
    
    for(uint nr = 0; nr < 21; ++nr) {
      
#if defined( DEBUG )
      if( Analyze::debugOn(Analyze::DOUBLE1,1) ) {
	dindent(nPlies - nPlies_);
	cout << (xOnPlay_ ? 'X' : 'O')
	     << " rolls " << roll2dice1[nr] << roll2dice2[nr] << endl;
      }
#endif
      float const e =
	cubefulEquity(pm[nr], ! xOnPlay_, nPlies_ - 1,
		      opAway, onPlayAway, onPlayHoldsCube, cube);

      float const f = rollIsDouble(nr) ? 1.0/36.0 : 2.0/36.0;
      eq += f * -e;
    }

    if( resetCube ) {
      Equities::match.set(0, 0, cube/2, xOnPlay_, -1);
    }

    if( advantage ) {
      Equities::pop();
    }
    
    return eq;
  }

  float const e =
    cubefulEquity0(anBoard, xOnPlay_, onPlayAway, opAway,
		   cube, hasCubeAccess);

  return e;
}

void
Analyze::R1::cubefulEquities(GNUbgBoard const anBoard)
{
  assert( Equities::match.cube == 1 || (Equities::match.xOwns == xOnPlay) );
    
  // away for doubler
  uint const onPlayAway =
    xOnPlay ? Equities::match.xAway : Equities::match.oAway;

  // away for opponent
  uint const opAway =
    xOnPlay ? Equities::match.oAway : Equities::match.xAway;

  uint const cube = Equities::match.cube;

  if( advantage ) {
    Equities::push(advantage->perDoubler);
  }
    
  if( nPlies > 0 ) {
    float eqNoDouble = cubefulEquity(anBoard, xOnPlay, nPlies,
				     onPlayAway, opAway, true, cube);

    Equities::match.set(0, 0, 2*cube, !xOnPlay, -1);
    
    float eqDoubleTake = cubefulEquity(anBoard, xOnPlay, nPlies,
				       onPlayAway, opAway, false, 2*cube);

    Equities::match.set(0, 0, cube, xOnPlay, -1);
    
    matchProbNoDouble   = Equities::equityToProb(eqNoDouble);
    matchProbDoubleTake = Equities::equityToProb(eqDoubleTake);

    matchProbDoubleDrop = Equities::prob(onPlayAway - cube, opAway,opAway > 1);
  } else {
    // 
    cubefulEquity(anBoard, xOnPlay, nPlies, onPlayAway, opAway, true, cube);
  }

  if( advantage ) {
    Equities::pop();
  }
}

// more efficient than going straight to EvaluatePosition, since we reuse
// the moves cache.

static void
cubeless(float                      p[NUM_OUTPUTS],
	 Analyze::GNUbgBoard const  board,
	 uint const                 nPlies_,
	 bool const                 xOnPlay_,
	 float const                factor,
	 bool const                 cbfMoves)
{
  if( nPlies_ == 0 ) {
    float p1[NUM_OUTPUTS];
    
    EvaluatePosition(board, p1, 0, 0, xOnPlay_, 0, 0, 0);
    
    for(uint k = 0; k < NUM_OUTPUTS; ++k) {
      p[k] += p1[k] * factor;
    }
    
  } else {
    AllMoves const& pm = *::get(board, xOnPlay_, 1, cbfMoves);

    for(uint nr = 0; nr < 21; ++nr) {
      float const f = rollIsDouble(nr) ? 1.0/36.0 : 2.0/36.0;
      cubeless(p, pm[nr], nPlies_ - 1, !xOnPlay_, f * factor, cbfMoves);
    }
  }
}

void
Analyze::R1::setDecision(void)
{
  actionTake = matchProbDoubleTake < matchProbDoubleDrop;
  tooGood = false;
  
  if( actionTake ) {
    if( matchProbDoubleTake >= matchProbNoDouble ) {
      actionDouble = true;
    } else {
      actionDouble = false;
    }
  } else {

    if( probs[1] == 0.0 || /* no gammons */
	matchProbDoubleDrop > matchProbNoDouble ) {
      actionDouble = true;
    } else {
      actionDouble = false;
      tooGood = true;
    }
  }
}

void
Analyze::R1::analyze(GNUbgBoard const board,
                          bool xOnPlay,
                          uint nPlies,
                          const Advantage* ad,
                          const float* prb,
                          bool optimize,
                          bool cbfMoves)
{
    this->nPlies = nPlies;
    this->xOnPlay = xOnPlay;
    fullEval = !optimize;
    advantage = ad;
    this->cbfMoves = cbfMoves;

  if( ! prb ) {
    float p[NUM_OUTPUTS];
    for(uint k = 0; k < NUM_OUTPUTS; ++k) {
      p[k] = 0.0;
    }
    cubeless(p, board, nPlies, xOnPlay, 1, cbfMoves);

    if ( nPlies & 1 ) {
      InvertEvaluation(p);
    }
    
    setProbs(p);
  } else {
    setProbs(prb);
  }
  
  cubefulEquities(board);

  locC.clear();

  setDecision();
}


float
rollLuck(Analyze::Result& r, // float        p[NUM_OUTPUTS],
	 float* const probs,
	 uint const   rollIndex,
	 uint const   cube,
	 uint const   onRollAway,
	 uint const   opAway,
	 bool const   onRollHoldsCube,
	 bool const   crawford,
	 bool const   firstMove,
	 float const  cubeLife)
{
#if defined( DEBUG )
  if( Analyze::debugOn(Analyze::RLUCK,1) ) {
    cout << "Roll Luck: " << endl;
    
    cout << endl
	 << "Cube at " << cube << endl
	 << "-" << onRollAway << ",-" << opAway << " Away" << endl;
  }
#endif


  // Estimate of position equity 
  //
  float nt = 0.0;

  // Estimate of roll equity
  //
  float rnt = 0.0;

  // true if roll in this ordinal number is a double (1-1 2-2 etc),
  // false if not.
    
  // Running pointer on probabilities.
  //
  const float* p1 = probs;

  // Go over 21 possible rolls for us.
    
  for(uint k = 0; k < 21; ++k) {
    // Probabilities for doubler, after he makes his move.
    //
    float rollProbs[NUM_OUTPUTS];
      
    for(uint n = 0; n < NUM_OUTPUTS; ++n) {
      rollProbs[n] = 0.0;
    }

    {
      const float* pk1 = probs + k * 21 * NUM_OUTPUTS;
	
      for(uint k1 = 0; k1 < 21; ++k1) {
	
	float const f = rollIsDouble(k1) ? 1.0/36.0 : 2.0/36.0;
	
	for(uint n = 0; n < NUM_OUTPUTS; ++n, ++pk1) {
	  rollProbs[n] += f * *pk1;
	}
      }
    }
      
#if defined( DEBUG )
    if( Analyze::debugOn(Analyze::RLUCK,2) ) {
      cout << "after rolling " << roll2dice1[k] << roll2dice2[k] << endl;
	
      cout << "Roll probs " << rollProbs[0] << ","
	   << rollProbs[1] << "," << rollProbs[2]
	   << "," << rollProbs[3] << "," << rollProbs[4] << endl;
    }
#endif

    // cube we end up with before next roll when not doubling
    uint noDoubleCube = cube;
      
    // True when opponent will double after a no double.
    // Start with: Is cube available and not dead for him?
    //
    // if cube != 1, and we are considering a double, it means we hold
    // the cube.
    //
    bool wouldDouble =
      !crawford && noDoubleCube < opAway && (cube == 1 || !onRollHoldsCube);
      
    // Would we drop after a no-double/move/double?
    bool wouldDoubleDrop = false;
      

    if( wouldDouble ) {
      // establish those two above
	
      float const xWins1 = rollProbs[WIN];
      float const xWinsGammon1 = rollProbs[WINGAMMON];
      float const oWins1 = 1 - xWins1;
      float const oWinsGammon1 = rollProbs[LOSEGAMMON];
	
      float const xgr = (xWins1 > 0) ? xWinsGammon1 / xWins1 : 0.0;
      float const ogr = (oWins1 > 0) ? oWinsGammon1 / oWins1 : 0.0;
	
      Equities::Es eo;
      // Equities::Es eod;

      // get market window for opponent with current cube
	  
      Equities::get(eo, 0, opAway, onRollAway, cube, ogr, xgr
#if defined( DEBUG )
		    ,Analyze::debugOn(Analyze::RLUCK,2) ? 1 : 0
#endif
	);

      // wins relative to market window
	  
      float const r = ((oWins1 - eo.xLow) / (eo.xHigh - eo.xLow));

      wouldDouble = r >= 0.7;
      
      if( wouldDouble ) {
	// assume a double above 0.85, below that count market losers.
	// I am sure this can be improved.

	if( r < 0.85 ) {
	  const float* pk1 = probs + k * 21 * NUM_OUTPUTS;

	  // Number of market losers
	  uint nm = 0;
	      
	  for(uint k1 = 0; k1 < 21; ++k1, pk1 += NUM_OUTPUTS) {
	    if( 1 - pk1[0] >= eo.xHigh ) {
	      nm += rollIsDouble(k1) ? 1 : 2;
	    }
	  }

	  // heuristic: double if at least 5 market losers
	  if( nm < 5 ) {
	    wouldDouble = false;
	  }
	} else if( r > 1 ) {
	  if( ((oWins1 - eo.xHigh) / (1 - eo.xHigh)) >= 1.0 / 3.0 ) {
	    wouldDouble = false;
	  } else {
	    const float* pk1 = probs + k * 21 * NUM_OUTPUTS;

	    // Number of market losers
	    uint nm = 0;
	      
	    for(uint k1 = 0; k1 < 21; ++k1, pk1 += NUM_OUTPUTS) {
	      if( 1 - pk1[0] < eo.xHigh ) {
		nm += rollIsDouble(k1) ? 1 : 2;
	      }
	    }
	    
	    if( nm < 5 ) {
	      wouldDouble = false;
	    }
	  }
	}

	if( wouldDouble ) {
	  wouldDoubleDrop = oWins1 > eo.xHigh;

	  if( wouldDoubleDrop ) {
	    // add double/drop equitiy for this roll
	    
	    if( k == rollIndex ) {
	      rnt = 36 * -eo.yHigh;
	    }
	      
	    nt += (rollIsDouble(k) ? 36 : 2*36) * -eo.yHigh;
	  } else {
	    // else, we end up with doubled cube
		
	    noDoubleCube *= 2;
	  }
	}
      }
    }
      
    if( ! (wouldDouble && wouldDoubleDrop) ) {
      // Go over all 21 rolls
      
      for(uint k1 = 0; k1 < 21; ++k1, p1 += NUM_OUTPUTS) {
    
#if defined( DEBUG )
	if( Analyze::debugOn(Analyze::RLUCK,2) ) {
	  cout << p1[0] << "," << p1[1] << "," << p1[2] << "," << p1[3]
	       << "," << p1[4] << endl;
	}
#endif

	// How many actual rolls does this cover?
	//
	uint const f = rollIsDouble(k) ?
	  (rollIsDouble(k1) ? 1 : 2) : (rollIsDouble(k1) ? 2 : 4);

	float const xWins1 = p1[WIN];
	float const xWinsGammon1 = p1[WINGAMMON];
	float const oWins1 = 1 - xWins1;
	float const oWinsGammon1 = p1[LOSEGAMMON];
	
	float const ogr = (oWins1 > 0) ? oWinsGammon1 / oWins1 : 0.0;
	float const xgr = (xWins1 > 0) ? xWinsGammon1 / xWins1 : 0.0;

	Equities::Es ed;
	Equities::Es e;

	float e1;
	
	if( crawford ) {
	  if( onRollAway == 1 ) {
	    Equities::getCrawfordEq(ed, opAway, ogr);
	    e.xHigh = 1.0; e.yHigh = 1.0;
	    e.xLow = 0.0; e.yLow = -ed.yHigh;
	  } else {
	    Equities::getCrawfordEq(e, onRollAway, xgr);
	  }

	  e1 = e.y(xWins1);
	  
	} else {
	  if( cube == 1 || onRollHoldsCube ) {
	    Equities::get(e, &ed, onRollAway, opAway, noDoubleCube, xgr, ogr
#if defined( DEBUG )
			  ,Analyze::debugOn(Analyze::RLUCK,2) ? 1 : 0
#endif
	      );
	  } else {
	    Equities::get(e, &ed, opAway, onRollAway, noDoubleCube, ogr, xgr
#if defined( DEBUG )
			  ,Analyze::debugOn(Analyze::RLUCK,2) ? 1 : 0
#endif
	      );
	    e.reverse();
	    ed.reverse();
	  }

//  	  e1 = e2(e, ed, xWins1, xgr, ogr, noDoubleCube, onRollHoldsCube,
//  		  onRollAway, opAway, doubleTH
//  #if defined( DEBUG )
//  		  ,false 
//  #endif
//  	    );

	  e1 = mwcStatic(e, ed, xWins1, xgr, ogr, noDoubleCube,
			 onRollHoldsCube, onRollAway, opAway, cubeLife, 0, 0
#if defined( DEBUG )
			 ,false 
#endif
	    );
	}
	
	if( k == rollIndex ) {
	  rnt += (rollIsDouble(k1) ? 1 : 2) * e1;
	}
	  
	nt += f * e1;
      }
    }
  }

  nt /= 36 * 36;
  rnt /= 36.0;

  if( firstMove ) {
    nt = 2 * Equities::prob(onRollAway, opAway, crawford) - 1.0;
  }

  r.preRollP = (1+nt)/2.0;
  r.rollP = (1+rnt)/2.0;

  return (rnt - nt) / 2.0;
}


uint
numberOfOppMoves(int const board[2][25])
{
  float x[4];
  
  for(uint side = 0; side < 2; ++side) {
    const int* const anBoard = board[1-side];
    const int* const anBoardOpp = board[side];
    
    int nOppBack;
    
    for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
      if( anBoardOpp[nOppBack] ) {
	break;
      }
    }
    
    nOppBack = 23 - nOppBack;

    uint n = 0;
    
    for(uint i = nOppBack + 1; i < 25; ++i) {
      if( anBoard[ i ] ) {
	n += ( i + 1 - nOppBack ) * anBoard[ i ];
      }
    }
    
    if( n == 0 ) {
      return 0;
    }
    
    x[side] = n / (15 + 152.0);

    n = 0;
    for(uint i = 0; i < 6; i++ ) {
      n += anBoardOpp[ i ] > 1;
    }
    
    x[2 + side] = ( 36 - ( n - 6 ) * ( n - 6 ) ) / 36.0; 
  }

  int n0 = int(0.5 + (8.78776 * x[0] + 5.84057 * x[1] +
    -4.43768 * x[2] + -1.06146 * x[3] + 7.48450));
  
  int n1 = int(0.5 + (8.78776 * x[2] + 5.84057 * x[3] +
    -4.43768 * x[0] + -1.06146 * x[1] + 7.48450));

  int n = std::min(n0, n1);
  
  if( n < 0 ) {
    n = 0;
  }

  if( n > 22 ) {
    n = 22;
  }
  
  return n;
}

