/*
 * analyze.cc
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
#if defined( __GNUG__ )
#pragma implementation
#endif

#include <iostream> /**/

#include <string>
#include <cmath>

#include "minmax.h"

#include <algorithm>

#include "bgdefs.h"

#include "misc.h"
#include <osr.h>

#include "bms.h"
#include "bm.h"
#include "analyze.h"
#include "equities.h"
#include "dice_gen.h"

#if defined(_WIN32)
#include <cstring>
#include <cctype>

static int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int d = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (d != 0) return d;
        ++s1;
        ++s2;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
#else
#include <strings.h>  // For strcasecmp on POSIX
#endif

#include <vector>
#include <array>
#include <cstring>  // for memcpy

using namespace std;

#if defined(DEBUG)
long
Analyze::debugFlags = 0;

unsigned int
Analyze::level[32];

void
Analyze::setDebug(uint t, uint l)
{
  debugFlags |= (0x1 << t);
  level[t] = l;
}

bool
Analyze::setDebug(const char* st)
{
  uint l = 0;
	
  if( isdigit(*st) ) {
    l = *st - '0';

    ++st;
  }

  if( strcasecmp(st, "double") == 0 ) {
    setDebug(DOUBLE, l);
  } else if( strcasecmp(st, "double1") == 0 ) {
    setDebug(DOUBLE1, l);
  } else if( strcasecmp(st, "rluck") == 0 ) {
    setDebug(RLUCK, l);
  } else if( strcasecmp(st, "playerac") == 0 ) {
    setDebug(PLAYERAC, l);
  } else {
    return false;
  }

  return true;
}

#endif



Analyze::Result::Result(uint const m, float const th, uint const r) :
  maxMoves(m),
  maxDiffFromBest(th),
  moveRolloutGames(r),
  moves(new Move [maxMoves])
{}



float 
Analyze::Result::m(uint const n) const
{
  {                          assert( n < nMoves || (nMoves == 0 && n == 0) ); }

  return moves[n].mwc;
}


Analyze::Result::~Result()
{
  delete [] moves;
}


uint
Analyze::nPliesMove = 1;

uint
Analyze::nPliesVerifyBlunder = 2;

uint
Analyze::nPliesToDouble = 0;

uint
Analyze::nPliesToDoubleVerify = 2;

float
Analyze::jokerDiff = 0.1;

uint
Analyze::maxPliesJoker = 2;

float
Analyze::blunderTH = 0.065;

float
Analyze::cubeLife = 0.65;

bool
Analyze::calcLuck = false;

uint
Analyze::netSearchSpace = 10;

bool
Analyze::useOSRinRollouts = true;

extern "C" char* weightsVersion;

const char*
Analyze::weightsVersion = ::weightsVersion;

Analyze::Analyze(void) :
  prr(new float [21 * 21 * NUM_OUTPUTS]),
  pauch(new unsigned char [21 * 21 * 10]),
  wide(false),
  diceGen(*(new GetDice))
{}


Analyze::~Analyze(void)
{
  delete [] prr;
  delete [] pauch;

  delete &diceGen;
}

static string initalNetName;


const char*
Analyze::init(const char* netFile, bool const shortCuts)
{
  if( ! netFile ) {
    netFile = getenv("GNUBGWEIGHTS");
  }
  
  const char* const envHome = getenv("GNUBGHOME");

  if( netFile ) {
    initalNetName = netFile;
  } else {
    if( envHome ) {
      initalNetName = string(envHome) + '/' + "gnubg.weights";
    } else {
      cerr << "GNUBGHOME/GNUBGWEIGHTS not set?" << endl;
      return 0;
    }
  }
  
#if defined( LOADED_BO )
  if( ! envHome ) {
    cerr << "GNUBGHOME not set" << endl;
    return 0;
  }
#ifdef WIN32
  string const os = string(envHome) + '\\' + "gnubg_os0.bd";
  string const ts = string(envHome) + '\\' + "gnubg_ts0.bd";
#else
  string const os = string(envHome) + '/' + "gnubg_os0.bd";
  string const ts = string(envHome) + '/' + "gnubg_ts0.bd";
#endif // Win32
#endif

#if defined( OS_BEAROFF_DB )
  string o;
  if( envHome ) {
#ifdef WIN32
    o = string(envHome) + '\\' + "gnubg_os.db";
#else
    o = string(envHome) + '/' + "gnubg_os.db";
#endif // Win32
  }
#endif

  const char* version = EvalInitialise(initalNetName.c_str()
#if defined( LOADED_BO )
				       , os.c_str(), ts.c_str()
#endif 
#if defined( OS_BEAROFF_DB )
				       , o.c_str()
#endif
    );

  if( ! version ) {
    cerr << "Failed to initialise net:" << initalNetName.c_str()
#if defined( LOADED_BO ) || defined( OS_BEAROFF_DB )
    << " or bearoff databases:" << string(envHome) << "/gnubg_*.bd"
#endif
    << endl;
    return 0;
  }

  Equities::set(Equities::mec26);
  Equities::set(Equities::Snowie);

  calcLuck = false;
  
  if( shortCuts ) {
    setShortCuts(1);
    setNetShortCuts(netSearchSpace);
  }
  
  Analyze::srandom(time(0));

  return version;
}

