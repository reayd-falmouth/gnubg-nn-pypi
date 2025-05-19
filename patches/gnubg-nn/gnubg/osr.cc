/*
 * osr.cc
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

#include <cassert>

#include <memory.h>

#include <vector>

typedef unsigned int uint;

template <typename T>
static inline T
min(T const a1, T const a2)
{
  return a1 < a2 ? a1 : a2;
}

template <typename T>
static inline T
max(T const a1, T const a2)
{
  return a1 > a2 ? a1 : a2;
}

#include "br.h"
#include "osr.h"

extern "C" {
#include "positionid.h"
#include "eval.h"
#include "mt19937int.h"
}

//#define DEBUG


// #if defined( LOADED_BO )
// static inline int
// bearx(unsigned short const nOpp, int const i)
// {
//   extern unsigned char* pBearoff1;

//   uint const i1 = (nOpp << 6) | ( i << 1 );
  
//   return pBearoff1[i1] + (pBearoff1[ i1 | 1 ] << 8);
// }
// #endif



// probabilities for bearoff position 'm'
static const B*
bearProbs(uint const m[6])
{
  int x[6];
  for(uint i = 0; i < 6; ++i) {
    x[i] = m[5-i];
  }
  
  unsigned short const nOpp = PositionBearoff(x);

  static B b;
  
  getBearoff(nOpp, &b);
  
  return &b;
}



static inline bool
isCross(uint const from, uint const to)
{
#if defined( DEBUG )
  {                                           assert( from < 24 && to < 24 ); }
#endif
  
  static uint const cross[24] =
  {0,0,0,0,0,0, 1,1,1,1,1,1, 2,2,2,2,2,2, 3,3,3,3,3,3};
  
  return cross[from] != cross[to];
}

#if defined( DEBUG )
static uint
pipc(uint const b[24])
{
  uint p = 0;
  for(uint k = 0; k < 24; ++k) {
    p += b[k] * (24-k);
  }
  return p;
}
#endif


static uint
osr(uint b[24], uint const r1, uint const r2, uint nOut)
{
  uint nRolls = 0;
  
  int dice[2];
  int& d0 = dice[0];
  int& d1 = dice[1];

#if defined( DEBUG )
  int nextPip = -1;
  uint bearOffLoss = 0;
#endif

  uint lc = 0;

  //std::cerr << std::endl;
  
  while( nOut > 0 ) {

    if( nRolls == 0 && r1 > 0 ) {
      d0 = r1;
      d1 = r2;
    } else {
      RollDice(dice);
    }

    if( d0 < d1 ) {
      int const k = d0;
      d0 = d1;
      d1 = k;
    }

    //std::cerr << ' ' << d0 << d1;
    
    ++nRolls;

#if defined( DEBUG )
    {
      for(uint k1 = 0; k1 < lc; ++k1) {
	assert( b[k1] == 0 );
      }
    }

    {
      uint const p = pipc(b);
      
      assert( nextPip == -1 || p == uint(nextPip) );

      nextPip = int(p) - (d0 + d1 + ((d0 == d1) ? 2*d0 : 0));
    }
#endif
    
    if( d0 != d1 ) {
      // see if 2 exactly in

      uint const ifar = 18 - d0;
      uint const inear = 18 - d1;
      
      if( b[ifar] && b[inear] ) {
	--b[ifar];
	--b[inear];
	b[18] += 2;

	nOut -= 2;

	if( lc == ifar && b[ifar] == 0 ) {
	  ++lc;
	}
	
	continue;
      }

      // see if one exactly in

      uint const iboth = 18 - (d0 + d1);
      
      if( b[iboth] ) {
	--b[iboth];

	b[18] += 1;

	nOut -= 1;
	
	if( lc == iboth && b[iboth] == 0 ) {
	  ++lc;
	}
	
	continue;
      }

      for(int nd = 0; nd < 2 && nOut > 0; ++nd) {
	int const d = dice[nd];

	// we are going to use it. flag as used 
	dice[nd] = 0;
      
	if( b[18 - d] ) {
	  --b[18 - d];
	  b[18] += 1;

	  nOut -= 1;
      
	  continue;
	}

	while( lc < 18 && b[lc] == 0 ) {
	  ++lc;
	}

	if( nOut == 1 && nd == 0 && lc+d1 >= 18 && !b[24-d1] && b[24-d] ) {
	  dice[nd] = d;
	  continue;
	}

	// try to make a crosing from the back

	uint k = lc;
	for(/**/; k + d < 18; ++k) {
	  if( b[k] ) {
	    
	    if( isCross(k, k + d) ) {
	      --b[k];

	      if( lc == k && b[k] == 0 ) {
		++lc;
	      }
	      
	      b[k+d]++;
	      break;
	    }
	  }
	}

	if( k + d >= 18 ) {
	  // move checker from rear
	  
	  for(k = lc; k < 18; ++k) {
	    if( b[k] ) {

	      --b[k];
	      
	      if( lc == k && b[k] == 0 ) {
		++lc;
	      }
	      
	      b[k+d]++;
	      
	      if( k+d >= 18 ) {
		--nOut;
	      }
	      break;
	    }
	  }
	}
      }

      // all in, see if a roll is left to bear off
      
      if( nOut == 0 ) {
	int d = max(d0, d1);

	if( d > 0 ) {
	  if( b[24 - d] ) {
	    // bear off
	    
	    --b[24 - d];
	  } else {
	    for(uint k = 18; k + d < 24; ++k) {
	      if( b[k] && (b[k+d] == 0) ) {
		// fill rearest empty place
		
		--b[k];
		++b[k+d];

		d = 0;
		break;
	      }
	    }

	    if( d != 0 ) {
	      for(uint k = 18; k < 24; ++k) {
		if( b[k] ) {
		  // move from rear
		  
		  --b[k];
		  if( k+d < 24 ) {
		    ++b[k+d];
		  } else {
		    // can bear from rear
		    
#if defined( DEBUG )
		    bearOffLoss += k+d - 24;
#endif
		  }
		  break;
		}
	      }
	    }
	  }
	}
      }
    } else {
      uint nd = 4;
      uint const d = dice[0];
      
      while( nd > 0 && nOut > 0 && b[18 - d0] ) {
	--b[18 - d];
	b[18] += 1;

	--nd;
	--nOut;
      }
      
      if( nOut > 0 ) {

	for(int n = nd; n > 1; --n) {
	  int const i = 18 - (n*d);
	  
	  if( i >= 0 && b[i] > 0) {
	    --b[i];
	    nd -= n;
	    b[18] += 1;

	    nOut -= 1;

	    n = nd; // restart loop
	  }
	}
	
	if( nOut > 0 && nd > 0 ) {
	  while( lc < 18 && b[lc] == 0 ) {
	    ++lc;
	  }
	  
	  bool first = true;

	  for(uint k = lc; k < 18; ++k) {
	    if( b[k] ) {
	      if( isCross(k, k + d) && (first || k + d < 18) ) {
		while( b[k] > 0 && nd > 0 && nOut > 0 ) {
		  --b[k];

		  if( lc == k && b[k] == 0 ) {
		    ++lc;
		  }

		  b[k+d]++;
		  
		  if( k+d >= 18 ) {
		    --nOut;
		  }

		  --nd;
		}
	      }
	      
	      if( nOut == 0 || nd == 0 ) {
		break;
	      }
	      
	      // moved all, still first
	      first = b[k] == 0;
	    }
	  }
	  
	  while( nOut > 0 && nd > 0 ) {
	    for(uint k = lc; k < 18; ++k) {
	      if( b[k] ) {
		while( b[k] > 0 && nd > 0 && nOut > 0 ) {
		  --b[k];

		  if( lc == k && b[k] == 0 ) {
		    ++lc;
		  }

		  b[k+d]++;
		  
		  if( k+d >= 18 ) {
		    --nOut;
		  }
		  --nd;
		}

		if( nd == 0 || nOut == 0 ) {
		  break;
		}
	      }
	    }
	  }
	}
      }

      if( nOut == 0 ) {
	while( nd > 0 ) {
	  if( b[24 - d] ) {
	    --b[24 - d];
	    --nd;
	    continue;
	  }

	  if( nd >= 2 && d <= 3 && b[24-2*d] > 0 ) {
	    --b[24-2*d];
	    nd -= 2;
	    continue;
	  }
	  
	  if( nd >= 3 && d <= 2 && b[24-3*d] > 0 ) {
	    --b[24-3*d];
	    nd -= 3;
	    continue;
	  }
	  
	  if( nd >= 4 && d <= 1 && b[24-4*d] > 0 ) {
	    --b[24-4*d];
	    nd -= 4;
	    continue;
	  }
	  
	  bool any = false;
	  for(uint k = 18; nd > 0 && k < 24; ++k) {
	    while( b[k] && nd > 0 ) {
	      any = true;
	      
	      --b[k];

	      --nd;
	      if( k+d < 24 ) {
		++b[k+d];
	      } else {
#if defined( DEBUG )
		bearOffLoss += k+d - 24;
#endif
	      }
	    }
	  }
	  if( ! any ) {
	    // no more!
#if defined( DEBUG )
	    bearOffLoss += nd * d;
#endif
	    nd = 0;
	  }
	}
      }
    }
  }

