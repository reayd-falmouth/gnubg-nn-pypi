/*
 * player.cc
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

#include "minmax.h"

#include "bgdefs.h"
#include "misc.h"
#include "equities.h"
#include "danalyze.h"
#include "analyze.h"
#include "player.h"


Player::Player(void) :
  //acceptCubeVig(0.8),
  lastXwpf(-2),
  xwpfEtable(0),
  owpfEtable(0)
{}

Player::~Player(void)
{
  delete[] xwpfEtable;
  delete[] owpfEtable;
}

void 
Player::setEqTables(float const xwpf) const
{
  if( lastXwpf != xwpf ) {
    delete[] xwpfEtable;
    delete[] owpfEtable;
    
    xwpfEtable = Equities::getTable(xwpf, 0.26);
    owpfEtable = Equities::getTable(1 - xwpf, 0.26);
    lastXwpf = xwpf;
  }
}

Player::R1 const&
Player::rollOrDouble(PrintMatchBoard const  board,
		     bool const             xOnPlay,
		     double const           odf,
		     bool const             APopt,
		     bool const             cbfMoves,
		     const float*           prb) const 
{
  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  
  if( (xOnPlay ? xAway : oAway) <= cube ) {
    doubleInfo.actionDouble = false;
    doubleInfo.actionTake = true;
    doubleInfo.tooGood = false;

    return doubleInfo;
  }

  GNUbgBoard b;
    
  set(board, b, xOnPlay);

  Advantage ad;
  Advantage* a = 0;
  if( odf > 0.5 ) {
    if( isRace(b) ) {
      setEqTables(odf);

      ad.perDoubler = xwpfEtable;
      ad.perOp      = owpfEtable;
      a = &ad;
    }
  }

  doubleInfo.analyze(b, xOnPlay, nPliesToDouble, a, prb, APopt, cbfMoves);
  
  if( nPliesToDouble < nPliesToDoubleVerify &&
      (doubleInfo.actionDouble || doubleInfo.tooGood ||
       ( (doubleInfo.matchProbDoubleDrop - doubleInfo.matchProbDoubleTake)/
	 (doubleInfo.matchProbDoubleDrop - doubleInfo.matchProbNoDouble) <
	 1.2 ) ) ) {
    doubleInfo.analyze(b, xOnPlay, nPliesToDoubleVerify, a, 0, APopt,cbfMoves);
  }
  
  return doubleInfo;
}


bool
Player::accept(PrintMatchBoard const  board,
	       bool const             xOnPlay,
	       uint const             nPlies,
	       double                 odf,
	       Analyze::R1&           r) const
{
  GNUbgBoard b;
    
  set(board, b, xOnPlay);
  
  Advantage ad;
  Advantage* a = 0;
  if( odf > 0.5 ) {
    if( isRace(b) ) {
      setEqTables(odf);

      ad.perDoubler = owpfEtable;
      ad.perOp      = xwpfEtable;
      a = &ad;
    }
  }

  r.analyze(b, xOnPlay, nPlies, a);
  
  return r.actionTake;
}


bool
Player::resignsOpAtRoll(PrintMatchBoard const  board,
			uint const             nPoints,
			bool const             xOnPlay,
			uint const             nPlies,
			float* const           gammonChances) const
{
  if( ! (nPoints == 1 || nPoints == 2) ) {
    return true;
  }

  float p[NUM_OUTPUTS];
  probs(p, board, xOnPlay, nPlies);
  InvertEvaluation(p);

  if( gammonChances ) {
    *gammonChances = p[WINGAMMON];
  }
  
  if( p[WINGAMMON] == 0 ) {
    return true;
  }

  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  
  int const rsAway = xOnPlay ? xAway : oAway;
  int const away = xOnPlay ? oAway : xAway;

  float const wins = p[WIN];
  float const winsGammon = p[WINGAMMON];
  float const rsWins = 1 - wins;
  float const rsWinsGammon = p[LOSEGAMMON];

  float const gr = winsGammon / wins;
  float const rsgr = (rsWins > 0) ? rsWinsGammon / rsWins : 0.0;

  float const e =
    wins * Equities::eWhenWin(gr, away, rsAway, cube) +
    rsWins * Equities::eWhenWin(rsgr, away, rsAway, cube);
  
  float const e1 = Equities::value(away - cube * nPoints, rsAway);
  
  return (e1 >= e);
}


bool
Player::resigns(PrintMatchBoard const  board,
		uint const             nPoints,
		bool const             xOnPlay,
		uint const             nPlies,
		float* const           gammonChances) const
{
  if( ! (nPoints == 1 || nPoints == 2) ) {
    return true;
  }

  float p[NUM_OUTPUTS];
  
  probs(p, board, xOnPlay, nPlies);

  if( gammonChances ) {
    *gammonChances = p[WINGAMMON];
  }

  float const e = Equities::money(p);

  return e <= nPoints;
  
//    if( nPoints == 2 ) {
//      return p[WINBACKGAMMON] < 0.05;
//    }

//    return r.probs[WINGAMMON] == 0.0;
  
//    R1 r;
//    analyze(r, board, xOnPlay, nPliesToDouble, nPliesToDoubleVerify);

//    if( gammonChances ) {
//      *gammonChances = r.probs[WINGAMMON];
//    }
  
//    return (r.probs[WINGAMMON] == 0.0) || !r.tooGood;
}

// 2 6-6 is low enough?
float const lowProb = 1.0 / (36.0 * 36.0);

static int
resignDecision(float const  probs[NUM_OUTPUTS],
	       bool const   oneOk,
	       bool const   twoOK)
{
  if( probs[WIN] <= lowProb ) {
    if( oneOk ) {
      return 1;
    }
    
    if( probs[LOSEGAMMON] >= 1-lowProb ) {
      if( probs[LOSEBACKGAMMON] == 0.0 || twoOK ) {
	return 2;
      }
    }
    else if( probs[LOSEGAMMON] < lowProb ) {
      return 1;
    }
  }

  return 0;
}

int
Player::offerResign(uint const            nPlies,
		    uint const            nPliesVerify,
		    PrintMatchBoard const board,
		    bool const            xOnPlay) const
{
  float p[NUM_OUTPUTS];
  
  probs(p, board, xOnPlay, nPlies);
  InvertEvaluation(p);
  
  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  
  bool const singleOK = cube >= (xOnPlay ? xAway : oAway);
  bool const gammonOK = 2*cube >= (xOnPlay ? xAway : oAway);
  
  int resignLevel = resignDecision(p, singleOK, gammonOK);

  if( resignLevel && nPlies < nPliesVerify ) {
    probs(p, board, xOnPlay, nPliesVerify);
    InvertEvaluation(p);

    resignLevel = resignDecision(p, singleOK, gammonOK);
  }

  return resignLevel;
}


#if defined( nono )
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

  int n = int(0.5 + (8.78776 * x[0] + 5.84057 * x[1] +
    -4.43768 * x[2] + -1.06146 * x[3] + 7.48450));

  if( n < 0 ) {
    n = 0;
  }

  if( n > 22 ) {
    n = 22;
  }
  
  return n;
}



bool
Player::accept(PrintMatchBoard const  board,
	       float const            xwpf,
	       bool const             xOnPlay,
	       uint const             nPlies,
	       Analyze::R1&           r) const
{
  float p[NUM_OUTPUTS];
  
  // r.xOnPlay = xOnPlay;
  probs(p, board, xOnPlay, nPlies);
  r.setProbs(p);

  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  
  uint const dbAway = xOnPlay ? xAway : oAway;
  uint const acAway = xOnPlay ? oAway : xAway;

  setEqTables(xOnPlay ? 1-xwpf : xwpf);
  
  {
    GNUbgBoard anBoard;

    set(board, anBoard, xOnPlay);
    float const a = ((xOnPlay ? xwpf : (1-xwpf)) - 0.5) / 20.0;
    ap(a, p, anBoard);

    Equities::push(owpfEtable);
  }
  
  
  float const xWins = p[WIN];
  float const xWinsGammon = p[WINGAMMON];
  float const oWins = 1 - xWins;
  float const oWinsGammon = p[LOSEGAMMON];

  float const oGammonRatio = (oWins > 0) ? oWinsGammon / oWins : 0.0;
  float const xGammonRatio = (xWins > 0) ? xWinsGammon / xWins : 0.0;

  
  if( dbAway <= cube ) {
    // ego problem
    r.matchProbDoubleDrop = 0.0;
    r.matchProbDoubleTake =
      Equities::equityToProb(
	oWins * Equities::eWhenWin(oGammonRatio, acAway, dbAway, 4 * cube) +
	xWins * Equities::eWhenLose(xGammonRatio, dbAway, acAway, 4 * cube));

    r.actionTake = true;
  } else {

    Equities::Es eq, eqd;

    Equities::get(eq, &eqd,
		  dbAway, acAway, cube, xGammonRatio, oGammonRatio
#if defined( DEBUG )
		  ,Analyze::debugOn(Analyze::PLAYERAC, 1)
#endif
      );
  
    r.matchProbDoubleDrop = Equities::equityToProb(-eq.yHigh);

    if( acAway <= 2*cube ) {
      // cube dead after take
      r.takeTH = 1 - eq.xHigh;

      r.matchProbDoubleTake = Equities::equityToProb(-eqd.e(xWins));
    } else {
      eqd.reverse();
    
      float const ce = 0.78;
    
      float const nom = (-eq.yHigh - eqd.yLow)/2;
      float const dom = (eqd.yHigh - eqd.yLow)/2;

      float const xt = nom / (dom * (1 - ce + ce/eqd.xHigh));
    
      r.takeTH = xt;
    
      //r.takeTH = eqd.x(-eq.yHigh);
    
      r.matchProbDoubleTake = Equities::equityToProb(eqd.e(oWins));
    }

    // modify calculated probs to match matchProbDoubleTake/Drop
  
    r.setProbs(p);
  
    r.actionTake = oWins >= r.takeTH;
  }

#if defined( DOICC )
  const Equities::EqTable et = 
#endif
    Equities::pop();                                            assert( ! et );
  
  return r.actionTake;
}
#endif


//    if( a ) {
//      delete a->perPlayer;
//      delete a->perOp;
//    }
  
#if defined( nono )
  if( r.actionDouble && odf > 0.5 ) {
    Equities::EqTable et = Equities::getTable(odf, 0.26);

    Equities::push(et);

    uint const dbAway = xOnPlay ? xAway : oAway;
    uint const acAway = xOnPlay ? oAway : xAway;

    float p[5];
    r.get(p);
    
    float const xWins = p[WIN];
    float const xWinsGammon = p[WINGAMMON];
    float const oWins = 1 - xWins;
    float const oWinsGammon = p[LOSEGAMMON];

    float const oGammonRatio = (oWins > 0) ? oWinsGammon / oWins : 0.0;
    float const xGammonRatio = (xWins > 0) ? xWinsGammon / xWins : 0.0;
    
    Equities::Es eq, eqd;
    
    Equities::get(eq, &eqd,
		  dbAway, acAway, cube, xGammonRatio, oGammonRatio
#if defined( DEBUG )
		,Analyze::debugOn(Analyze::PLAYERAC,1)
#endif
    );

    float const eqy = eq.yHigh;
    
    eq.xHigh = 1;
    eq.yHigh = Equities::eqWhenWin(xGammonRatio, dbAway, acAway, cube);

    GNUbgBoard anBoard;
    set(board, anBoard, xOnPlay);
    
    float const a = (odf - 0.5) / 20.0;
    
    ap(a, p, anBoard);

    float const eNoUseCube = eq.y(p[WIN]);

    float const eAfterD = eqd.y(p[WIN]);

    eq.xHigh = p[WIN];
    eq.yHigh = eAfterD;

    float const dPoint = eq.xLow +
      (eqy - eq.yLow) * ((eq.xHigh - eq.xLow) / (eq.yHigh - eq.yLow));
    
    Equities::pop();
    delete et;
  }
#endif


#if defined( nono )
    //    float const ce = 0.8;
//      {
//        GNUbgBoard anBoard;

//        set(board, anBoard, xOnPlay);
//        uint const nm = numberOfOppMoves(anBoard);

//        ce = 0.8 + 0.2 * (min(nm, 10U) / 10.0);
//      }
    float const ce = 0.3;
    
//      float const x = eqd.xLow + (eqd.xHigh - eqd.xLow) * ce;
//      float const y = eqd2.y(x);

    if( 0 ) {
      // not true when return double is aggresive, say cube dead for op
      float const x = eqd.xHigh * (1 - ce) + ce * 1;
      eqd.xHigh = x;
    }
    // eqd.yHigh = y;
#endif



  
#if defined( nono )
// from accept
  float p[NUM_OUTPUTS];
  
  // r.xOnPlay = xOnPlay;
  probs(p, board, xOnPlay, nPlies);
  r.setProbs(p);

  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  
  uint const dbAway = xOnPlay ? xAway : oAway;
  uint const acAway = xOnPlay ? oAway : xAway;

  float const xWins = p[WIN];
  float const xWinsGammon = p[WINGAMMON];
  float const oWins = 1 - xWins;
  float const oWinsGammon = p[LOSEGAMMON];

  float const oGammonRatio = (oWins > 0) ? oWinsGammon / oWins : 0.0;
  float const xGammonRatio = (xWins > 0) ? xWinsGammon / xWins : 0.0;

  if( dbAway <= cube ) {
    // ego problem
    r.matchProbDoubleDrop = 0.0;
    r.matchProbDoubleTake =
      Equities::equityToProb(
	oWins * Equities::eWhenWin(oGammonRatio, acAway, dbAway, 4 * cube) +
	xWins * Equities::eWhenLose(xGammonRatio, dbAway, acAway, 4 * cube));

    r.actionTake = true;
    return true;
  }

  Equities::Es eq, eqd;

  Equities::get(eq, &eqd,
		dbAway, acAway, cube, xGammonRatio, oGammonRatio
#if defined( DEBUG )
		,Analyze::debugOn(Analyze::PLAYERAC,1)
#endif
    );
  
  r.matchProbDoubleDrop = Equities::equityToProb(-eq.yHigh);

  if( acAway <= 2*cube ) {
    // cube dead after take
    r.takeTH = 1 - eq.xHigh;

    r.matchProbDoubleTake = Equities::equityToProb(-eqd.e(xWins));
  } else {
    eqd.reverse();

    // if cube dead for doubler, cube is alive
    float const ce = (dbAway <= 2*cube) ? 1.0 : 2.0/3.0;
    
    float const nom = (-eq.yHigh - eqd.yLow)/2;
    float const dom = (eqd.yHigh - eqd.yLow)/2;

    float const xt = nom / (dom * (1 - ce + ce/eqd.xHigh));
    
    //float const xt = eqd.x(-eq.yHigh);
    r.takeTH = xt;
    
    r.matchProbDoubleTake = Equities::equityToProb(eqd.e(oWins));
  }
	  
  r.actionTake = oWins >= r.takeTH;
  
  return r.actionTake;
#endif


//    if( r.actionDouble ) {
//      float const diff =
//  	(r.actionTake ? r.matchProbDoubleTake : r.matchProbDoubleDrop) -
//  	r.matchProbNoDouble;

//      // take care of numerical instability
      
//      int const d = int(1000.0 * diff + 0.5);
      
//      {                                                     assert( d >= 0 ); }
      
//      if( d <= 10 ) {
//        float const th = Analyze::doubleTH;
  
//        Analyze::doubleTH = 0.8;
    
//        r.analyze(*this, board, xOnPlay, nPliesToDouble, nPliesToDoubleVerify,a);

//        Analyze::doubleTH = th;
//      }
//    }

 