const char*
Analyze::initializedWith(void)
{
  return initalNetName.c_str();
}

bool
Analyze::setScore(uint const xAway, uint const oAway)
{
  if( xAway == 0 && oAway == 0 ) {
    Equities::match.reset();
    matchRange = 0;
    
    return true;
  }

  // set cube to 1, non crawford
  
  bool const ok = Equities::match.set(xAway, oAway, 1, false, 0);
  
  matchRange = Equities::match.range();
  
  return ok;
}

bool
Analyze::setScore(uint const xScore, uint const oScore, uint const matchLen)
{
  uint const xAway = matchLen - xScore;
  uint const oAway = matchLen - oScore;
  
//   if( xAway <= 15 && oAway <= 15 ) {
//     Equities::set(Equities::Snowie);
//   } else {
//     Equities::set(Equities::mec26);
//   }
  
  return setScore(xAway, oAway);
}

void
Analyze::setCube(uint const cube, bool const xOwns)
{
  Equities::match.set(0, 0, cube, xOwns, -1);
  
  matchRange = Equities::match.range();
}

void
Analyze::crawford(bool const on)
{
  Equities::match.set(0, 0, 0, false, on);
  
  matchRange = Equities::match.range();
}

float
Analyze::matchEquity(void) const
{
  return Equities::prob(Equities::match.xAway,
			Equities::match.oAway, Equities::match.crawfordGame);
}

// direction  -> O (+) -> 0, X (-) -> 1
// !direction -> O (+) -> 1, X (-) -> 0

void
Analyze::set(PrintMatchBoard const  board,
	     GNUbgBoard             gBoard,
	     bool const             direction)
{
  memset(gBoard[0], 0, 25 * sizeof(int));
  memset(gBoard[1], 0, 25 * sizeof(int));

  uint const side = direction ? 0 : 1;
  
  if( board[0] != 0 ) {
    // bar of O
    gBoard[side][24] = abs(board[0]);
  }
  
  if( board[25] != 0 ) {
    // bar of X
    
    gBoard[1-side][24] = abs(board[25]);
  }

  for(uint k = 1; k < 25; ++k) {
    int const n = board[k];
    
    if( n != 0 ) {
      if( n > 0 ) {
	gBoard[side][24 - k] = n;
      } else {
	gBoard[1-side][k - 1] = -n;
      }
    }
  }
}

class SortMWC {
public:
  typedef Analyze::Result::Move Move;
  
  int	operator ()(Move const& i1, Move const& i2) const;
};


inline int
SortMWC::operator ()(Move const& i1, Move const& i2) const 
{
  return i1.mwc > i2.mwc;
}

template<class T>
inline void
exch(T& s1, T& s2)
{
  T s = s1;
  s1 = s2;
  s2 = s;
}

  
void
Analyze::Result::sortByMatchMCW(bool const xOnPlay)
{
  for(uint i = 0; i < nMoves; i++) {
    float* const ar = moves[i].probs;

    moves[i].mwc = Equities::mwc(ar, xOnPlay);
  }

  int moveMade[8];
  memcpy(moveMade, moves[actualMove].desc, sizeof(moveMade));
  
  sort(moves, moves + nMoves, SortMWC());
  
  for(uint i = 0; i < nMoves; i++) {
    if( memcmp(moveMade, moves[i].desc, sizeof(moveMade)) == 0 ) {
      actualMove = i;
      break;
    }
  }
}