#if defined( DEBUG )
  {        assert( nextPip == -1 || pipc(b) == uint(nextPip) + bearOffLoss); }
#endif
  
  return nRolls;
}

int osrRepeatable = true;

static void
rollOSR(uint const nRollouts, uint const board[24], uint const out,
	float* const probs, uint const maxProbs,
	float* const gmProbs, uint const maxc)
{
  uint b[24];
  // Dynamically sized buffer instead of VLA:
  std::vector<uint> counts(maxc, 0u);

  for(uint n = 0; n < maxProbs; ++n) {
    probs[n] = 0.0;
  }

  unsigned long rBase = 0;
  if( osrRepeatable ) {
    rBase = genrand();
  }
  
  bool const nonRandFirst = (nRollouts % 36) == 0;
  uint d1 = 0, d2 = 0;
  
  for(uint i = 0; i < nRollouts; ++i) {
    memcpy(b, board, sizeof(b));

    if( nonRandFirst ) {
      d1 = (i % 6)  + 1;
      d2 = ((i / 6) % 6) + 1;
    }

    if( osrRepeatable ) {
      sgenrand(rBase + i);
    }
    
    uint const nr = osr(b, d1, d2, out);

    uint s = 0;
    for(uint k = 18; k < 24; ++k) {
      s += b[k];
    }

    ++counts[min(s == 15 ? nr + 1: nr, maxc-1)];

    B const& p = *bearProbs(b + 18);

    for(uint k = 0; k < p.len; ++k) {
      uint const nrb = p.start + k;

      probs[min(nr + nrb, maxProbs-1)] += p.p[k];
    }
  }

  for(uint n = 0; n < maxProbs; ++n) {
    probs[n] /= nRollouts;
  }

  for(uint n = 0; n < maxc; ++n) {
    // cast to float to avoid implicit double→float conversions
    gmProbs[n] = float(counts[n]) / float(nRollouts);
  }
}
      