void
Analyze::Result::setBlunder(float const range)
{
  if( actualMove == 0 ) {
    blunder = false;
  } else {
    blunder = ((moves[0].mwc - moves[actualMove].mwc) / range) >= blunderTH;
  }
}


void
Analyze::rollMoves(Analyze::Result& r, movelist& ml, bool const xOnPlay)
{
    // Save move boards. they are destroyed by EvaluatePosition
    std::vector< std::vector<unsigned char> > aucs(r.nMoves, std::vector<unsigned char>(10));
    for (uint i = 0; i < r.nMoves; i++) {
        memcpy(aucs[i].data(), ml.amMoves[i].auch, aucs[i].size());
    }

    Analyze::GNUbgBoard tmpBoard;

    float arStdDev[NUM_OUTPUTS];
    uint const nPlies = 0;
    uint const nTruncate = 500;

    for (uint i = 0; i < r.nMoves; i++) {
        float* const ar = r.moves[i].probs;

        PositionFromKey(tmpBoard, aucs[i].data());

        SwapSides(tmpBoard);

        rollout(tmpBoard, !xOnPlay, ar, arStdDev, nPlies, nTruncate,
                r.moveRolloutGames, i);

        InvertEvaluation(ar);
    }

    r.sortByMatchMCW(xOnPlay);
}


extern float
rollLuck(Analyze::Result& r, //float        p[NUM_OUTPUTS],
	 float* const probs,
	 uint const   rollIndex,
	 uint const   cube,
	 uint const   onRollAway,
	 uint const   opAway,
	 bool const   onRollHoldsCube,
	 bool const   crawford,
	 bool const   firstMove,
	 float const  cubeLife);

bool
Analyze::analyze(Result&                r,
		 PrintMatchBoard const  board,
		 bool const             xOnPlay,
		 uint const             roll1,
		 uint const             roll2,
		 PrintMatchBoard const  after,
		 bool const             firstMove)
{
  GNUbgBoard anBoard;

  set(board, anBoard, xOnPlay);

  GNUbgBoard afterBoard;
  // after
  set(after, afterBoard, xOnPlay);

  unsigned char auch[10];
  PositionKey(afterBoard, auch);

  movelist ml;

  r.xonplay = xOnPlay;

  findBestMoves(ml, nPliesMove, roll1, roll2, anBoard, auch, xOnPlay,
		r.maxMoves, r.maxDiffFromBest);

  r.nMoves = ml.cMoves;
    
  
  static float sp[NUM_OUTPUTS];
  
  if( r.nMoves > 0 ) {
    r.actualMove = ml.cMoves+1;
  
    for(uint i = 0; i < r.nMoves; i++) {
      Result::Move& mi = r.moves[i];
      ::move& gmi = ml.amMoves[i];
      
      mi.probs = gmi.pEval;

      memcpy(mi.desc, gmi.anMove, sizeof(gmi.anMove));

      mi.mwc = gmi.rScore;
      
      {                                                   assert( mi.probs ); }
    
#if !defined(NDEBUG)
      {
	const int* const m = mi.desc;
	for(uint k = 0; k < 4 && m[k] >= 0; ++k) {
	  if( ! (m[2*k] < 25 && ((m[2*k+1] <= 0) || m[2*k+1] < 25)) ) {
	    assert(0);
	  }
	}
      }
#endif

      if( EqualKeys(auch, ml.amMoves[i].auch) ) {
	r.actualMove = i;
      }
    }

    if( !( r.actualMove < r.nMoves ) ) {
      // none of the legal moves result in given board
      return false;
    }

    r.nPlies = nPliesMove;
    
    if( r.moveRolloutGames > 0 ) {
      rollMoves(r, ml, xOnPlay);
    }

    // see if blunder
    
    r.setBlunder(matchRange);

    // verify blunder if needed
    
    if( r.blunder && r.moveRolloutGames == 0 ) {
      if( nPliesMove < nPliesVerifyBlunder ) {

	// Save move boards, which are destroyed by EvaluatePosition
      
	unsigned char aucs[r.nMoves][10];
	for(uint i = 0; i < r.nMoves; i++) {
	  memcpy(aucs[i], ml.amMoves[i].auch, sizeof(aucs[i]));
	}
      
	GNUbgBoard tmpBoard;
      
	for(uint i = 0; i < r.nMoves; i++) {
	  float* const ar = r.moves[i].probs;
	
	  PositionFromKey(tmpBoard, aucs[i]);

	  SwapSides(tmpBoard);

	  EvaluatePosition(tmpBoard, ar, nPliesVerifyBlunder, 0,
			   !xOnPlay, 0, 0, 0);

	  InvertEvaluation(ar);
	}

	r.sortByMatchMCW(xOnPlay);

	// see if still blunder
      
	r.setBlunder(matchRange);

	r.nPlies = nPliesVerifyBlunder;
      }
    }
  }

  uint const xAway = Equities::match.xAway;
  uint const oAway = Equities::match.oAway;
  uint const cube = Equities::match.cube;
  bool const xOwns = Equities::match.xOwns;
  
  // not crawford, not first move and not 1,1 away
  
  bool wantToDouble = (!Equities::match.crawfordGame &&
		       !firstMove && !(xAway == 1 && oAway == 1));

  if( wantToDouble ) {
    if( cube == 1 ) {
      // centered cube, see if post crawford
      
      if( (xOnPlay && xAway == 1) || (!xOnPlay && oAway == 1) ) {
	wantToDouble = false;
      }
    } else {
      if( xOnPlay && xOwns ) {
	// See if cube dead
      
	if( xAway <= cube ) {
	  wantToDouble = false;
	}
      } else if( !xOnPlay && !xOwns ) {
	if( oAway <= cube ) {
	  wantToDouble = false;
	}
      } else {
	wantToDouble = false;
      }
    }
  }

  if( calcLuck ) {
    float p1[NUM_OUTPUTS];

    wide = true;
    probs(p1, board, xOnPlay, 2, 2);
    wide = false;

      uint const hr = std::max(roll1, roll2);
      uint const k = (hr * (hr - 1)) / 2 + std::min(roll1, roll2) - 1;

      float l = rollLuck(r, prr, k, cube,
		       xOnPlay ? xAway : oAway,
		       xOnPlay ? oAway : xAway,
		       xOwns == xOnPlay,
		       Equities::match.crawfordGame, firstMove,
		       cubeLife);

    r.rollLuck = xOnPlay ? l : -l;
  } else {
    r.rollLuck = -1;
  }

  r.lucky = false;

  // Equity of position, before roll
  float me;

  if( wantToDouble ) {
    analyze(r.doubleAna, board, xOnPlay, nPliesToDouble, nPliesToDoubleVerify);

    r.doubleAnaDone = true;

  } else {
    r.doubleAnaDone = false;
    
    r.doubleAna.actionDouble = false;
  }

  // me should be one ply higher than moves
  
  if( r.doubleAnaDone && (r.nPlies + 1 == r.doubleAna.nPlies) ) {
    me = r.doubleAna.match();
  } else {
    uint np = 1 + (r.blunder ? nPliesVerifyBlunder : nPliesMove);
    
    if( np > maxPliesJoker ) {
      --np;
    }
    
    EvaluatePosition(anBoard, sp, np, 0, xOnPlay, 0,0,0);

    me = Equities::mwc(sp, xOnPlay);
  }

  if( r.nMoves == 0 ) {
    
    SwapSides(anBoard);

    r.nPlies = nPliesMove;

    if( r.moveRolloutGames == 0 ) {
      EvaluatePosition(anBoard, sp, nPliesMove, 0, !xOnPlay, 0,0,0);
    } else {
      float arStdDev[NUM_OUTPUTS];
      
      rollout(anBoard, !xOnPlay, sp, arStdDev, 0, 500,
	      r.moveRolloutGames, -1);
    }

    InvertEvaluation(sp);

    r.moves[0].probs = sp;

    r.moves[0].mwc = Equities::mwc(sp, xOnPlay);
    
    r.actualMove = 0;
  }

  float const bestMove = r.m(0U);

  r.rollMinusPositionEquityDiff = bestMove - me;

  float const normDif =  r.rollMinusPositionEquityDiff / matchRange;
  
  if( normDif >= jokerDiff ) {
    r.lucky = true;
  } else if( -normDif >= jokerDiff ) {
    r.lucky = true;
  }

  return true;
}


void
Analyze::probs(float                  p[NUM_OUTPUTS],
	       PrintMatchBoard const  board,
	       bool const             xOnPlay,
	       uint const             nPlies,
	       uint const             n,
	       bool const             getAuch) const
{
  {                                                    assert( n <= nPlies ); }
  GNUbgBoard anBoard;

  set(board, anBoard, xOnPlay);

  EvaluatePosition(anBoard, p, nPlies, (wide && nPlies >= 2) ? 1 : 0,
		   xOnPlay, n ? prr : 0, n, getAuch ? pauch : 0);
}