static uint const maxProbs = 32;
static uint const maxGprobs = 15;

static uint
osp(int anBoard[25], uint const nGames, uint oboard[24],
    float oprob[maxProbs], float og[maxGprobs])
{
  uint out = 0;
  uint tot = 0;
  
  for(uint k = 0; k < 24; ++k) {
    uint const nc = anBoard[23 - k];
    
    oboard[k] = nc;
    
    if( nc ) {
      tot += nc;
      if( k < 18 ) {
	out += nc;
      }
    }
  }
  
  if( out > 0 ) {
    rollOSR(nGames, oboard, out, oprob, maxProbs, og, maxGprobs);
  } else {
    for(uint io = 0; io < maxGprobs; ++io) {
      og[io] = 0.0;
    }
    
    if( tot == 15 ) {
      og[1] = 1.0;
    } else {
      og[0] = 1.0;
    }

    B const& p = *bearProbs(oboard + 18);

    for(uint k = 0; k < maxProbs; ++k) {
      oprob[k] = 0.0;
    }

    for(uint k = 0; k < p.len; ++k) {
      uint const i = min(p.start + k, maxProbs-1);
      
      oprob[i] += p.p[k];
    }
  }

  return tot;
}

extern "C"
void
raceProbs(int anBoard[2][25], float ar[5], uint nGames)
{
  unsigned long xBase = 0;
  
  if( osrRepeatable ) {
    unsigned long oBase = genrand();
    xBase = genrand();

    sgenrand(oBase);
  }
    
  uint oboard[24];

  float oprob[maxProbs], og[maxGprobs];
  
  uint const toto = osp(anBoard[1], nGames, oboard, oprob, og);
  
  
  if( osrRepeatable ) {
    sgenrand(xBase);
  }
  
  uint xboard[24];
  
  float xprob[maxProbs], xg[maxGprobs];
  
  uint const totx = osp(anBoard[0], nGames, xboard, xprob, xg);
  

  float w = 0.0;

  for(uint io = 0; io < maxProbs; ++io) {
    float s = 0.0;
    for(uint ix = io; ix < maxProbs; ++ix) {
      s += xprob[ix];
    }

    w += oprob[io] * s;
  }

  ar[OUTPUT_WINBACKGAMMON] = 0.0;
  ar[OUTPUT_LOSEBACKGAMMON] = 0.0;
  
  float lg = 0.0;
  
  if( toto == 15 ) {
    for(uint io = 0; io < maxGprobs; ++io) {
      float s = 0.0;

      for(uint ix = 0; ix < io; ++ix) {
	s += xprob[ix];
      }

      lg += og[io] * s;
    }

    if( lg > 0.0 ) {
      float const p = raceBGprob(anBoard, 0);
      if( p > 0.0 ) {
	ar[OUTPUT_LOSEBACKGAMMON] = p;
      }
    }
  }

  float wg = 0.0;
  
  if( totx == 15 ) {
    for(uint ix = 0; ix < maxGprobs; ++ix) {

      float s = 0.0;

      for(uint io = 0; io <= ix; ++io) {
	s += oprob[io];
      }

      wg += xg[ix] * s;
    }

    if( wg > 0.0 ) {
      float const p = raceBGprob(anBoard, 1);
      if( p > 0 ) {
	ar[OUTPUT_WINBACKGAMMON] = p;
      }
    }
  }

  ar[OUTPUT_WIN] = min(w, 1.0F);

  ar[OUTPUT_WINGAMMON] = min(wg, 1.0F);
  if( ar[OUTPUT_WINGAMMON] > ar[OUTPUT_WIN] ) {
    ar[OUTPUT_WINGAMMON] = ar[OUTPUT_WIN];
  }
  
  ar[OUTPUT_LOSEGAMMON] = min(lg, 1.0F);

  float lose = 1-ar[OUTPUT_WIN];
  if( ar[OUTPUT_LOSEGAMMON] > lose ) {
    ar[OUTPUT_LOSEGAMMON] = lose;
  }
}