void
Analyze::analyze(R1&                    r,
		 PrintMatchBoard const  board,
		 bool const             xOnPlay,
		 uint const             nPlies,
		 uint const             nPliesVerify)
{
  GNUbgBoard b;
  set(board, b, xOnPlay);

  analyze(r, b, xOnPlay, nPlies, nPliesVerify);
}

void
Analyze::analyze(R1&               r,
		 GNUbgBoard const  b,
		 bool const        xOnPlay,
		 uint const        nPlies,
		 uint const        nPliesVerify)
{
  if( r.nRolloutGames > 0 ) {
    float arOutput[NUM_OUTPUTS], arStdDev[NUM_OUTPUTS];

    if( r.rollOutProbs ) {
      // diceGen.startSave in rollout, from nSeq == 0
      rollout(b, xOnPlay, arOutput, arStdDev, 0, 500, r.nRolloutGames, 0);

      // for first rolloutCubefull
      diceGen.startRetrive();
    } else {
      EvaluatePosition(b, arOutput, r.nPlies, 0, xOnPlay, 0,0,0);

      // for first rolloutCubefull
      diceGen.startSave(r.nRolloutGames);
    }
    
    r.setProbs(arOutput);
    
    r.matchProbNoDouble =
      Equities::equityToProb(*rolloutCubefull(b, 0, r.nRolloutGames, xOnPlay));

    uint& cube = Equities::match.cube;

    Equities::match.set(0, 0, 2*cube, !xOnPlay, -1);
    
    r.matchProbDoubleTake =
      Equities::equityToProb(*rolloutCubefull(b, 0, r.nRolloutGames, xOnPlay));
    
    Equities::match.set(0, 0, cube/2, xOnPlay, -1);

    {
      // away for doubler
      uint const onPlayAway =
	xOnPlay ? Equities::match.xAway : Equities::match.oAway;

      // away for opponent
      uint const opAway =
	xOnPlay ? Equities::match.oAway : Equities::match.xAway;

      r.matchProbDoubleDrop =
	Equities::equityToProb(Equities::value(onPlayAway - cube, opAway));
    }
    
    r.setDecision();
  } else {
    r.analyze(b, xOnPlay, nPlies, 0);

    if( nPlies < nPliesVerify &&
	(r.actionDouble || r.tooGood ||
	 ( (r.matchProbDoubleDrop - r.matchProbDoubleTake)/
	   (r.matchProbDoubleDrop - r.matchProbNoDouble) < 1.2 ) ) ) {
      r.analyze(b, xOnPlay, nPliesVerify, 0);
    }
  }
}


void
Analyze::rollout(PrintMatchBoard const board,
		 bool const            xOnPlay,
		 float                 arOutput[],
		 float                 arStdDev[],
		 uint const            nPlies,
		 uint const            nTruncate,
		 uint const            cGames,
		 RolloutEndsAt const   endsAt)
{
  GNUbgBoard anBoard;

  set(board, anBoard, xOnPlay);

  rollout(anBoard, xOnPlay, arOutput, arStdDev, nPlies, nTruncate, cGames,
	  -1, endsAt);
}


static inline bool
any(Analyze::GNUbgBoard const board, uint const side)
{
  for(uint i = 0; i < 25; ++i) {
    if( board[side][i] != 0 ) {
      return true;
    }
  }
  return false;
}

bool
Analyze::gameOn(Analyze::GNUbgBoard const board)
{
  return any(board, 0) && any(board, 1);
}


void
Analyze::srandom(unsigned long const l)
{
  sgenrand(l);
}

const float*
Analyze::rolloutCubefull(GNUbgBoard  const  board,
			 uint const         nPlies,
			 uint               nGames,
			 bool const         xOnPlay)
{
  static float amcw[1 + 2*6];
  
  MatchState initialMatchState = Equities::match;
  MatchState const& state = Equities::match;

  for(uint k = 0; k < 13; ++k) {
    amcw[k] = 0;
  }
  
  float& outMcw = amcw[0];
  uint const xWinsAtCube = xOnPlay ? 1 : 4;
  uint const xWinsDT = xOnPlay ? 2 : 5;
  uint const xWinsDD = xOnPlay ? 3 : 6;

  uint const oWinsAtCube = xOnPlay ? 4 : 1;
  uint const oWinsDT = xOnPlay ? 5 : 2;;
  uint const oWinsDD =  xOnPlay ? 6 : 3;
  
  int dice[2];

  R1 di;
  di.nPlies = 0;
  
  for(uint ng = 0; ng < nGames; ++ng) {
    GNUbgBoard boardEval;
    memcpy(&boardEval[0][0], &board[0][0], sizeof(boardEval) );

    bool xToPlay = xOnPlay;
    bool first = true;
    
    float mcw = -2;
    uint iMcw = 0;

    if( ng ) {
      diceGen.next();
    }

    while( gameOn(boardEval) ) {
      // Never consider a double on the first move. rollout starts after dice
      // roll. User can set the cube via match state, so we get the right
      // answer for the no-double cases.
      
      if( (state.cube == 1 || (xToPlay == state.xOwns)) && ! first ) {

	di.analyze(boardEval, xToPlay, 0);

	if( di.actionDouble ) {
	  if( di.actionTake ) {
	    setCube(2*state.cube, xToPlay ? false : true);

	    if( iMcw == 0 ) {
	      iMcw = xToPlay ? xWinsDT : oWinsDT;
	    }

	    if( state.cubeDead() ) {
	      break;
	    }
	  } else {
	    if( iMcw == 0 ) {
	      iMcw = xToPlay ? xWinsDD : oWinsDD;
	    }
	    
	    mcw = Equities::value(state.xAway - (xToPlay ? state.cube : 0),
				  state.oAway - (xToPlay ? 0 : state.cube));
	    break;
	  }
	}
      }

      diceGen.get(dice);

      if( first ) {
	if( state.cubeDead() ) {
	  nGames = 1;
	  break;
	}

	first = false;
      }

      findBestMove(0, dice[0], dice[1], boardEval, xToPlay, nPlies);

      xToPlay = !xToPlay;
    
      SwapSides(boardEval);
    }

    if( mcw < -1 ) {
      float p[NUM_OUTPUTS];

      EvaluatePosition(boardEval, p, 2, 0, xToPlay, 0, 0, 0/*fixme*/);

      if( !xToPlay ) {
	InvertEvaluation(p);
      }

      float const xWins = p[WIN];
      float const xWinsGammon = p[WINGAMMON];
      float const oWins = 1 - xWins;
      float const oWinsGammon = p[LOSEGAMMON];

      float const ogr = (oWins > 0) ? oWinsGammon / oWins : 0.0;
      float const xgr = (xWins > 0) ? xWinsGammon / xWins : 0.0;

      float const xBGammons =  p[WINBACKGAMMON];
      float const xbgr = xWinsGammon > 0 ? (xBGammons / xWinsGammon) : 0.0;

      float const oBGammons = p[LOSEBACKGAMMON];
      float const obgr = oWinsGammon > 0 ? (oBGammons / oWinsGammon) : 0.0;
      
      mcw =
	xWins * Equities::eWhenWin(xgr, xbgr,
				   state.xAway, state.oAway, state.cube) +
	oWins * Equities::eWhenLose(ogr, obgr,
				    state.xAway, state.oAway, state.cube);

      if( iMcw == 0 ) {
	iMcw = xToPlay ? oWinsAtCube : xWinsAtCube;
      }
    }

    // mcw accumulated for X, result is for side on play
    
    if( ! xOnPlay ) {
      mcw = -mcw;
    }

    outMcw += mcw;
    amcw[2*iMcw-1] += mcw;
    amcw[2*iMcw] += 1;
    
    setScore(initialMatchState.xAway, initialMatchState.oAway);
    setCube(initialMatchState.cube, initialMatchState.xOwns);
  }

  outMcw /= nGames;
  for(uint k = 1; k < 7; ++k) {
    if( amcw[2*k] != 0 ) {
      amcw[2*k-1] /= amcw[2*k];
      amcw[2*k] /= nGames;
    }
  }
  
  return amcw;
}


Analyze::RolloutEndsAt
Analyze::rolloutTarget(GNUbgBoard const board) const
{
  RolloutEndsAt v = RACE;
  if( isRace(board) ) {
    v = isBearoff(board) ? OVER : BEAROFF;
  }

  return v;
}

static inline bool
gameOver(Analyze::GNUbgBoard const board)
{
  return ! Analyze::gameOn(board);
}

inline bool
rollOver(Analyze::GNUbgBoard const board, Analyze::RolloutEndsAt const target)
{
  switch( target ) {
    case Analyze::OVER:
    {
      if( gameOver(board) ) {
	return true;
      }
      break;
    }
    case Analyze::BEAROFF:
    {
      if( gameOver(board) || isBearoff(board) ) {
	return true;
      }
      break;
    }
    case Analyze::RACE:
    {
      if( isRace(board) ) {
	return true;
      }
      break;
    }
    case Analyze::AUTO: { break; }
  }

  return false;
}
      
  
void
Analyze::rollout(GNUbgBoard const board,
		 bool const       xOnPlay,
		 float            arOutput[],
		 float            arStdDev[],
		 uint const       nPlies,
		 uint const       nTruncate,
		 uint const       nGames,
		 int  const       nSeq,
		 RolloutEndsAt    endsAt)
{
  assert( nGames > 0 );
  
  if( nSeq >= 0 ) {
    if( nSeq == 0 || diceGen.curNseq() != nGames ) {
      diceGen.startSave(nGames);
    } else {
      diceGen.startRetrive();
    }
  } else {
    // don't use
    diceGen.endSave(nGames);
  }
  
  double arVariance[NUM_OUTPUTS];
  for(uint i = 0; i < NUM_OUTPUTS; ++i) {
    arOutput[i] = 0.0;
    arVariance[i] = 0.0;
  }

  if( endsAt == AUTO ) {
    endsAt = rolloutTarget(board);
  }
  
  GNUbgBoard boardEval;
  int dice[2];

  for(uint ng = 0; ng < nGames; ++ng) {
  
    memcpy(&boardEval[0][0], &board[0][0], sizeof(boardEval));

    bool onPlay = xOnPlay;
    uint iTurn = 0;

    if( ng ) {
      diceGen.next();
    }

    //std::cerr << "Dice for game " << ng;

    for(/**/; ! rollOver(boardEval, endsAt) && iTurn < nTruncate; ++iTurn) {

      diceGen.get(dice);
      
      //std::cerr << " (" << dice[0] << dice[1] << ")";

      findBestMove(0, dice[0], dice[1], boardEval, onPlay, nPlies);

      onPlay = !onPlay;
    
      SwapSides(boardEval);
    }
    
    //std::cerr << endl;

    float ar[NUM_OUTPUTS];

    switch( endsAt ) {
      case RACE:
      {
	if( isRace(boardEval) && ! gameOver(boardEval) ) {
	  if( useOSRinRollouts ) {
	    // Make OSR repeatable as well

	    // no nSeq, no diceGen
	    if( nSeq >= 0 ) {
	      Analyze::srandom(diceGen.curSeed()+1);
	    }
	
	    raceProbs(boardEval, ar, 576);

	  } else {
	    // do a 1 ply
	    EvaluatePosition(boardEval, ar, 1, 0, onPlay, 0,0,0/*fixme*/);
	  }
	} else {
	  EvaluatePosition(boardEval, ar, 1, 0, onPlay, 0,0,0/*fixme*/);
	}
	break;
      }
      case BEAROFF:
      case OVER:
      {
	EvaluatePosition(boardEval, ar, 0, 0, onPlay, 0,0,0/*fixme*/);
	break;
      }
      case AUTO: { break; }
    }

    // 	std::cerr << " - " << ar[0] << " " << ar[1] << " "
    //            << ar[2] << " " << ar[3] << " " << ar[4] << endl;
    
    if( iTurn & 1 ) {
      InvertEvaluation(ar);
    }
  
    for(uint i = 0; i < NUM_OUTPUTS; ++i) {
      float const x = min(max(ar[i], 0.0f), 1.0f);
      arOutput[i] += x;
      if( arStdDev ) {
	arVariance[i] += x * x;
      }
    }
  }

  for(uint i = 0; i < NUM_OUTPUTS; ++i) {
    arOutput[i] /= nGames;

    if( arStdDev ) {
      if( nGames == 1 ) {
	arStdDev[i] = 0;
      } else {
	float v = (arVariance[i] / nGames) - arOutput[i] * arOutput[i];
	{                                              assert( v >= -0.001 ); }
	arStdDev[i] = v >= 0 ? sqrt(v) : 0;
      }
    }
  }
}