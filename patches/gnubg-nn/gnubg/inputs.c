/*
 * inputs.c
 *
 * by Joseph Heled <joseph@gnubg.org>, 2002
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
 * $Id: inputs.c,v 1.7 2018/12/12 19:25:53 plm Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "eval.h"
#include "inputs.h"

static inline int
max(int a, int b)
{
  return a > b ? a : b;
}

/* Race inputs */
enum {
  /* Base is 23 * 4 inputs , since points 24 and 25 are always empty in a
     race */

  /* RI_OFF + i is 1 if i+1 checkers are borne off, 0 otherwise */
  RI_OFF = 92,

  /* Number of crosses by checkers outside home */
  RI_NCROSS = 92 + 14,

  HALF_RACE_INPUTS,

  RI_OPIP = HALF_RACE_INPUTS,
  RI_PIP,

  OLD_HALF_RACE_INPUTS
};

/* new race inputs */
enum {

  RI_NCROSS1 = RI_OFF + 14,
  RI_NCROSS2,
  RI_NCROSS3,

  RI_OPIP1,
  RI_OPIP2,
  RI_OPIP3,
  
  NRI_PIP,
  NRI_WASTAGE,
  NRI_AWASTAGE,

  NEW_HALF_RACE_INPUTS
};


/* Contact inputs -- see Berliner for most of these */
enum {
  I_OFF1 = 0, I_OFF2, I_OFF3,
  
  /* Minimum number of pips required to break contact.

     For each checker x, N(x) is checker location,
     C(x) is max({forall o : N(x) - N(o)}, 0)

     Break Contact : (sum over x of C(x)) / 152

     152 is dgree of contact of start position.
  */
  I_BREAK_CONTACT,

  /* Location of back checker (Normalized to [01])
   */
  I_BACK_CHEQUER,

  /* Location of most backward anchor.  (Normalized to [01])
   */
  I_BACK_ANCHOR,

  /* Forward anchor in opponents home.

     Normalized in the following way:  If there is an anchor in opponents
     home at point k (1 <= k <= 6), value is k/6. Otherwise, if there is an
     anchor in points (7 <= k <= 12), take k/6 as well. Otherwise set to 2.
     
     This is an attempt for some continuity, since a 0 would be the "same" as
     a forward anchor at the bar.
   */
  I_FORWARD_ANCHOR,

  /* Average number of pips opponent loses from hits.
     
     Some heuristics are required to estimate it, since we have no idea what
     the best move actually is.

     1. If board is weak (less than 3 anchors), don't consider hitting on
        points 22 and 23.
     2. Dont break anchors inside home to hit.
   */
  I_PIPLOSS,

  /* Number of rolls that hit at least one checker.
     Same heuristics as PIPLOSS.
   */
  I_P1,

  /* Number of rolls that hit at least two checkers.
     Same heuristics as PIPLOSS.
   */
  I_P2,

  /* How many rolls permit the back checker to escape (Normalized to [01])
     More precisely, how many rolls can be fully utilized by the back checker.
     Uses only 2 of doubles.
   */
  I_BACKESCAPES,

  /* Maximum containment of opponent checkers, from our points 9 to op back 
     checker.
     
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  I_ACONTAIN,
  
  /* Above squared */
  I_ACONTAIN2,

  /* Maximum containment, from our point 9 to home.
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  I_CONTAIN,

  /* Above squared */
  I_CONTAIN2,

  /* Number of builders for the next empty slot in importance.
     Order of importance (in anPoint) is (5, 4, 3, 6, 2, 7).
     Normailed to [01]
  */
  /*I_BUILDERS, */

  /* Next empty slot in importance (see above) is slotted.
     value (6-k)/6 if k is the ordinal number, so 1 for most impotant, 5/6
     for next etc.
  */
  /*I_SLOTTED, */

  /* For all checkers out of home, 
     sum (Number of rolls that let x escape * distance from home)

     Normalized by dividing by 3600.
  */
  I_MOBILITY,

  /* One sided moment.
     Let A be the point of weighted average: 
     A = sum of N(x) for all x) / nCheckers.
     
     Then for all x : A < N(x), M = (average (N(X) - A)^2)

     Diveded by 400 to normalize. 
   */
  I_MOMENT2,

  /* Average number of pips lost when on the bar.
     Normalized to [01]
  */
  I_ENTER,

  /* Probablity of one checker not entering from bar.
     1 - (1 - n/6)^2, where n is number of closed points in op home.
   */
  I_ENTER2,

  /* 1 if last location has an even number of checkers (0,2 ...), 0 otherwise
   */
  /* I_TOP_EVEN, */
  
  /* 1 if total of last 2 location has an even number of checkers (0,2 ...),
     0 otherwise
   */
  /*  I_TOP2_EVEN, */

  I_TIMING,
  
  I_BACKBONE,

  I_BACKG, 
  
  I_BACKG1,
  
  I_FREEPIP,

  I_BACKRESCAPES,
  
  MORE_INPUTS
};

enum {
  I_BURIED = 0,
  
  MORE_INPUTS_E1,
};

enum {
  I_RCONTAIN = 0,
  
  I_CBURIED,
  
  MORE_CRASHED_INPUTS,
};

enum {
  C2_BREAK_CONTACT = 0,

  C2_PIPLOSS,

  C2_P1,
  C2_P2,
  C2_BACKESCAPES,
  C2_BACKRESCAPES,
  C2_MOBILITY,
  C2_ACONTAIN,
  C2_CONTAIN,
  
  C2_RCONTAIN,

  C2_BURIED,
  C2_USELESS,
  C2_ALLE,
  C2_ALLE_LAST = C2_ALLE + 11,
  
  C2_NUM_CRASHED_INPUTS,
};

enum {
  C3_BREAK_CONTACT = 0,

  C3_ENTERLOSS ,
  
  C3_PIPLOSS,

  C3_CONTAIN,
  
  C3_RCONTAIN,

  C3_BURIED,
  
  C3_USELESS,

  C3_NUM_CRASHED_INPUTS
};

enum {
  C4_BURIED,
  C4_USELESS,
  C4_RCONTAIN,
  C4_ALLE,
  C4_ALLE_LAST = C4_ALLE + 6,

  C4_NUM_CRASHED_INPUTS = C4_ALLE_LAST
};

#define NPERPOINT 4

#define NUM_INPUTS ((25 * NPERPOINT + MORE_INPUTS) * 2)
#define NUM_RACE_INPUTS ( HALF_RACE_INPUTS * 2 )

#define NUM_INPUTS_E1  (NUM_INPUTS + (2 *  MORE_INPUTS_E1))
#define NUM_INPUTS_C1  (NUM_INPUTS + (2 *  MORE_CRASHED_INPUTS))
#define NUM_INPUTS_C2  ((25 * NPERPOINT + C2_NUM_CRASHED_INPUTS) * 2)
#define NUM_INPUTS_C3  ((25 * NPERPOINT + C3_NUM_CRASHED_INPUTS) * 2)

#define NUM_INPUTS_C4  ( NUM_INPUTS + 2*C4_NUM_CRASHED_INPUTS )

#define NUM_OLD_RACE_INPUTS ( OLD_HALF_RACE_INPUTS * 2 )

#define NUM_RACE_BASE_INPUTS ( 23 * 4 * 2 )
#define NUM_NEW_RACE_INPUTS ( NEW_HALF_RACE_INPUTS * 2 )



/* Bearoff inputs */
enum {
  BI_OFF = 6*6,

  BI_PIP = BI_OFF + 14,
  
  HALF_BEAROFF_INPUTS
};

#define NUM_BEAROFF_INPUTS (2 * HALF_BEAROFF_INPUTS)

static int anEscapes[ 0x1000 ];



static void
CalculateRace00Inputs(CONST int board[2][25], float inputs[]);
static void 
CalculateRaceInputs(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateInputs(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateMInputs(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateMXInputs(CONST int anBoard[2][25], float inputs[]);
static void
CalculateCrashedInputs(CONST int anBoard[2][25], float arInput[]);
static void
CalculateCrashed1Inputs(CONST int anBoard[2][25], float arInput[]);
static void
CalculateCrashed2Inputs(CONST int anBoard[2][25], float arInput[]);
static void
CalculateCrashed3Inputs(CONST int anBoard[2][25], float arInput[]);
static void
CalculateCrashed4Inputs(CONST int anBoard[2][25], float arInput[]);
static void 
CalculateContactInputs(CONST int anBoard[2][25], float inputs[]);
static void
CalculateContactInputsE1(CONST int anBoard[2][25], float arInput[]);
static void 
CalculateRaceBaseInputs(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateRaceNewInputs(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateRaceInputsOld(CONST int anBoard[2][25], float inputs[]);
static void 
baseInputs250(CONST int anBoard[2][25], float inputs[]);
static void 
CalculateContactInputs201(CONST int anBoard[2][25], float inputs[]);

static CONST char*
std250InputName(unsigned int n);
static CONST char*
raceInputName(unsigned int n);
static CONST char*
oldraceInputName(unsigned int n);
static CONST char*
braceInputName(unsigned int n);
static CONST char*
E1InputName(unsigned int n);


NetInputFuncs
inputFuncs[] =
{
  { "bearoff", CalculateBearoffInputs,  2 * HALF_BEAROFF_INPUTS, 0, 0},
  { "race",    CalculateRaceInputs,     NUM_RACE_INPUTS, raceInputName, 0 } ,
  { "std250",  CalculateInputs,         NUM_INPUTS, std250InputName, 0 } ,
  { "crashed", CalculateCrashedInputs,  NUM_INPUTS, std250InputName, 0 } ,

  { "crashed1", CalculateCrashed1Inputs,  NUM_INPUTS_C1, 0, 0 } ,

  { "crashed2", CalculateCrashed2Inputs,  NUM_INPUTS_C2, 0, 0 } ,
  { "crashed3", CalculateCrashed3Inputs,  NUM_INPUTS_C3, 0, 0 } ,
  { "crashed4", CalculateCrashed4Inputs,  NUM_INPUTS_C4, 0, 0 } ,
  
  { "std250m", CalculateMInputs,  NUM_INPUTS, std250InputName, 0 } ,
  { "std250mx", CalculateMXInputs,  NUM_INPUTS, std250InputName, 0 } ,
  
  { "contact250",
               CalculateContactInputs,  NUM_INPUTS, std250InputName, 0 } ,
  { "raceold", CalculateRaceInputsOld,  NUM_OLD_RACE_INPUTS, oldraceInputName,
  0},
  { "brace",   CalculateRaceBaseInputs, NUM_RACE_BASE_INPUTS, braceInputName,
  0},
  { "nrace",   CalculateRaceNewInputs,  NUM_NEW_RACE_INPUTS, 0, 0 } ,
  { "0race",   CalculateRace00Inputs,   206, 0, 0 },
  { "base200", baseInputs,              2 * 4 * 25, 0, 0} ,
  
  { "base250", baseInputs250,           2 * (6 * 25 + 1) , 0, 0} ,
  
  /* base + piploss */
  { "contact201", CalculateContactInputs201,          (2 * 4 * 25) + 1, 0, 0} ,
  
  { "contactE1", CalculateContactInputsE1,  NUM_INPUTS_E1,  E1InputName, 0} ,
};

const NetInputFuncs*
ifByName(const char* name)
{
  unsigned int i;
  for(i = 0; i < sizeof(inputFuncs)/sizeof(inputFuncs[0]); ++i) {
    NetInputFuncs* p = &inputFuncs[i];
    if( strcmp(name, p->name) == 0 ) {
      return p;
    }
  }
  return 0;
}

static void
ComputeTable0(void)
{
  int i, c, n0, n1;

  for(i = 0; i < 0x1000; ++i) {
    c = 0;
	
    for( n0 = 0; n0 <= 5; n0++ )
      for( n1 = 0; n1 <= n0; n1++ )
	if( !( i & ( 1 << ( n0 + n1 + 1 ) ) ) &&
	    !( ( i & ( 1 << n0 ) ) && ( i & ( 1 << n1 ) ) ) )
	  c += ( n0 == n1 ) ? 1 : 2;
	
    anEscapes[i] = c;
  }
}

static int
Escapes(CONST int anBoard[25], int n)
{
  int i, af = 0;
    
  for( i = 0; i < 12 && i < n; ++i )
    if( anBoard[24 - n + i] > 1 )
      af |= ( 1 << i );
    
  return anEscapes[af];
}


static int anEscapes1[ 0x1000 ];

static void ComputeTable1( void )
{
  int i, c, n0, n1, low;

  anEscapes1[ 0 ] = 0;
  
  for( i = 1; i < 0x1000; i++ ) {
    c = 0;

    low = 0;
    while( ! (i & (1 << low)) ) {
      ++low;
    }
    
    for( n0 = 0; n0 <= 5; n0++ )
      for( n1 = 0; n1 <= n0; n1++ ) {
	
	if( (n0 + n1 + 1 > low) &&
	    !( i & ( 1 << ( n0 + n1 + 1 ) ) ) &&
	    !( ( i & ( 1 << n0 ) ) && ( i & ( 1 << n1 ) ) ) ) {
	  c += ( n0 == n1 ) ? 1 : 2;
	}
      }
	
    anEscapes1[ i ] = c;
  }
}

static int
EscapesR(CONST int anBoard[25], int n)
{
  int i, af = 0;
    
  for( i = 0; i < 12 && i < n; i++ )
    if( anBoard[ 24 - n + i ] > 1 )
      af |= ( 1 << i );
    
  return anEscapes1[ af ];
}


void
ComputeTable(void)
{
  ComputeTable0();
  ComputeTable1();
}



/* aanCombination[n] -
   How many ways to hit from a distance of n pips.
   Each number is an index into aIntermediate below. 
*/
static int CONST
aanCombination[ 24 ][ 5 ] = {
  {  0, -1, -1, -1, -1 }, /*  1 */
  {  1,  2, -1, -1, -1 }, /*  2 */
  {  3,  4,  5, -1, -1 }, /*  3 */
  {  6,  7,  8,  9, -1 }, /*  4 */
  { 10, 11, 12, -1, -1 }, /*  5 */
  { 13, 14, 15, 16, 17 }, /*  6 */
  { 18, 19, 20, -1, -1 }, /*  7 */
  { 21, 22, 23, 24, -1 }, /*  8 */
  { 25, 26, 27, -1, -1 }, /*  9 */
  { 28, 29, -1, -1, -1 }, /* 10 */
  { 30, -1, -1, -1, -1 }, /* 11 */
  { 31, 32, 33, -1, -1 }, /* 12 */
  { -1, -1, -1, -1, -1 }, /* 13 */
  { -1, -1, -1, -1, -1 }, /* 14 */
  { 34, -1, -1, -1, -1 }, /* 15 */
  { 35, -1, -1, -1, -1 }, /* 16 */
  { -1, -1, -1, -1, -1 }, /* 17 */
  { 36, -1, -1, -1, -1 }, /* 18 */
  { -1, -1, -1, -1, -1 }, /* 19 */
  { 37, -1, -1, -1, -1 }, /* 20 */
  { -1, -1, -1, -1, -1 }, /* 21 */
  { -1, -1, -1, -1, -1 }, /* 22 */
  { -1, -1, -1, -1, -1 }, /* 23 */
  { 38, -1, -1, -1, -1 } /* 24 */
};

/* One way to hit */ 
typedef struct {
  /* if true, all intermediate points (if any) are required;
     if false, one of two intermediate points are required.
     Set to true for a direct hit, but that can be checked with
     nFaces == 1,
  */
  int fAll;

  /* Intermediate points required */
  int anIntermediate[ 3 ];

  /* Number of faces used in hit (1 to 4) */
  int nFaces;

  /* Number of pips used to hit */
  int nPips;
}  OneWayToHit;

/* All ways to hit */
static CONST OneWayToHit
aIntermediate[ 39 ] = {
  { 1, { 0, 0, 0 }, 1, 1 }, /*  0: 1x hits 1 */
  { 1, { 0, 0, 0 }, 1, 2 }, /*  1: 2x hits 2 */
  { 1, { 1, 0, 0 }, 2, 2 }, /*  2: 11 hits 2 */
  { 1, { 0, 0, 0 }, 1, 3 }, /*  3: 3x hits 3 */
  { 0, { 1, 2, 0 }, 2, 3 }, /*  4: 21 hits 3 */
  { 1, { 1, 2, 0 }, 3, 3 }, /*  5: 11 hits 3 */
  { 1, { 0, 0, 0 }, 1, 4 }, /*  6: 4x hits 4 */
  { 0, { 1, 3, 0 }, 2, 4 }, /*  7: 31 hits 4 */
  { 1, { 2, 0, 0 }, 2, 4 }, /*  8: 22 hits 4 */
  { 1, { 1, 2, 3 }, 4, 4 }, /*  9: 11 hits 4 */
  { 1, { 0, 0, 0 }, 1, 5 }, /* 10: 5x hits 5 */
  { 0, { 1, 4, 0 }, 2, 5 }, /* 11: 41 hits 5 */
  { 0, { 2, 3, 0 }, 2, 5 }, /* 12: 32 hits 5 */
  { 1, { 0, 0, 0 }, 1, 6 }, /* 13: 6x hits 6 */
  { 0, { 1, 5, 0 }, 2, 6 }, /* 14: 51 hits 6 */
  { 0, { 2, 4, 0 }, 2, 6 }, /* 15: 42 hits 6 */
  { 1, { 3, 0, 0 }, 2, 6 }, /* 16: 33 hits 6 */
  { 1, { 2, 4, 0 }, 3, 6 }, /* 17: 22 hits 6 */
  { 0, { 1, 6, 0 }, 2, 7 }, /* 18: 61 hits 7 */
  { 0, { 2, 5, 0 }, 2, 7 }, /* 19: 52 hits 7 */
  { 0, { 3, 4, 0 }, 2, 7 }, /* 20: 43 hits 7 */
  { 0, { 2, 6, 0 }, 2, 8 }, /* 21: 62 hits 8 */
  { 0, { 3, 5, 0 }, 2, 8 }, /* 22: 53 hits 8 */
  { 1, { 4, 0, 0 }, 2, 8 }, /* 23: 44 hits 8 */
  { 1, { 2, 4, 6 }, 4, 8 }, /* 24: 22 hits 8 */
  { 0, { 3, 6, 0 }, 2, 9 }, /* 25: 63 hits 9 */
  { 0, { 4, 5, 0 }, 2, 9 }, /* 26: 54 hits 9 */
  { 1, { 3, 6, 0 }, 3, 9 }, /* 27: 33 hits 9 */
  { 0, { 4, 6, 0 }, 2, 10 }, /* 28: 64 hits 10 */
  { 1, { 5, 0, 0 }, 2, 10 }, /* 29: 55 hits 10 */
  { 0, { 5, 6, 0 }, 2, 11 }, /* 30: 65 hits 11 */
  { 1, { 6, 0, 0 }, 2, 12 }, /* 31: 66 hits 12 */
  { 1, { 4, 8, 0 }, 3, 12 }, /* 32: 44 hits 12 */
  { 1, { 3, 6, 9 }, 4, 12 }, /* 33: 33 hits 12 */
  { 1, { 5, 10, 0 }, 3, 15 }, /* 34: 55 hits 15 */
  { 1, { 4, 8, 12 }, 4, 16 }, /* 35: 44 hits 16 */
  { 1, { 6, 12, 0 }, 3, 18 }, /* 36: 66 hits 18 */
  { 1, { 5, 10, 15 }, 4, 20 }, /* 37: 55 hits 20 */
  { 1, { 6, 12, 18 }, 4, 24 }  /* 38: 66 hits 24 */
};

/* aaRoll[n] - All ways to hit with the n'th roll
   Each entry is an index into aIntermediate above.
*/
    
static int CONST
aaRoll[ 21 ][ 4 ] = {
  {  0,  2,  5,  9 }, /* 11 */
  {  0,  1,  4, -1 }, /* 21 */
  {  1,  8, 17, 24 }, /* 22 */
  {  0,  3,  7, -1 }, /* 31 */
  {  1,  3, 12, -1 }, /* 32 */
  {  3, 16, 27, 33 }, /* 33 */
  {  0,  6, 11, -1 }, /* 41 */
  {  1,  6, 15, -1 }, /* 42 */
  {  3,  6, 20, -1 }, /* 43 */
  {  6, 23, 32, 35 }, /* 44 */
  {  0, 10, 14, -1 }, /* 51 */
  {  1, 10, 19, -1 }, /* 52 */
  {  3, 10, 22, -1 }, /* 53 */
  {  6, 10, 26, -1 }, /* 54 */
  { 10, 29, 34, 37 }, /* 55 */
  {  0, 13, 18, -1 }, /* 61 */
  {  1, 13, 21, -1 }, /* 62 */
  {  3, 13, 25, -1 }, /* 63 */
  {  6, 13, 28, -1 }, /* 64 */
  { 10, 13, 30, -1 }, /* 65 */
  { 13, 31, 36, 38 }  /* 66 */
};



static void
pipLossP1P2(CONST int* anBoard, CONST int* anBoardOpp,
	    float* piploss, float* p1, float* p2)
{
  int nBoard, i, j, n, k;
  int aHit[39];
  CONST OneWayToHit* pi;

  struct {
    /* count of pips this roll hits */
    int nPips;
      
    /* number of chequers this roll hits */
    int nChequers;
  } aRoll[ 21 ];
    
  /* Piploss */
    
  nBoard = 0;
  for(i = 0; i < 6; ++i) {
    if( anBoard[i] >= 2) {
      nBoard++;
    }
  }

  for(i = 0; i < 39; ++i) {
    aHit[ i ] = 0;
  }
    
  /* for every point we'd consider hitting a blot on, */
    
  for( i = ( nBoard > 2 ) ? 23 : 21; i >= 0; i-- )
    /* if there's a blot there, then */
      
    if( anBoardOpp[ i ] == 1 )
      /* for every point beyond */
	
      for( j = 24 - i; j < 25; j++ )
	/* if we have a hitter and are willing to hit */
	  
	if( anBoard[ j ] && !( j < 6 && anBoard[ j ] == 2 ) )
	  /* for every roll that can hit from that point */
	    
	  for( n = 0; n < 5; n++ ) {
	    if( aanCombination[ j - 24 + i ][ n ] == -1 )
	      break;

	    /* find the intermediate points required to play */
	      
	    pi = aIntermediate + aanCombination[ j - 24 + i ][ n ];

	    if( pi->fAll ) {
	      /* if nFaces is 1, there are no intermediate points */
		
	      if( pi->nFaces > 1 ) {
		/* all the intermediate points are required */
		  
		for( k = 0; k < 3 && pi->anIntermediate[k] > 0; k++ )
		  if( anBoardOpp[ i - pi->anIntermediate[ k ] ] > 1 )
		    /* point is blocked; look for other hits */
		    goto cannot_hit;
	      }
	    } else {
	      /* either of two points are required */
		
	      if( anBoardOpp[ i - pi->anIntermediate[ 0 ] ] > 1
		  && anBoardOpp[ i - pi->anIntermediate[ 1 ] ] > 1 ) {
				/* both are blocked; look for other hits */
		goto cannot_hit;
	      }
	    }
	      
	    /* enter this shot as available */
	      
	    aHit[ aanCombination[ j - 24 + i ][ n ] ] |= 1 << j;
	  cannot_hit: ;
	  }

  for( i = 0; i < 21; i++ )
    aRoll[ i ].nPips = aRoll[ i ].nChequers = 0;
    
  if( !anBoard[ 24 ] ) {
    /* we're not on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = -1; /* (hitter used) */
	
      /* for each way that roll hits, */
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;

	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  for( k = 23; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* select the most advanced blot; if we still have
		 a chequer that can hit there */
		      
	      if( n != k || anBoard[ k ] > 1 )
		aRoll[ i ].nChequers++;

	      n = k;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
		
	      /* if rolling doubles, check for multiple
		 direct shots */
		      
	      if( aaRoll[ i ][ 3 ] >= 0 &&
		  aHit[ r ] & ~( 1 << k ) )
		aRoll[ i ].nChequers++;
			    
	      break;
	    }
	  }
	} else {
	  /* indirect shot */
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;

	  /* find the most advanced hitter */
	    
	  for( k = 23; k >= 0; k-- )
	    if( aHit[ r ] & ( 1 << k ) )
	      break;

	  if( k - pi->nPips + 1 > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = k - pi->nPips + 1;

	  /* check for blots hit on intermediate points */
	  {
	    int l;
	    for( l = 0; l < 3 && pi->anIntermediate[ l ] > 0; l++ ) {
	      if( anBoardOpp[ 23 - k + pi->anIntermediate[ l ] ] == 1 ) {
		
		aRoll[ i ].nChequers++;
		break;
	      }
	    }
	  }
	}
      }
    }
  } else if( anBoard[ 24 ] == 1 ) {
    /* we have one on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = 0; /* (free to use either die to enter) */
	
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;
		
	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  
	  for( k = 24; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* if we need this die to enter, we can't hit elsewhere */
	      
	      if( n && k != 24 )
		break;
			    
	      /* if this isn't a shot from the bar, the
		 other die must be used to enter */
	      
	      if( k != 24 ) {
		int npip = aIntermediate[aaRoll[ i ][ 1 - j ] ].nPips;
		
		if( anBoardOpp[npip - 1] > 1 )
		  break;
				
		n = 1;
	      }

	      aRoll[ i ].nChequers++;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
	    }
	  }
	} else {
	  /* indirect shot -- consider from the bar only */
	  if( !( aHit[ r ] & ( 1 << 24 ) ) )
	    continue;
		    
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;
		    
	  if( 25 - pi->nPips > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = 25 - pi->nPips;
		    
	  /* check for blots hit on intermediate points */
	  for( k = 0; k < 3 && pi->anIntermediate[ k ] > 0; k++ )
	    if( anBoardOpp[ pi->anIntermediate[ k ] + 1 ] == 1 ) {
		
	      aRoll[ i ].nChequers++;
	      break;
	    }
	}
      }
    }
  } else {
    /* we have more than one on the bar --
       count only direct shots from point 24 */
      
    for( i = 0; i < 21; i++ ) {
      /* for the first two ways that hit from the bar */
	
      for( j = 0; j < 2; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( !( aHit[r] & ( 1 << 24 ) ) )
	  continue;

	pi = aIntermediate + r;

	/* only consider direct shots */
	
	if( pi->nFaces != 1 )
	  continue;

	aRoll[ i ].nChequers++;

	if( 25 - pi->nPips > aRoll[ i ].nPips )
	  aRoll[ i ].nPips = 25 - pi->nPips;
      }
    }
  }

  {
    int np = 0;
    int n1 = 0;
    int n2 = 0;
      
    for(i = 0; i < 21; i++) {
      int w = aaRoll[i][3] > 0 ? 1 : 2;
      int nc = aRoll[i].nChequers;
	
      np += aRoll[i].nPips * w;
	
      if( nc > 0 ) {
	n1 += w;

	if( nc > 1 ) {
	  n2 += w;
	}
      }
    }

    if( piploss ) {
      *piploss = np / ( 12.0 * 36.0 );
    }

    if( p1 ) {
      *p1 = n1 / 36.0;
    }
    if( p2 ) {
      *p2 = n2 / 36.0;
    }
  }
}

static void
menOffAll(CONST int* anBoard, float* afInput)
{
  /* Men off */
  int menOff = 15;
  int i;
  
  for(i = 0; i < 25; i++ ) {
    menOff -= anBoard[i];
  }

  if( menOff > 10 ) {
    afInput[ 0 ] = 1.0;
    afInput[ 1 ] = 1.0;
    afInput[ 2 ] = ( menOff - 10 ) / 5.0;
  } else if( menOff > 5 ) {
    afInput[ 0 ] = 1.0;
    afInput[ 1 ] = ( menOff - 5 ) / 5.0;
    afInput[ 2 ] = 0.0;
  } else {
    afInput[ 0 ] = menOff ? menOff / 5.0 : 0.0;
    afInput[ 1 ] = 0.0;
    afInput[ 2 ] = 0.0;
  }
}

static void
menOffNonCrashed(CONST int* anBoard, float* afInput)
{
  int menOff = 15;
  int i;
  
  for(i = 0; i < 25; ++i) {
    menOff -= anBoard[i];
  }
  {                                                   assert( menOff <= 8 ); }
    
  if( menOff > 5 ) {
    afInput[ 0 ] = 1.0;
    afInput[ 1 ] = 1.0;
    afInput[ 2 ] = ( menOff - 6 ) / 3.0;
  } else if( menOff > 2 ) {
    afInput[ 0 ] = 1.0;
    afInput[ 1 ] = ( menOff - 3 ) / 3.0;
    afInput[ 2 ] = 0.0;
  } else {
    afInput[ 0 ] = menOff ? menOff / 3.0 : 0.0;
    afInput[ 1 ] = 0.0;
    afInput[ 2 ] = 0.0;
  }
}

typedef struct {
  /* count of pips this roll hits */
  int nPips;
      
  /* number of chequers this roll hits */
  int nChequers;
} RollInfo;

typedef struct {
  // in my numbering
  int nOppBack;
  int backChequer;
  int backAnchor;
  int nBackAnchors;
  int nBackCheckers;
  int menOff;
  int escapes[25];
  int escapesR[25];

  RollInfo roll[21];
} BoardCache;

static inline void
initBoardTmp(BoardCache* t)
{
  t->nOppBack = -2;
  t->backChequer = -1;
  t->backAnchor = -1;
  t->nBackAnchors = -1;
  t->nBackCheckers = -1;
  t->menOff = -1;

  {
    int i;
    
    for(i = 0; i < 25; ++i) {
      t->escapes[i] = -1;
      t->escapesR[i] = -1;
    }
  }
  
  t->roll[0].nPips = -1;
}

static inline int
menOff(BoardCache* t, CONST int* anBoard)
{
  if( t->menOff < 0 ) {
    /* Men off */
    int m = 15;
    int i;
  
    for(i = 0; i < 25; i++ ) {
      m -= anBoard[i];
    }
    t->menOff = m;
  }
  return t->menOff;
}

static inline int
oppBack(BoardCache* t, CONST int* anBoardOpp)
{
  if( t->nOppBack == -2 ) {
    int nOppBack;

    for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
      if( anBoardOpp[nOppBack] ) {
	break;
      }
    }
    
    t->nOppBack = 23 - nOppBack;
  }
  return t->nOppBack;
}
  
static inline int
backChequer(BoardCache* t, CONST int* anBoard)
{
  if( t->backChequer == -1 ) {
    int nBack;
    
    for( nBack = 24; nBack >= 0; --nBack ) {
      if( anBoard[nBack] ) {
	break;
      }
    }

    t->backChequer = nBack;
  }
  return t->backChequer;
}

static inline int
nBackAnchors(BoardCache* t, CONST int* anBoard)
{
  if( t->nBackAnchors == -1 ) {
    int i;
    t->nBackAnchors = 0;
    t->nBackCheckers = 0;
    
    for( i = 18; i < 24; ++i ) {
      t->nBackCheckers += anBoard[i];
      if( anBoard[i] > 1 ) {
	++ t->nBackAnchors;
      }
    }
  }
  return t->nBackAnchors;
}

static inline int
nBackCheckers(BoardCache* t, CONST int* anBoard)
{
  if( t->nBackCheckers == -1 ) {
    (void)nBackAnchors(t, anBoard);
  }
  return t->nBackCheckers;
}

static inline int
backAnchor(BoardCache* t, CONST int* anBoard)
{
  if( t->backAnchor == -1 ) {
    int i;
    int nBack = backChequer(t, anBoard);
  
    for( i = nBack == 24 ? 23 : nBack; i >= 0; --i ) {
      if( anBoard[i] >= 2 ) {
	break;
      }
    }
    t->backAnchor = i;
  }
  return t->backAnchor;
}

static inline int
escapes(BoardCache* t, CONST int* anBoard, int n)
{
  {                                              assert( 0 <= n && n < 25 ); }
  if( t->escapes[n] == -1 ) {
    t->escapes[n] = Escapes(anBoard, n);
  }
  return t->escapes[n];
}

static inline int
escapesR(BoardCache* t, CONST int* anBoard, int n)
{
  {                                               assert( 0 <= n && n < 25 ); }
  if( t->escapesR[n] == -1 ) {
    t->escapesR[n] = EscapesR(anBoard, n);
  }
  return t->escapesR[n];
}


static void
rollInfo(BoardCache* t, CONST int* anBoard, CONST int* anBoardOpp)
{
  if( t->roll[0].nPips >= 0 ) {
    return;
  } else {
  int nBoard, i, j, n, k;
  int aHit[39];
  CONST OneWayToHit* pi;

  RollInfo* aRoll = t->roll;
    
  nBoard = 0;
  for(i = 0; i < 6; ++i) {
    if( anBoard[i] > 1 ) {
      nBoard++;
    }
  }

  for(i = 0; i < 39; ++i) {
    aHit[ i ] = 0;
  }
    
  /* for every point we'd consider hitting a blot on, */
    
  for( i = ( nBoard > 2 ) ? 23 : 21; i >= 0; i-- )
    /* if there's a blot there, then */
      
    if( anBoardOpp[i] == 1 )
      /* for every point beyond */
	
      for( j = 24 - i; j < 25; j++ )
	/* if we have a hitter and are willing to hit */
	  
	if( anBoard[ j ] && !( j < 6 && anBoard[ j ] == 2 ) )
	  /* for every roll that can hit from that point */
	    
	  for( n = 0; n < 5; n++ ) {
	    if( aanCombination[ j - 24 + i ][ n ] == -1 )
	      break;

	    /* find the intermediate points required to play */
	      
	    pi = aIntermediate + aanCombination[ j - 24 + i ][ n ];

	    if( pi->fAll ) {
	      /* if nFaces is 1, there are no intermediate points */
		
	      if( pi->nFaces > 1 ) {
		/* all the intermediate points are required */
		  
		for( k = 0; k < 3 && pi->anIntermediate[k] > 0; k++ )
		  if( anBoardOpp[ i - pi->anIntermediate[ k ] ] > 1 )
		    /* point is blocked; look for other hits */
		    goto cannot_hit;
	      }
	    } else {
	      /* either of two points are required */
		
	      if( anBoardOpp[ i - pi->anIntermediate[ 0 ] ] > 1
		  && anBoardOpp[ i - pi->anIntermediate[ 1 ] ] > 1 ) {
				/* both are blocked; look for other hits */
		goto cannot_hit;
	      }
	    }
	      
	    /* enter this shot as available */
	      
	    aHit[ aanCombination[ j - 24 + i ][ n ] ] |= 1 << j;
	  cannot_hit: ;
	  }

  for( i = 0; i < 21; i++ )
    aRoll[i].nPips = aRoll[i].nChequers = 0;
    
  if( !anBoard[ 24 ] ) {
    /* we're not on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = -1; /* (hitter used) */
	
      /* for each way that roll hits, */
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;

	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  for( k = 23; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* select the most advanced blot; if we still have
		 a chequer that can hit there */
		      
	      if( n != k || anBoard[ k ] > 1 )
		aRoll[ i ].nChequers++;

	      n = k;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
		
	      /* if rolling doubles, check for multiple
		 direct shots */
		      
	      if( aaRoll[ i ][ 3 ] >= 0 &&
		  aHit[ r ] & ~( 1 << k ) )
		aRoll[ i ].nChequers++;
			    
	      break;
	    }
	  }
	} else {
	  /* indirect shot */
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;

	  /* find the most advanced hitter */
	    
	  for( k = 23; k >= 0; k-- )
	    if( aHit[ r ] & ( 1 << k ) )
	      break;

	  if( k - pi->nPips + 1 > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = k - pi->nPips + 1;

	  /* check for blots hit on intermediate points */
	  {
	    int l;
	    for( l = 0; l < 3 && pi->anIntermediate[ l ] > 0; l++ ) {
	      if( anBoardOpp[ 23 - k + pi->anIntermediate[ l ] ] == 1 ) {
		
		aRoll[ i ].nChequers++;
		break;
	      }
	    }
	  }
	}
      }
    }
  } else if( anBoard[ 24 ] == 1 ) {
    /* we have one on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = 0; /* (free to use either die to enter) */
	
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;
		
	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  
	  for( k = 24; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* if we need this die to enter, we can't hit elsewhere */
	      
	      if( n && k != 24 )
		break;
			    
	      /* if this isn't a shot from the bar, the
		 other die must be used to enter */
	      
	      if( k != 24 ) {
		int npip = aIntermediate[aaRoll[ i ][ 1 - j ] ].nPips;
		
		if( anBoardOpp[npip - 1] > 1 )
		  break;
				
		n = 1;
	      }

	      aRoll[ i ].nChequers++;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
	    }
	  }
	} else {
	  /* indirect shot -- consider from the bar only */
	  if( !( aHit[ r ] & ( 1 << 24 ) ) )
	    continue;
		    
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;
		    
	  if( 25 - pi->nPips > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = 25 - pi->nPips;
		    
	  /* check for blots hit on intermediate points */
	  for( k = 0; k < 3 && pi->anIntermediate[ k ] > 0; k++ )
	    if( anBoardOpp[ pi->anIntermediate[ k ] + 1 ] == 1 ) {
		
	      aRoll[ i ].nChequers++;
	      break;
	    }
	}
      }
    }
  } else {
    /* we have more than one on the bar --
       count only direct shots from point 24 */
      
    for( i = 0; i < 21; i++ ) {
      /* for the first two ways that hit from the bar */
	
      for( j = 0; j < 2; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( !( aHit[r] & ( 1 << 24 ) ) )
	  continue;

	pi = aIntermediate + r;

	/* only consider direct shots */
	
	if( pi->nFaces != 1 )
	  continue;

	aRoll[ i ].nChequers++;

	if( 25 - pi->nPips > aRoll[ i ].nPips )
	  aRoll[ i ].nPips = 25 - pi->nPips;
      }
    }
  }
  }
}

static void
menOff0(CONST int* anBoard, BoardCache* t, float* inp)
{
  int m = menOff(t, anBoard);

  if( m == 0 ) {
    *inp = 0.0;
  } else {
    if( m > 2 ) {
      *inp = 1.0;
    } else {
      *inp = m / 3.0;
    }
  }
}

static void
menOff1(CONST int* anBoard, BoardCache* t, float* inp)
{
  int m = menOff(t, anBoard);

  if( m <= 2 ) {
    *inp = 0.0;
  } else {
    if( m > 5 ) {
      *inp = 1.0;
    } else {
      *inp = (m-3) / 3.0;
    }
  }
}

static void
menOff2(CONST int* anBoard, BoardCache* t, float* inp)
{
  int m = menOff(t, anBoard);
  {                                                         assert( m <= 8 ); }
  if( m <= 5 ) {
    *inp = 0.0;
  } else {
    *inp = (m-6) / 2.0;
  }
}

static void
breakContact(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	     float* inp, int scale)
{
  int i;
  int n = 0;
  int nOppBack = oppBack(t, anBoardOpp);
  
  for( i = nOppBack + 1; i < 25; i++ )
    if( anBoard[i] )
      n += ( i + 1 - nOppBack ) * anBoard[ i ];

  {                                                              assert( n ); }

  switch( scale ) {
    case 0: *inp = n;                break;
    case 1: *inp = n / (15 + 152.0); break;
    case 2: *inp = (n - 2)/ 354.0;   break;
  }
}

static void
sBreakContact(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	      float* inp, int scale)
{
  int i;
  int n = 0;
  int nOppBack = oppBack(t, anBoardOpp);
  
  for( i = nOppBack + 1; i < 25; i++ )
    if( anBoard[i] )
      n += ( i + 1 - nOppBack );

  {                                                              assert( n ); }

  switch( scale ) {
    case 0: *inp = n;           break;
    case 1: *inp = n;           break;
    case 2: *inp = n / 100.0;   break;
  }
}


static void
freePip(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	float* inp, int scale)
{
  unsigned int p = 0;
  int nOppBack = oppBack(t, anBoardOpp);

  {
    int i;
    for(i = 0; i < nOppBack; ++i) {
      if( anBoard[i] ) {
	p += (i+1) * anBoard[i];
      }
    }
  }
  
  switch( scale ) {
    case 0: *inp = p;         break;
    case 1: *inp = p / 100.0; break;
    case 2: *inp = p / 236.0;   break;
  }
}

static void
timing(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* bt, float* inp,
       int scale)
{
  int i;
  int t = 0;
  int no = 0;
  int nOppBack = oppBack(bt, anBoardOpp);
      
  t += 24 * anBoard[24];
  no += anBoard[24];
      
  for( i = 23;  i >= 12 && i > nOppBack; --i ) {
    if( anBoard[i] && anBoard[i] != 2 ) {
      int n = ((anBoard[i] > 2) ? (anBoard[i] - 2) : 1);
      no += n;
      t += i * n;
    }
  }

  for( ; i >= 6; --i ) {
    if( anBoard[i] ) {
      int n = anBoard[i];
      no += n;
      t += i * n;
    }
  }
    
  for( i = 5;  i >= 0; --i ) {
    if( anBoard[i] > 2 ) {
      t += i * (anBoard[i] - 2);
      no += (anBoard[i] - 2);
    } else if( anBoard[i] < 2 ) {
      int n = (2 - anBoard[i]);

      if( no >= n ) {
	t -= i * n;
	no -= n;
      }
    }
  }

  if( t < 0 ) {
    t = 0;
  }

  switch( scale ) {
    case 0: *inp = t;         break;
    case 1: *inp = t / 100.0; break;
    case 2: *inp = t / 262.0; break;
  }
}

static inline void
iBackChequer(CONST int* anBoard, BoardCache* bt, float* inp)
{
  *inp = backChequer(bt, anBoard) / 24.0;
}

static void
iBackAnchor(CONST int* anBoard, BoardCache* bt, float* inp)
{
  *inp = backAnchor(bt, anBoard) / 24.0;
}

static void
iForwardAnchor(CONST int* anBoard, BoardCache* t, float* inp)
{
  int j;
  int i = backAnchor(t, anBoard);

  int n = 0;

  for( j = 18; j <= i; ++j ) {
    if( anBoard[j] >= 2 ) {
      n = 24 - j;
      break;
    }
  }

  if( n == 0 ) {
    for( j = 17; j >= 12 ; --j ) {
      if( anBoard[j] >= 2 ) {
	n = 24 - j;
	break;
      }
    }
  }
	
  *inp = (n == 0) ? 2.0 : n / 6.0;
}

static void
iMoment2(CONST int* anBoard, float* inp, int scale)
{
  int j = 0;
  int n = 0;
  int i, k;
  
  for(i = 0; i < 25; i++ ) {
    int ni = anBoard[i];
      
    if( ni ) {
      j += ni;
      n += i * ni;
    }
  }

  if( j ) {
    n = (n + j - 1) / j;
  }

  j = 0;
  for(k = 0, i = n + 1; i < 25; i++ ) {
    int ni = anBoard[ i ];

    if( ni ) {
      j += ni;
      k += ni * ( i - n ) * ( i - n );
    }
  }

  if( j ) {
    k = (k + j - 1) / j;
  }

  switch( scale ) {
    case 0: *inp = k;         break;
    case 1: *inp = k / 400.0; break;
    case 2: *inp = k / 441.0; break;
  }
}

static int
enterLoss(CONST int* anBoardOpp, int two)
{
  int i, j;
    
  int loss = 0;
      
  for(i = 0; i < 6; ++i) {
    if( anBoardOpp[i] > 1 ) {
      /* any double loses */
	  
      loss += 4*(i+1);

      for(j = i+1; j < 6; ++j) {
	if( anBoardOpp[ j ] > 1 ) {
	  loss += 2*(i+j+2);
	} else {
	  if( two ) {
	    loss += 2*(i+1);
	  }
	}
      }
    } else {
      if( two ) {
	for(j = i+1; j < 6; ++j) {
	  if( anBoardOpp[ j ] > 1 ) {
	    loss += 2*(j+1);
	  }
	}
      }
    }
  }
  return loss;
}

static void
iEnter(CONST int* anBoard, CONST int* anBoardOpp, float* inp)
{
  if( anBoard[24] > 0 ) {
    int loss = enterLoss(anBoardOpp, anBoard[24] > 1);
      
    *inp = loss / (36.0 * (49.0/6.0));
  } else {
    *inp = 0.0;
  }
}

static void
iEnterLoss(CONST int* anBoardOpp, float* inp)
{
  int loss = enterLoss(anBoardOpp, 0);
      
  *inp = loss / (36.0 * (49.0/6.0));
}

static void
iEnter2incorrect(CONST int* anBoardOpp, float* inp)
{
  int n = 0;
  int i;
  
  for(i = 0; i < 6; i++ ) {
    n += anBoardOpp[ i ] > 1;
  }
    
  *inp = ( 36 - ( n - 6 ) * ( n - 6 ) ) / 36.0;
}

static void
iEnter2(CONST int* anBoardOpp, float* inp)
{
  int i;
  int n = 0;
  
  for(i = 0; i < 6; i++ ) {
    n += anBoardOpp[ i ] > 1;
  }
    
  *inp = (n * n) / 36.0;
}

static void
iBackbone(CONST int* anBoard, float* inp)
{
  int pa = -1;
  int w = 0;
  int tot = 0;
  int np;
    
  for(np = 23; np > 0; --np) {
    if( anBoard[np] >= 2 ) {
      if( pa == -1 ) {
	pa = np;
	continue;
      }

      {
	int d = pa - np;
	int c = 0;
	
	if( d <= 6 ) {
	  c = 11;
	} else if( d <= 11 ) {
	  c = 13 - d;
	}

	w += c * anBoard[pa];
	tot += anBoard[pa];
      }
    }
  }

  if( tot ) {
    *inp = 1 - (w / (tot * 11.0));
  } else {
    *inp = 0;
  }
}

static void
iBackEscapes(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	     float* inp)
{
  int nOppBack = oppBack(t, anBoardOpp);
  
  *inp = escapes(t, anBoard, 23 - nOppBack) / 36.0;
}

static void
iBackEscapesR(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	     float* inp)
{
  int nOppBack = oppBack(t, anBoardOpp);
  
  *inp = escapesR(t, anBoard, 23 - nOppBack ) / 36.0;
}

static void
iAContain(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	 float* inp)
{
  int n = 36;
  int nOppBack = oppBack(t, anBoardOpp);
  int i, j;
  
  for(i = 15; i < 24 - nOppBack; i++ )
    if( ( j = escapes(t, anBoard, i ) ) < n )
      n = j;

  *inp = ( 36 - n ) / 36.0;
}

static void
iContain(CONST int* anBoard, BoardCache* t, float* inp)
{
  int n = 36;
  int i;
    
  for(i = 15; i < 24; ++i ) {
    int j = escapes(t, anBoard, i );
    if( j < n ) {
      n = j;
    }
  }
    
  *inp = ( 36 - n ) / 36.0;
}

static void
iMobility(CONST int* anBoard, CONST int* anBoardOpp, float* inp, int scale)
{
  int n = 0;
  int i;
  
  for(i = 6; i < 25; ++i ) {
    if( anBoard[ i ] ) {
      n += (i - 5) * anBoard[i] * Escapes(anBoardOpp, i);
    }
  }


  switch( scale ) {
    case 1: *inp = n / 3600.0; break;
    case 2: *inp = n / 8280.0; break;
  }
}

static void
iRMobility(CONST int* anBoard, CONST int* anBoardOpp, float* inp, int scale)
{
  int n = 0;
  int i;
  
  for(i = 6; i < 25; ++i ) {
    if( anBoard[ i ] ) {
      n += (i - 5) * anBoard[i] * EscapesR(anBoardOpp, i);
    }
  }


  switch( scale ) {
    case 1: *inp = n / 3600.0; break;
    case 2: *inp = n / 8280.0; break;
  }
}

static void
iBackG(CONST int* anBoard, BoardCache* t, float* inp, int scale)
{
  unsigned int nAc = nBackAnchors(t, anBoard);
  
  if( nAc > 1 ) {
    unsigned int tot = nBackCheckers(t, anBoard) + anBoard[24];
    
    switch( scale ) {
      case 0: *inp = tot - 3;   break;
      case 1: *inp = (tot-3) / 4.0; break;
      case 2: *inp = (tot-3) / 12.0; break;
    }
  } else {
    *inp = 0;
  }
}

static void
iBackG1(CONST int* anBoard, BoardCache* t, float* inp, int scale)
{
  unsigned int nAc = nBackAnchors(t, anBoard);
  
  if( nAc == 1 ) {
    unsigned int tot = nBackCheckers(t, anBoard) + anBoard[24];
    switch( scale ) {
      case 0: *inp = tot;        break;
      case 1: *inp = tot / 8.0;  break;
      case 2: *inp = tot / 15.0; break;
    }
  } else {
    *inp = 0;
  }
}

static void
iBuried(CONST int* anBoard, float* inp)
{
  CONST int* b = anBoard;

  unsigned int buried = b[0] > 2 ? b[0] - 2 : 0;
  int missing = b[0] < 2 ? 2 - b[0] : 0;
  int i;
    
  for(i = 1; i < 6; ++i) {
    int bi = b[i];
    if( bi < 2 ) {
      missing += 2 - bi;
    } else {
      if ( bi > missing  + 2 ) {
	buried +=  bi - (missing  + 2);
	missing = 0;
      } else {
	missing -= (bi - 2);
	assert( missing >= 0 );
      }
    }
  }

  *inp = buried / 10.0;
}

static void
iUseless(CONST int* anBoard, float* inp)
{
  int i;
  *inp = 0.0;
  for(i = 6; i < 24; ++i) {
    int n = anBoard[i];
    if( n > 3 ) {
      *inp += (n - 3) * (n - 3);
    }
  }
  *inp /= 20.0;
}
  

static void
iPipLoss(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
	float* piploss, int scale)
{
  int np = 0;
  int i;
  
  rollInfo(t, anBoard, anBoardOpp);
      
  for(i = 0; i < 21; i++) {
    int w = aaRoll[i][3] > 0 ? 1 : 2;
	
    np += t->roll[i].nPips * w;
  }

  switch( scale ) {
    case 0: *piploss = np;                 break;
    case 1: *piploss = np / (12.0 * 36.0); break;
    case 2: *piploss = np / 809.0;         break;
  }
}

static void
iP1(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
    float* p1)
{
  int n1 = 0;
  int i;
  
  rollInfo(t, anBoard, anBoardOpp);
      
  for(i = 0; i < 21; i++) {
    int w = aaRoll[i][3] > 0 ? 1 : 2;
    int nc = t->roll[i].nChequers;
	
    if( nc > 0 ) {
      n1 += w;
    }
  }

  *p1 =  n1 / 36.0;
}

static void
iP2(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
    float* p2)
{
  int n2 = 0;
  int i;
  
  rollInfo(t, anBoard, anBoardOpp);
      
  for(i = 0; i < 21; i++) {
    int w = aaRoll[i][3] > 0 ? 1 : 2;
    int nc = t->roll[i].nChequers;
	
    if( nc > 1 ) {
      n2 += w;
    }
  }

  *p2 =  n2 / 36.0;
}

#if 0
static void
iEscapesContainment(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
		    float* v)
{
  int i, j;
    
  int pipCount0 = 0;
  int pipCount1 = 0;

  *v = 0.0;
  
  for( i = 0; i < 25; ++i) {
    if( (j = anBoardOpp[i]) ) {
      pipCount0 += j * (i+1);
    }
    if( (j = anBoard[i]) ) {
      pipCount1 += j * (i+1);
    }
  }

  pipCount1 = max(pipCount1 - 8, 0);

  if( 10 * pipCount1 > 9 * pipCount0 ) {
    /* not behind */
    return;
  }

  {
    int myBack = backChequer(t, anBoard);
    int opBack = oppBack(t, anBoardOpp);
    int c = 0;

    {                                     assert( anBoardOpp[23 - opBack] ); }
    
    for(i = 23 - opBack; i >= 25 - myBack; --i) {
      c += anBoardOpp[i];
      if( c >= 6 ) {
	i = myBack;
	break;
      }
    }

    /* magic - 6 or more checkers needed to contain */
    if( c >= 6 ) {
      int af = 0;
      int avg = 0;
      int n = 0;
      int m = 36;
	
      for(j = i; j >= 25 - myBack; --j) {
	int p = anBoardOpp[j];
	if( p <= 1 ) {
	  int f;
	  if( af == 0 ) {
	    if( i - j >= 12 ) {
	      f = 3;
	    } else {
	      f = (int[]){36,36,36,35,31,27,23,17,12,8,6,4} [i-j];
	    }
	  } else {
	    f = anEscapes1[af];
	  }
	  if( f > m ) {
	    f = m;
	  }
	  avg += f * (i-j);
	  m = f;
	}
	n += (i-j);
	  
	af <<= 1;
	if( p > 1 ) {
	  af |= 1;
	}
	af &= 0xfff;
      }
      /* avg == 0 ??
   0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 3, 0
      */
      assert(! (avg > 0 && n == 0) );
      if( avg > 0 ) {
	*v = 1 - (((1.0/36.0) * avg) / n);
      }
    }
  }
}
#endif

static int
escapes1(CONST int* anBoardOpp, int from, int* escapesByPoint)
{
  int e = escapesByPoint[from];
  if( e < 0 ) {
    int af = 0;
    int k;
    for(k = from; k < from + 12 && k < 24; ++k) {
      if( anBoardOpp[k] > 1 ) {
	af |= ( 1 << (k-from) );
      }
    }
    e = anEscapes1[af];
    escapesByPoint[from] = e;
  }
  return e;
}

static void
iEscapesContainment(CONST int* anBoard, CONST int* anBoardOpp, BoardCache* t,
		    float* v)
{
  int i;
    
  int pipCount0 = 0;
  int pipCount1 = 0;

  *v = 0.0;
  
  for( i = 0; i < 25; ++i) {
    int j;
    
    if( (j = anBoardOpp[i]) ) {
      pipCount0 += j * (i+1);
    }
    if( (j = anBoard[i]) ) {
      pipCount1 += j * (i+1);
    }
  }

  pipCount1 = max(pipCount1 - 8, 0);

  if( 10 * pipCount1 > 9 * pipCount0 ) {
    /* not behind */
    return;
  }

  {
    int myBack = backChequer(t, anBoard);
    int opBack = oppBack(t, anBoardOpp);
    int c = 0;

    {                                     assert( anBoardOpp[23 - opBack] ); }
    
    for(i = 23 - opBack; i >= 25 - myBack; --i) {
      c += anBoardOpp[i];
      if( c >= 6 )
	break;
    }

    /* magic - 6 or more checkers needed to contain */
    if( c >= 6 ) {
      int j;
      int escapesByPoint[25];
      for(j = 0; j < 25; ++j) {
	escapesByPoint[j] = -1;
      }
      
      for(j = myBack; j > opBack; --j) {
	if( anBoard[j] ) {
/* 	  int e = 0; */
/* 	  if( 0 && j <= 12 ) { */
/* 	    e = escapes1(anBoardOpp, j, escapesByPoint); */
/* 	  } else { */
	  
	    int firstAnchor;
	    for(firstAnchor = 24 - j; firstAnchor < 24; ++firstAnchor) {
	      if( anBoardOpp[firstAnchor] > 1 ) {
		break;
	      }
	    }

	    if( firstAnchor < 24 ) {
	      int e = escapes1(anBoardOpp, firstAnchor, escapesByPoint);
	      *v += (anBoard[j]/3.0) * (36 - e) / 36.0;
	    }
	  }
/* 	  if( e > 0 ) { */
/* 	    *v += (anBoard[j]/3.0) * (36 - e) / 36.0; */
/* 	  } */
/* 	} */
      }
    }
  }
}

/* Calculates the inputs for one player only. */

static void
CalculateHalfInputs(CONST int anBoard[25], CONST int anBoardOpp[25],
		    float afInput[])
{
  BoardCache t;
  initBoardTmp(&t);

  menOffAll(anBoard, &afInput[ I_OFF1 ]);

  breakContact(anBoard, anBoardOpp, &t, &afInput[I_BREAK_CONTACT], 1);
  
  freePip(anBoard, anBoardOpp, &t, &afInput[I_FREEPIP], 1);

  timing(anBoard, anBoardOpp, &t, &afInput[I_TIMING], 1);
  
  iBackChequer(anBoard, &t, &afInput[ I_BACK_CHEQUER ]);

  iBackAnchor(anBoard, &t, &afInput[ I_BACK_ANCHOR ]);

  iForwardAnchor(anBoard, &t, &afInput[ I_FORWARD_ANCHOR ]);
    
  pipLossP1P2(anBoard, anBoardOpp, 
	      & afInput[ I_PIPLOSS ], & afInput[ I_P1 ], & afInput[ I_P2 ] );
  
  iBackEscapes(anBoard, anBoardOpp, &t, &afInput[ I_BACKESCAPES ]);
  iBackEscapesR(anBoard, anBoardOpp, &t, &afInput[ I_BACKRESCAPES ]);
  

  iAContain(anBoard, anBoardOpp, &t, &afInput[ I_ACONTAIN ]);
  afInput[ I_ACONTAIN2 ] = afInput[ I_ACONTAIN ] * afInput[ I_ACONTAIN ];

  iContain(anBoard, &t, &afInput[ I_CONTAIN ]);
  afInput[ I_CONTAIN2 ] = afInput[ I_CONTAIN ] * afInput[ I_CONTAIN ];
  
  iMobility(anBoard, anBoardOpp, &afInput[ I_MOBILITY ], 1);

  iMoment2(anBoard, &afInput[ I_MOMENT2 ], 1);

  iEnter(anBoard, anBoardOpp, &afInput[ I_ENTER ]);

  iEnter2incorrect(anBoardOpp, &afInput[ I_ENTER2 ]);

  iBackbone(anBoard, & afInput[I_BACKBONE]);

  iBackG(anBoard, &t, & afInput[I_BACKG], 1);
  iBackG1(anBoard, &t, & afInput[I_BACKG1], 1);
}

/* Calculates the inputs for one player only. */

static void
CalculateCrashedHalfInputs(CONST int anBoard[25], CONST int anBoardOpp[25],
			   float afInput[])
{
  int i, j, k, nOppBack, n;
    
  menOffAll(anBoard, &afInput[ I_OFF1 ]);
  
  for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
    if( anBoardOpp[nOppBack] ) {
      break;
    }
  }
    
  nOppBack = 23 - nOppBack;

  n = 0;
  for( i = nOppBack + 1; i < 25; i++ )
    if( anBoard[ i ] )
      n += ( i + 1 - nOppBack ) * anBoard[ i ];

  {                                                              assert( n ); }
    
  afInput[ I_BREAK_CONTACT ] = n / (15 + 152.0);

  {
    unsigned int p  = 0;
    
    for( i = 0; i < nOppBack; i++ ) {
      if( anBoard[i] )
	p += (i+1) * anBoard[i];
    }
    
    afInput[I_FREEPIP] = p / 100.0;
  }

  {
    int t = 0;
    
    int no = 0;
      
    t += 24 * anBoard[24];
    no += anBoard[24];
      
    for( i = 23;  i >= 12 && i > nOppBack; --i ) {
      if( anBoard[i] && anBoard[i] != 2 ) {
	int n = ((anBoard[i] > 2) ? (anBoard[i] - 2) : 1);
	no += n;
	t += i * n;
      }
    }

    for( ; i >= 6; --i ) {
      if( anBoard[i] ) {
	int n = anBoard[i];
	no += n;
	t += i * n;
      }
    }
    
    for( i = 5;  i >= 0; --i ) {
      if( anBoard[i] > 2 ) {
	t += i * (anBoard[i] - 2);
	no += (anBoard[i] - 2);
      } else if( anBoard[i] < 2 ) {
	int n = (2 - anBoard[i]);

	if( no >= n ) {
	  t -= i * n;
	  no -= n;
	}
      }
    }

    afInput[ I_TIMING ] = t / 100.0;
  }

  /* Back chequer */

  {
    int nBack;
    
    for( nBack = 24; nBack >= 0; --nBack ) {
      if( anBoard[nBack] ) {
	break;
      }
    }
    
    afInput[ I_BACK_CHEQUER ] = nBack / 24.0;

    /* Back anchor */

    for( i = nBack == 24 ? 23 : nBack; i >= 0; --i ) {
      if( anBoard[i] >= 2 ) {
	break;
      }
    }
    
    afInput[ I_BACK_ANCHOR ] = i / 24.0;
    
    /* Forward anchor */

    n = 0;
    for( j = 18; j <= i; ++j ) {
      if( anBoard[j] >= 2 ) {
	n = 24 - j;
	break;
      }
    }

    if( n == 0 ) {
      for( j = 17; j >= 12 ; --j ) {
	if( anBoard[j] >= 2 ) {
	  n = 24 - j;
	  break;
	}
      }
    }
	
    afInput[ I_FORWARD_ANCHOR ] = n == 0 ? 2.0 : n / 6.0;
  }
    
  pipLossP1P2(anBoard, anBoardOpp, 
	      & afInput[ I_PIPLOSS ], & afInput[ I_P1 ],
	      & afInput[ I_P2 ] );
  
  afInput[ I_BACKESCAPES ] = Escapes( anBoard, 23 - nOppBack ) / 36.0;

  afInput[ I_BACKRESCAPES ] = EscapesR( anBoard, 23 - nOppBack ) / 36.0;
  
  for( n = 36, i = 15; i < 24 - nOppBack; i++ )
    if( ( j = EscapesR( anBoard, i ) ) < n )
      n = j;

  afInput[ I_ACONTAIN ] = ( 36 - n ) / 36.0;
  afInput[ I_ACONTAIN2 ] = afInput[ I_ACONTAIN ] * afInput[ I_ACONTAIN ];

  if( nOppBack < 0 ) {
    /* restart loop, point 24 should not be included */
    i = 15;
    n = 36;
  }
    
  for( ; i < 24; i++ )
    if( ( j = EscapesR( anBoard, i ) ) < n )
      n = j;

    
  afInput[ I_CONTAIN ] = ( 36 - n ) / 36.0;
  afInput[ I_CONTAIN2 ] = afInput[ I_CONTAIN ] * afInput[ I_CONTAIN ];
    
  for( n = 0, i = 6; i < 25; i++ )
    if( anBoard[ i ] )
      n += ( i - 5 ) * anBoard[ i ] * Escapes( anBoardOpp, i );

  afInput[ I_MOBILITY ] = n / 3600.00;

  j = 0;
  n = 0; 
  for(i = 0; i < 25; i++ ) {
    int ni = anBoard[ i ];
      
    if( ni ) {
      j += ni;
      n += i * ni;
    }
  }

  if( j ) {
    n = (n + j - 1) / j;
  }

  j = 0;
  for(k = 0, i = n + 1; i < 25; i++ ) {
    int ni = anBoard[ i ];

    if( ni ) {
      j += ni;
      k += ni * ( i - n ) * ( i - n );
    }
  }

  if( j ) {
    k = (k + j - 1) / j;
  }

  afInput[ I_MOMENT2 ] = k / 400.0;

  if( anBoard[ 24 ] > 0 ) {
    int loss = 0;
    int two = anBoard[ 24 ] > 1;
      
    for(i = 0; i < 6; ++i) {
      if( anBoardOpp[ i ] > 1 ) {
	/* any double loses */
	  
	loss += 4*(i+1);

	for(j = i+1; j < 6; ++j) {
	  if( anBoardOpp[ j ] > 1 ) {
	    loss += 2*(i+j+2);
	  } else {
	    if( two ) {
	      loss += 2*(i+1);
	    }
	  }
	}
      } else {
	if( two ) {
	  for(j = i+1; j < 6; ++j) {
	    if( anBoardOpp[ j ] > 1 ) {
	      loss += 2*(j+1);
	    }
	  }
	}
      }
    }
      
    afInput[ I_ENTER ] = loss / (36.0 * (49.0/6.0));
  } else {
    afInput[ I_ENTER ] = 0.0;
  }

  n = 0;
  for(i = 0; i < 6; i++ ) {
    n += anBoardOpp[ i ] > 1;
  }
    
  afInput[ I_ENTER2 ] = ( 36 - ( n - 6 ) * ( n - 6 ) ) / 36.0; 

  {
    int pa = -1;
    int w = 0;
    int tot = 0;
    int np;
    
    for(np = 23; np > 0; --np) {
      if( anBoard[np] >= 2 ) {
	if( pa == -1 ) {
	  pa = np;
	  continue;
	}

	{
	  int d = pa - np;
	  int c = 0;
	
	  if( d <= 6 ) {
	    c = 11;
	  } else if( d <= 11 ) {
	    c = 13 - d;
	  }

	  w += c * anBoard[pa];
	  tot += anBoard[pa];
	}
      }
    }

    if( tot ) {
      afInput[I_BACKBONE] = 1 - (w / (tot * 11.0));
    } else {
      afInput[I_BACKBONE] = 0;
    }
  }

  {
    unsigned int nAc = 0;
    
    for( i = 18; i < 24; ++i ) {
      if( anBoard[i] > 1 ) {
	++nAc;
      }
    }
    
    afInput[I_BACKG] = 0.0;
    afInput[I_BACKG1] = 0.0;

    if( nAc >= 1 ) {
      unsigned int tot = 0;
      for( i = 18; i < 25; ++i ) {
	tot += anBoard[i];
      }

      if( nAc > 1 ) {
	/* assert( tot >= 4 ); */
      
	afInput[I_BACKG] = (tot - 3) / 4.0;
      } else if( nAc == 1 ) {
	afInput[I_BACKG1] = tot / 8.0;
      }
    }
  }
	
    
}

static void
CalculateRace00Inputs(CONST int board[2][25], float input[])
{
  unsigned int side;
  int i, n;
  int OFF1 = 24 * 4 + 3 + 1;
  int OFF2 = OFF1 + 1;
  int OFF3 = OFF2 + 1;
  
  for(side = 0; side < 2; ++side) {
    const int* CONST anBoard = board[side];
#define TARGET(i) input[ (2 * (i)) + (side == 1 ? 0 : 1) ]
    
    for( i = 0; i < 24; i++ ) {
	TARGET( i * 4 + 0 ) = anBoard[ i ] == 1;
	TARGET( i * 4 + 1 ) = anBoard[ i ] >= 2;
	TARGET( i * 4 + 2 ) = anBoard[ i ] >= 3;
	TARGET( i * 4 + 3 ) = anBoard[ i ] > 3 ?
	    ( anBoard[ i ] - 3 ) / 2.0 : 0.0;
    }

    /* Bar */
    TARGET( 24 * 4 + 0 ) = anBoard[ 24 ] >= 1;
    TARGET( 24 * 4 + 1 ) = anBoard[ 24 ] >= 2;
    TARGET( 24 * 4 + 2 ) = anBoard[ 24 ] >= 3;
    TARGET( 24 * 4 + 3 ) = anBoard[ 24 ] > 3 ?
	( anBoard[ 24 ] - 3 ) / 2.0 : 0.0;


    /* Men off */
    for( n = 15, i = 0; i < 25; i++ )
	n -= anBoard[ i ];

    if( n > 10 ) {
	TARGET( OFF1 ) = 1.0;
	TARGET( OFF2 ) = 1.0;
	TARGET( OFF3 ) = ( n - 10 ) / 5.0;
    } else if( n > 5 ) {
	TARGET( OFF1 ) = 1.0;
	TARGET( OFF2 ) = ( n - 5 ) / 5.0;
	TARGET( OFF3 ) = 0.0;
    } else {
	TARGET( OFF1 ) = n / 5.0;
	TARGET( OFF2 ) = 0.0;
	TARGET( OFF3 ) = 0.0;
    }
  }
#undef TARGET
}


static void 
CalculateRaceBaseInputs(CONST int anBoard[2][25], float inputs[])
{
  unsigned int side;
  
  for(side = 0; side < 2; ++side) {
    const int* CONST board = anBoard[side];
    float* CONST afInput = inputs + side * (NUM_RACE_BASE_INPUTS / 2);
    int i;
    
    {                                              assert( board[24] == 0 ); }
    {                                              assert( board[23] == 0 ); }
    
    /* Points */
    for(i = 0; i < 23; ++i) {
      unsigned int CONST nc = board[ i ];

      unsigned int k = i * 4;
      
      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc == 2;
      afInput[ k++ ] = nc >= 3;
      afInput[ k ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }
  }
}

static void 
CalculateRaceNewInputs(CONST int anBoard[2][25], float inputs[])
{
  unsigned int side;
  
  for(side = 0; side < 2; ++side) {
    const int* CONST board = anBoard[side];
    float* CONST afInput = inputs + side * NEW_HALF_RACE_INPUTS;
    int i;
    int menOff = 15;
    
    {                                              assert( board[24] == 0 ); }
    {                                              assert( board[23] == 0 ); }
    
    /* Points */
    for(i = 0; i < 23; ++i) {
      unsigned int const nc = board[i];
      unsigned int k = i * 4;

      if( nc ) {
	afInput[ k++ ] = nc == 1;
	afInput[ k++ ] = nc == 2;
	afInput[ k++ ] = nc >= 3;
	afInput[ k ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
	
	menOff -= nc;
      } else {
	afInput[ k++ ] = 0;
	afInput[ k++ ] = 0;
	afInput[ k++ ] = 0;
	afInput[ k ] = 0;
      }
    }

    /* Men off */
    {
      int k;
      for(k = 0; k < 14; ++k) {
	afInput[ RI_OFF + k ] = menOff > k ? 1.0 : 0.0;
      }
    }

    {
      unsigned int k;
      unsigned int i;
      unsigned int totNp = 0;
      
      unsigned int nCross = 0;
      unsigned int np = 0;
      
      for(k = 1; k < 4; ++k) {
	for(i = 6*k; i < 6*k + 6; ++i) {
	  unsigned int const nc = board[i];

	  if( nc ) {
	    nCross += nc * k;

	    np += nc * (i+1);
	  }
	}
	
	afInput[RI_NCROSS1 + (k - 1)] = nCross / 10.0;
	afInput[RI_OPIP1 + (k - 1)] = np / 50.0;

	totNp += np;
      }

      {
	unsigned int wastage = 0;
	unsigned int totIn = 0;
      
	for(i = 0; i < 6; ++i) {
	  unsigned int const nc = board[i];

	  if( nc ) {
	    totIn += nc;
	  
	    totNp += nc * (i+1);

	    wastage += (5 - i) * nc;
	  }
	}
      
	afInput[NRI_PIP] = totNp / 100.0;
	afInput[NRI_AWASTAGE] = totIn > 0 ? (wastage / (2.5 * totIn)) : 0.0;
	afInput[NRI_WASTAGE] = wastage / 20.0;
      }
    }
  }
}


static void 
CalculateRaceInputsOld(CONST int anBoard[2][25], float inputs[])
{
  unsigned int side;
  
  for(side = 0; side < 2; ++side) {
    const int* const board = anBoard[side];
    float* const afInput = inputs + side * OLD_HALF_RACE_INPUTS;
    int i;
    
    {                             assert( board[23] == 0 && board[24] == 0 ); }
    
    /* Points */
    for(i = 0; i < 23; ++i) {
      unsigned int const nc = board[ i ];

      unsigned int k = i * 4;
      
      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc >= 2;
      afInput[ k++ ] = nc >= 3;
      afInput[ k ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }

    /* Men off */
    {
      int menOff = 15;
      int i;
      
      for(i = 0; i < 25; ++i) {
	menOff -= board[i];
      }

      {
	int k;
	for(k = 0; k < 14; ++k) {
	  afInput[ RI_OFF + k ] = menOff > k ? 1.0 : 0.0;
	}
      }
    }

    {
      unsigned int nCross = 0;
      unsigned int np = 0;
      unsigned int k;
      unsigned int i;
      
      for(k = 1; k < 4; ++k) {
      
	for(i = 6*k; i < 6*k + 6; ++i) {
	  unsigned int const nc = board[i];

	  if( nc ) {
	    nCross += nc * k;

	    np += nc * (i+1);
	  }
	}
      }
      
      afInput[RI_NCROSS] = nCross / 10.0;
      
      afInput[RI_OPIP] = np / 50.0;

      for(i = 0; i < 6; ++i) {
	unsigned int const nc = board[i];

	if( nc ) {
	  np += nc * (i+1);
	}
      }
      
      afInput[RI_PIP] = np / 100.0;
    }
  }
}

static void 
CalculateRaceInputs(CONST int anBoard[2][25], float inputs[])
{
  unsigned int side;
  
  for(side = 0; side < 2; ++side) {
    unsigned int i, k;

    const int* const board = anBoard[side];
    float* const afInput = inputs + side * HALF_RACE_INPUTS;

    unsigned int menOff = 15;
    
    {                             assert( board[23] == 0 && board[24] == 0 ); }
    
    /* Points */
    for(i = 0; i < 23; ++i) {
      unsigned int const nc = board[i];

      k = i * 4;

      menOff -= nc;
      
      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc == 2;
      afInput[ k++ ] = nc >= 3;
      afInput[ k ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }

    /* Men off */
    for(k = 0; k < 14; ++k) {
      afInput[ RI_OFF + k ] = (menOff == (k+1)) ? 1.0 : 0.0;
    }
    
    {
      unsigned int nCross = 0;
      
      for(k = 1; k < 4; ++k) {
	for(i = 6*k; i < 6*k + 6; ++i) {
	  unsigned int const nc = board[i];

	  if( nc ) {
	    nCross += nc * k;
	  }
	}
      }
      
      afInput[RI_NCROSS] = nCross / 10.0;
    }
  }
}


void
baseInputs(CONST int anBoard[2][25], float arInput[])
{
  int j, i;
    
  for(j = 0; j < 2; ++j ) {
    float* afInput = arInput + j * 25*4;
    const int* board = anBoard[j];
    
    /* Points */
    for( i = 0; i < 24; i++ ) {
      int nc = board[ i ];
      
      afInput[ i * 4 + 0 ] = nc == 1;
      afInput[ i * 4 + 1 ] = nc == 2;
      afInput[ i * 4 + 2 ] = nc >= 3;
      afInput[ i * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }

    /* Bar */
    {
      int nc = board[ 24 ];
      
      afInput[ 24 * 4 + 0 ] = nc >= 1;
      afInput[ 24 * 4 + 1 ] = nc >= 2; /**/
      afInput[ 24 * 4 + 2 ] = nc >= 3;
      afInput[ 24 * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }
  }
}

static void
mbaseInputs(CONST int anBoard[2][25], float arInput[])
{
  int j, i;
    
  for(j = 0; j < 2; ++j ) {
    float* afInput = arInput + j * 25*4;
    const int* board = anBoard[j];
    
    /* Points */
    for( i = 0; i < 24; i++ ) {
      int nc = board[ i ];
      
      afInput[ i * 4 + 0 ] = nc == 1;
      afInput[ i * 4 + 1 ] = nc == 2;
      afInput[ i * 4 + 2 ] = nc >= 3;
      afInput[ i * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 6.0 : 0.0;
    }

    /* Bar */
    {
      int nc = board[ 24 ];
      
      afInput[ 24 * 4 + 0 ] = nc >= 1;
      afInput[ 24 * 4 + 1 ] = nc >= 2; /**/
      afInput[ 24 * 4 + 2 ] = nc >= 3;
      afInput[ 24 * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 6.0 : 0.0;
    }
  }
}

static void
mxbaseInputs(CONST int anBoard[2][25], float arInput[])
{
  int j, i;
    
  for(j = 0; j < 2; ++j ) {
    float* afInput = arInput + j * 25*4;
    const int* board = anBoard[j];
    
    /* Points */
    for( i = 0; i < 24; i++ ) {
      int nc = board[ i ];
      
      afInput[ i * 4 + 0 ] = nc == 1;
      afInput[ i * 4 + 1 ] = nc == 2;
      afInput[ i * 4 + 2 ] = nc >= 3;
      afInput[ i * 4 + 3 ] = nc <= 3 ? 0.0 :
	(nc <= 7 ? ( nc - 3 ) / 8.0 : (0.5 + (nc-7)/16.0));
    }

    /* Bar */
    {
      int nc = board[ 24 ];
      
      afInput[ 24 * 4 + 0 ] = nc >= 1;
      afInput[ 24 * 4 + 1 ] = nc >= 2; /**/
      afInput[ 24 * 4 + 2 ] = nc >= 3;
      afInput[ 24 * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 6.0 : 0.0;
    }
  }
}


static void
baseInputs250(CONST int anBoard[2][25], float arInput[])
{
  int s;
    
  for(s = 0; s < 2; ++s ) {
    float* afInput = arInput + s * 25*6 + 1;
    const int* board = anBoard[s];
    int p = 0;
    int i;
    int k = 0;
    
    /* Points */
    for(i = 0; i < 25; ++i) {
      int nc = board[i];
      
      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc == 2;
      afInput[ k++ ] = nc == 3;
      afInput[ k++ ] = nc == 4;
      afInput[ k++ ] = nc == 5;
      afInput[ k++ ] = nc >= 6;

      if( nc > 6 ) {
	p += (nc - 6) * (i+1);
      }
    }
    afInput[k] = p;
  }
}

static void
CalculateContactInputs201(CONST int board[2][25], float arInput[])
{
  baseInputs(board, arInput);

  pipLossP1P2(board[1], board[0], & arInput[ 200 ], 0, 0);
}

/* Calculates neural net inputs from the board position.
   Returns 0 for contact positions, 1 for races. */

static void
CalculateInputs(CONST int anBoard[2][25], float arInput[])
{
  baseInputs(anBoard, arInput);
  
  CalculateHalfInputs(anBoard[1], anBoard[0], arInput + 4 * 25 * 2);

  CalculateHalfInputs(anBoard[0], anBoard[1],
		      arInput + (4 * 25 * 2 + MORE_INPUTS));
}

static void
CalculateMInputs(CONST int anBoard[2][25], float arInput[])
{
  mbaseInputs(anBoard, arInput);
  
  CalculateHalfInputs(anBoard[1], anBoard[0], arInput + 4 * 25 * 2);

  CalculateHalfInputs(anBoard[0], anBoard[1],
		      arInput + (4 * 25 * 2 + MORE_INPUTS));
}

static void
CalculateMXInputs(CONST int anBoard[2][25], float arInput[])
{
  mxbaseInputs(anBoard, arInput);
  
  CalculateHalfInputs(anBoard[1], anBoard[0], arInput + 4 * 25 * 2);

  CalculateHalfInputs(anBoard[0], anBoard[1],
		      arInput + (4 * 25 * 2 + MORE_INPUTS));
}

#if 0 
static void
rContain(CONST int anBoard[2][25], int toPlayBack, int oppBack,
	 float* toPlay, float* op)
{
  int i, j;
  /* side that need to contain */
  int side = -1;
    
  int pipCount0 = 0;
  int pipCount1 = 0;

/*   afInput[ I_ACONTAIN2 ] = 0.0; */
/*   afInput[ MORE_INPUTS + I_ACONTAIN2 ] = 0.0; */
  *toPlay = *op = 0.0;
  
  for( i = 0; i < 25; ++i) {
    if( (j = anBoard[0][i]) ) {
      pipCount0 += 2 * j * (i+1);
      if( i < 5 ) {
	pipCount0 += j * (5-i);
      }
    }
    if( (j = anBoard[1][i]) ) {
      pipCount1 += 2 * j * (i+1);
      if( i < 5 ) {
	pipCount1 += j * (5-i);
      }
    }
  }
  pipCount0 /= 2;
  pipCount1 /= 2;
  
  pipCount1 = max(pipCount1 - 8, 0);

  if( 9 * pipCount1 >  10 * pipCount0 ) {
    /* I am behind */
    side = 1;
  } else if ( 9 * pipCount0 > 10 * pipCount1 ) {
    side = 0;
  }

  if( side != -1 ) {
    int myBack = side ? toPlayBack : oppBack;
    int opBack = side ? oppBack : toPlayBack;
    int c = 0;
      
    for(i = myBack ; i >= 24 - opBack; --i) {
      c += anBoard[side][i];
      if( c >= 6 ) {
	/* op from i to home */
	for(i = 22 - i; i >= 0; --i) {
	  if( anBoard[1-side][i] > 1 ) {
	    break;
	  }
	}
	/* works also when loop finds none */
	i = 22 - i;
	if( i > myBack ) {
	  i = myBack;
	}
	break;
      }
    }

    /* magic - 6 or more checkers needed to contain */
    if( c >= 6 ) {
      int af = 0;
      int avg = 0;
      int n = 0;
      int m = 36;
	
      for(j = i; j >= 24 - opBack; --j) {
	int p = anBoard[side][j];
	if( p <= 1 ) {
	  int f;
	  if( af == 0 ) {
	    if( i - j >= 12 ) {
	      f = 3;
	    } else {
	      f = (int[]){36,36,36,35,31,27,23,17,12,8,6,4} [i-j];
	    }
	  } else {
	    f = anEscapes1[af];
	  }
	  if( f > m ) {
	    f = m;
	  }
	  avg += f * (i-j);
	  m = f;
	}
	n += (i-j);
	  
	af <<= 1;
	if( p > 1 ) {
	  af |= 1;
	}
	af &= 0xfff;
      }
      /* avg == 0 ??
     0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 3, 0
      */
      assert(! (avg > 0 && n == 0) );
      if( avg > 0 ) {
	float v = 1 - (((1.0/36.0) * avg) / n);
	if( side ) {
	  *toPlay = v;
	} else {
	  *op = v;
	}
      }
    }
  }
}

#endif

static void
CalculateCrashedInputs(CONST int anBoard[2][25], float arInput[])
{
  float* afInput = arInput + 4 * 25 * 2;
  
  baseInputs(anBoard, arInput);
  
  CalculateCrashedHalfInputs(anBoard[1], anBoard[0], afInput);

  CalculateCrashedHalfInputs(anBoard[0], anBoard[1], afInput + MORE_INPUTS);

  {
    int i, j;
    /* side that need to contain */
    int side = -1;
    
    int pipCount0 = 0;
    int pipCount1 = 0;

    afInput[ I_ACONTAIN2 ] = 0.0;
    afInput[ MORE_INPUTS + I_ACONTAIN2 ] = 0.0;
    
    for( i = 0; i < 25; ++i) {
      if( (j = anBoard[0][i]) ) {
	pipCount0 += j * (i+1);
      }
      if( (j = anBoard[1][i]) ) {
	pipCount1 += j * (i+1);
      }
    }

    pipCount1 = max(pipCount1 - 8, 0);

    if( 9 * pipCount1 >  10 * pipCount0 ) {
      /* I am behind */
      side = 1;
    } else if ( 9 * pipCount0 > 10 * pipCount1 ) {
      side = 0;
    }

    if( side != -1 ) {
      int myBack =
	0.5 + 24 * afInput[ (side ? 0 : MORE_INPUTS) + I_BACK_CHEQUER ];
      int opBack =
	0.5 + 24 * afInput[ (side ? MORE_INPUTS : 0) + I_BACK_CHEQUER ];
      int c = 0;
      
      for(i = myBack ; i >= 24 - opBack; --i) {
	c += anBoard[side][i];
	if( c >= 6 ) {
	  /* op from i to home */
	  for(i = 22 - i; i >= 0; --i) {
	    if( anBoard[1-side][i] > 1 ) {
	      break;
	    }
	  }
	  /* works also when loop finds none */
	  i = 22 - i;
	  if( i > myBack ) {
	    i = myBack;
	  }
	  break;
	}
      }

      /* magic - 6 or more checkers needed to contain */
      if( c >= 6 ) {
	int af = 0;
	int avg = 0;
	int n = 0;
	int m = 36;
	
	for(j = i; j >= 24 - opBack; --j) {
	  int p = anBoard[side][j];
	  if( p <= 1 ) {
	    int f;
	    if( af == 0 ) {
	      if( i - j >= 12 ) {
		f = 3;
	      } else {
		f = (int[]){36,36,36,35,31,27,23,17,12,8,6,4} [i-j];
	      }
	    } else {
	      f = anEscapes1[af];
	    }
	    if( f > m ) {
	      f = m;
	    }
	    avg += f * (i-j);
	    m = f;
	  }
	  n += (i-j);
	  
	  af <<= 1;
	  if( p > 1 ) {
	    af |= 1;
	  }
	  af &= 0xfff;
	}
	/* avg == 0 ??
   0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 3, 0
	*/
	assert(! (avg > 0 && n == 0) );
	if( avg > 0 ) {
	  afInput[ (side ? 0 : MORE_INPUTS) + I_ACONTAIN2 ] =
	    1 - (((1.0/36.0) * avg) / n);
	}
      }
    }
  }
}

static void
CalculateCrashed1Inputs(CONST int anBoard[2][25], float arInput[])
{
  float* afInput = arInput + 4 * 25 * 2;

  CalculateInputs(anBoard, arInput);

  {
    float* i = afInput + 2*MORE_INPUTS;
    BoardCache t;
    initBoardTmp(&t);

    iEscapesContainment(anBoard[1], anBoard[0], &t, i + I_RCONTAIN);
    iBuried(anBoard[1], i + I_CBURIED);
  
    initBoardTmp(&t);
    i += MORE_CRASHED_INPUTS;
    
    iEscapesContainment(anBoard[0], anBoard[1], &t, i + I_RCONTAIN);
    iBuried(anBoard[0], i + I_CBURIED);
  }
  
#if 0 
  afInput[ I_P2 ] = 0.0;
  afInput[ MORE_INPUTS + I_P2 ] = 0.0;
  
  int toPlayBack = 0.5 + 24 * afInput[ I_BACK_CHEQUER ];
  int opBack = 0.5 + 24 * afInput[ MORE_INPUTS + I_BACK_CHEQUER ];

  rContain(anBoard, toPlayBack, opBack,
	   &afInput[2*MORE_INPUTS + I_RCONTAIN],
	   &afInput[2*MORE_INPUTS + I_RCONTAIN + 1]);
#endif
}


static void
CalculateCrashed2Inputs(CONST int anBoard[2][25], float arInput[])
{
  mbaseInputs(anBoard, arInput);

  {
    float* i = arInput + 4 * 25 * 2;
    BoardCache t;
    unsigned int side;
    
    for(side = 0; side < 2; ++side) {
      CONST int* bpl = anBoard[1 - side];
      CONST int* bop = anBoard[side];
    
      initBoardTmp(&t);
    
      breakContact(bpl, bop, &t, i + C2_BREAK_CONTACT, 1);
      iPipLoss(bpl, bop, &t, i + C2_PIPLOSS, 1);
      iP1(bpl, bop, &t, i + C2_P1);
      iP2(bpl, bop, &t, i + C2_P2);

      iBackEscapes(bpl, bop, &t, i + C2_BACKESCAPES);
      iBackEscapesR(bpl, bop, &t, i + C2_BACKRESCAPES);
      iMobility(bpl, bop, i + C2_MOBILITY, 1);

      iAContain(bpl, bop, &t, i + C2_ACONTAIN);
      iContain(bpl, &t, i + C2_CONTAIN);
    
      iEscapesContainment(bpl, bop, &t, i + C2_RCONTAIN);
      iBuried(bpl, i + C2_BURIED);
      iUseless(bpl, i + C2_USELESS);

      {
	int af = 0;
	int j;
	for(j = 6; j < 18; ++j) {
	  if( bop[j] > 1 ) {
	    af |= ( 1 << (j-6) );
	  }
	}
	
	for(j = 6; j < 18; ++j) {
	  i[C2_ALLE + (j-6)] = anEscapes1[af] / 36.0;
	  af >>= 1;
	  if( j+12 < 24 && bop[j + 12] > 1 ) {
	    af |= (1 << 11);
	  } else {
	    af &= 0x7ff;
	  }
	}
      }
    
      i += C2_NUM_CRASHED_INPUTS;
    }
  }
}


static void
CalculateCrashed3Inputs(CONST int anBoard[2][25], float arInput[])
{

  mbaseInputs(anBoard, arInput);

  {
    float* i = arInput + 4 * 25 * 2;
    CONST int* bpl = anBoard[1];
    CONST int* bop = anBoard[0];
    
    BoardCache t;
    initBoardTmp(&t);
    
    breakContact(bpl, bop, &t, i + C3_BREAK_CONTACT, 1);
    iPipLoss(bpl, bop, &t, i + C3_PIPLOSS, 1);
    iEnterLoss(bop, i + C3_ENTERLOSS);
    iContain(bpl, &t, i + C3_CONTAIN);
    
    iEscapesContainment(bpl, bop, &t, i + C3_RCONTAIN);
    iBuried(bpl, i + C3_BURIED);
    iUseless(bpl, i + C3_USELESS);
  
    initBoardTmp(&t);
    i += C3_NUM_CRASHED_INPUTS;
    
    breakContact(bop, bpl, &t, i + C3_BREAK_CONTACT, 1);
    iPipLoss(bop, bpl, &t, i + C3_PIPLOSS, 1);
    iEnterLoss(bpl, i + C3_ENTERLOSS);
    
    iContain(bop, &t, i + C3_CONTAIN);
    
    iEscapesContainment(bop, bpl, &t, i + C3_RCONTAIN);
    iBuried(bop, i + C3_BURIED);
    iUseless(bop, i + C3_USELESS);
  }
}

static void
CalculateCrashed4Inputs(CONST int anBoard[2][25], float arInput[])
{
  baseInputs(anBoard, arInput);
  
  CalculateHalfInputs(anBoard[1], anBoard[0], arInput + 4 * 25 * 2);

  CalculateHalfInputs(anBoard[0], anBoard[1],
		      arInput + (4 * 25 * 2 + MORE_INPUTS));

  {
    float* i = arInput + 4 * 25 * 2 + MORE_INPUTS*2;
    BoardCache t;
    unsigned int side;
    
    for(side = 0; side < 2; ++side, i += C4_NUM_CRASHED_INPUTS) {
      CONST int* bpl = anBoard[1 - side];
      CONST int* bop = anBoard[side];
    
      initBoardTmp(&t);

      iEscapesContainment(bpl, bop, &t, i + C4_RCONTAIN);
      iBuried(bpl, i + C4_BURIED);
      iUseless(bpl, i + C4_USELESS);

      {
	int af = 0;
	int j;
	for(j = 6+6; j < 18+6; ++j) {
	  if( bop[j] > 1 ) {
	    af |= ( 1 << (j-12) );
	  }
	}
	
	for(j = 12; j < 18; ++j) {
	  i[C4_ALLE + (j-12)] = anEscapes1[af] / 36.0;
	  af >>= 1;
	  if( j+12 < 24 && bop[j + 12] > 1 ) {
	    af |= (1 << 11);
	  } else {
	    af &= 0x7ff;
	  }
	}
      }
    }
  }
}

static void
CalculateContactInputs(CONST int anBoard[2][25], float arInput[])
{
  int s;
  
  CalculateInputs(anBoard, arInput);

  for(s = 0; s < 2; ++s) {
    float* afInput = arInput + (4 * 25 * 2) + (s ? MORE_INPUTS : 0);
    menOffNonCrashed(anBoard[s], afInput + I_OFF1);
  }
}

static void
CalculateContactInputsE1(CONST int anBoard[2][25], float arInput[])
{
  int s;
  
  CalculateContactInputs(anBoard, arInput);

  for(s = 0; s < 2; ++s) {
    float* afInput =
      arInput + NUM_INPUTS + (s ? MORE_INPUTS_E1 : 0);

    CONST int* b = anBoard[s];

    unsigned int buried = b[0] > 2 ? b[0] - 2 : 0;
    int missing = b[0] < 2 ? 2 - b[0] : 0;
    int i;
    
    for(i = 1; i < 6; ++i) {
      int bi = b[i];
      if( bi < 2 ) {
	missing += 2 - bi;
      } else {
	if ( bi > missing  + 2 ) {
	  buried +=  bi - (missing  + 2);
	  missing = 0;
	} else {
	  missing -= (bi - 2);
	  assert( missing >= 0 );
	}
      }
    }

    afInput[I_BURIED] = buried / 5.0;
  }
}



void 
CalculateBearoffInputs(CONST int anBoard[2][25], float inputs[])
{
  unsigned int side, i, k;
  
  for(side = 0; side < 2; ++side) {
    const int* const board = anBoard[side];
    float* const afInput = inputs + side * NUM_BEAROFF_INPUTS / 2;

    for(i = 6; i < 25; ++i) {
      {                                              assert( board[i] == 0 ); }
    }
    
    /* Points */
    k = 0;
      
    for(i = 0; i < 6; ++i) {
      unsigned int const nc = board[i];

      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc == 2;
      afInput[ k++ ] = nc == 3;
      afInput[ k++ ] = nc == 4;
      afInput[ k++ ] = nc == 5;
      afInput[ k++ ] = nc > 5 ? ( nc - 5 ) / 2.0 : 0.0;
    }

    /* Men off */
    {
      unsigned int menOff = 15;
      
      for(i = 0; i < 6; ++i) {
	menOff -= board[i];
      }

      for(k = 0; k < 14; ++k) {
	afInput[ BI_OFF + k ] = menOff > k ? 1.0 : 0.0;
      }
    }
    
    {
      unsigned int np = 0;
      
      for(i = 0; i < 6; ++i) {
	unsigned int const nc = board[i];

	if( nc ) {
	  np += nc * (i+1);
	}
      }
      
      afInput[BI_PIP] = np / (15.0 * 6);
    }
  }
}







static CONST char*
stdInputsNames[] =
{
  "OFF1",
  "OFF2",
  "OFF3",
  "BREAK_CONTACT",
  "BACK_CHEQUER",
  "BACK_ANCHOR",
  "FORWARD_ANCHOR",
  "PIPLOSS",
  "P1",
  "P2",
  "BACKESCAPES",
  "ACONTAIN",
  "ACONTAIN2",
  "CONTAIN",
  "CONTAIN2",
  "MOBILITY",
  "MOMENT2",
  "ENTER",
  "ENTER2",
  "TIMING",
  "BACKBONE",
  "BACKG", 
  "BACKG1",
  "FREEPIP",
  "BACKRESCAPES",
};

static void
rawInputName(char* name, unsigned int n)
{
  unsigned int point = n >= 100 ? (n - 100)/4 : n/4;

  if( point < 24 ) {
    sprintf(name, "POINT%02d-%1d", point+1, n % 4);
  } else {
    sprintf(name, "BAR-%1d", n % 4);
  }

  if( n < 100 ) {
    strcat(name, "(op)");
  }
}

static CONST char*
std250InputName(unsigned int n)
{
  static char name[32];
  name[0] = 0;
  
  if( n < 200 ) {
    rawInputName(name, n);
  } else {
    n -= 200;
    
    strcpy(name, stdInputsNames[n >= 25 ? n - 25 : n]);
    if ( n >= 25 ) {
      strcat(name, "(op)");
    }
  }
  
  {                                    assert( strlen(name) < sizeof(name) ); }
  
  return name;
}

static CONST char*
E1InputName(unsigned int n)
{
  if( n < 250 ) {
    return std250InputName(n);
  }
  return n == 250 ? "BURIED" : "BURIED(op)";
}

static CONST char*
braceInputName(unsigned int n)
{
  static char name[32];
  name[0] = 0;
  
  unsigned int h = NUM_RACE_BASE_INPUTS / 2;
  unsigned int k = (n < h) ? n : n - h;

  unsigned int point = k/4;
  sprintf(name, "POINT%02d-%1d", point+1, k % 4);

  return name;
}

static CONST char*
oldraceInputName(unsigned int n)
{
  static char name[32];
  name[0] = 0;
  unsigned int half = OLD_HALF_RACE_INPUTS;
  
  unsigned int k = (n < half) ? n : n - half;

  if( k < 23 * 4 ) {
    unsigned int point = k/4;
    sprintf(name, "POINT%02d-%1d", point+1, k % 4);
  } else {
    if( k < 23*4 + 14 ) {
      sprintf(name, "OFF-%02d", k - 23*4);
    } else {
      switch( k ) {
        case RI_NCROSS: strcpy(name, "NCROSS"); break;
        case RI_OPIP: strcpy(name, "OPIP"); break;
        case RI_PIP: strcpy(name, "PIP"); break;
        default: assert(0);
      }
    }
  }

  if ( n < half ) {
    strcat(name, "(op)");
  }

  return name;
}

static CONST char*
raceInputName(unsigned int n)
{
  static char name[32];
  name[0] = 0;
  
  unsigned int k = (n < HALF_RACE_INPUTS) ? n : n - HALF_RACE_INPUTS;

  if( k < 23 * 4 ) {
    unsigned int point = k/4;
    sprintf(name, "POINT%02d-%1d", point+1, k % 4);
  } else {
    if( k < 23*4 + 14 ) {
      sprintf(name, "OFF-%02d", k - 23*4);
    } else {
      switch( k ) {
        case RI_NCROSS: strcpy(name, "NCROSS"); break;
        default: assert(0);
      }
    }
  }

  if ( n < HALF_RACE_INPUTS ) {
    strcat(name, "(op)");
  }

  return name;
}


CONST char*
inputNameFromFunc(const NetInputFuncs* inp, unsigned int k)
{
  unsigned int i;

  if( inp->ifunc ) {
    return inp->ifunc(k);
  }
  
  for(i = 0; i < sizeof(inputFuncs)/sizeof(inputFuncs[0]); ++i) {
    if( inputFuncs[i].func == inp->func ) {
      if( inputFuncs[i].ifunc ) {
	return inputFuncs[i].ifunc(k);
      }
      return "";
    }
  }

  return 0;
}




enum {
  II_OFF1 = 0, II_OFF2, II_OFF3,
  
  /* Minimum number of pips required to break contact.

     For each checker x, N(x) is checker location,
     C(x) is max({forall o : N(x) - N(o)}, 0)

     Break Contact : (sum over x of C(x)) / 152

     152 is dgree of contact of start position.
  */
  II_BREAK_CONTACT,

  /* Location of back checker (Normalized to [01])
   */
  II_BACK_CHEQUER,

  /* Location of most backward anchor.  (Normalized to [01])
   */
  II_BACK_ANCHOR,

  /* Forward anchor in opponents home.

     Normalized in the following way:  If there is an anchor in opponents
     home at point k (1 <= k <= 6), value is k/6. Otherwise, if there is an
     anchor in points (7 <= k <= 12), take k/6 as well. Otherwise set to 2.
     
     This is an attempt for some continuity, since a 0 would be the "same" as
     a forward anchor at the bar.
   */
  II_FORWARD_ANCHOR,

  /* Average number of pips opponent loses from hits.
     
     Some heuristics are required to estimate it, since we have no idea what
     the best move actually is.

     1. If board is weak (less than 3 anchors), don't consider hitting on
        points 22 and 23.
     2. Dont break anchors inside home to hit.
   */
  II_PIPLOSS,

  /* Number of rolls that hit at least one checker.
     Same heuristics as PIPLOSS.
   */
  II_P1,

  /* Number of rolls that hit at least two checkers.
     Same heuristics as PIPLOSS.
   */
  II_P2,

  /* How many rolls permit the back checker to escape (Normalized to [01])
     More precisely, how many rolls can be fully utilized by the back checker.
     Uses only 2 of doubles.
   */
  II_BACKESCAPES,

  II_BACKRESCAPES,
  
  /* Maximum containment of opponent checkers, from our points 9 to op back 
     checker.
     
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  II_ACONTAIN,
  
  /* Maximum containment, from our point 9 to home.
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  II_CONTAIN,

  /* For all checkers out of home, 
     sum (Number of rolls that let x escape * distance from home)

     Normalized by dividing by 3600.
  */
  II_MOBILITY,

  /* One sided moment.
     Let A be the point of weighted average: 
     A = sum of N(x) for all x) / nCheckers.
     
     Then for all x : A < N(x), M = (average (N(X) - A)^2)

     Diveded by 400 to normalize. 
   */
  II_MOMENT2,

  /* Average number of pips lost when on the bar.
     Normalized to [01]
  */
  II_ENTER,

  /* Probablity of one checker not entering from bar.
     1 - (1 - n/6)^2, where n is number of closed points in op home.
   */
  II_ENTER2,

  II_TIMING,
  
  II_BACKBONE,

  II_BACKG, 
  
  II_BACKG1,
  
  II_FREEPIP,

  II_BURIED,

  II_RMOBILITY,

  II_ENTERLOSS,
  
  II_CESCAPES,
  
  II_SBREAK_CONTACT,
  /****/
  II_LAST,
};  

static CONST char*
genInputsNames[] =
{
  "OFF1",               //0
  "OFF2",               //2
  "OFF3",               //4
  "BREAK_CONTACT",      //6
  "BACK_CHEQUER",       //8
  "BACK_ANCHOR",        //10
  "FORWARD_ANCHOR",     //12
  "PIPLOSS",            //14
  "P1",                 //16
  "P2",                 //18
  "BACKESCAPES",        //20
  "BACKRESCAPES",       //22
  "ACONTAIN",           //24
  "CONTAIN",            //26
  "MOBILITY",           //28
  "MOMENT2",            //30
  "ENTER",              //32
  "ENTER2",             //34
  "TIMING",             //36
  "BACKBONE",           //38
  "BACKG",              //40
  "BACKG1",             //42
  "FREEPIP",            //44
  "BURIED",             //46
  "RMOBILITY",          //48
  "ENTERLOSS",          //50
  "CESCAPES",		//52
  "SBREAK_CONTACT",     //54
};

unsigned int
maxIinputs(void)
{
  return 2 * II_LAST - 1;
}


CONST char*
genericInputName(unsigned int n)
{
  static char name[32];
  name[0] = 0;

  if( n < 200 ) {
    rawInputName(name, n);
  } else {
    n -= 200;
    {                                                assert( n/2 < II_LAST ); }
    strcpy(name, genInputsNames[n/2]);

    if( (n & 0x1) == 0 ) {
      strcat(name, "(op)");
    }
  }
  return name;
}

void
getInputs(CONST int anBoard[2][25], CONST int* which, float* values)
{
  CONST int* j;
  float* v = values;

  CONST int* b0 = anBoard[0];
  CONST int* b1 = anBoard[1];

  BoardCache t0, t1;
  initBoardTmp(&t0);
  initBoardTmp(&t1);
  
  for(j = which; *j >= 0; ++j, ++v) {
    switch( *j ) {
      // Men off are switched to match error in CalculateContactInputs
      case 2*II_OFF1+1:
      {
	menOff0(b0, &t0, v); break;
      }
      case 2*II_OFF1:
      {
	menOff0(b1, &t1, v); break;
      }
      case 2*II_OFF2+1:
      {
	menOff1(b0, &t0, v); break;
      }
      case 2*II_OFF2:
      {
	menOff1(b1, &t1, v); break;
      }
      case 2*II_OFF3+1:
      {
	menOff2(b0, &t0, v); break;
      }
      case 2*II_OFF3:
      {
	menOff2(b1, &t1, v); break;
      }
      case 2*II_BREAK_CONTACT:
      {
	breakContact(b0, b1, &t0, v, 2); break;
      }
      case 2*II_BREAK_CONTACT+1:
      {
	breakContact(b1, b0, &t1, v, 2); break;
      }
      case 2*II_BACK_CHEQUER:
      {
	iBackChequer(b0, &t0, v); break;
      }
      case 2*II_BACK_CHEQUER+1:
      {
	iBackChequer(b1, &t1, v); break;
      }
      case 2*II_BACK_ANCHOR:
      {
	iBackAnchor(b0, &t0, v); break;
      }
      case 2*II_BACK_ANCHOR+1:
      {
	iBackAnchor(b1, &t1, v); break;
      }
      case 2*II_FORWARD_ANCHOR:
      {
	iForwardAnchor(b0, &t0, v); break;
      }
      case 2*II_FORWARD_ANCHOR+1:
      {
	iForwardAnchor(b1, &t1, v); break;
      }
      case 2*II_PIPLOSS:
      {
	iPipLoss(b0, b1, &t0, v, 2); break;
      }
      case 2*II_PIPLOSS+1:
      {
	iPipLoss(b1, b0, &t1, v, 2); break;
      }
      case 2*II_P1:
      {
	iP1(b0, b1, &t0, v); break;
      }
      case 2*II_P1+1:
      {
	iP1(b1, b0, &t1, v); break;
      }
      case 2*II_P2:
      {
	iP2(b0, b1, &t0, v); break;
      }
      case 2*II_P2+1:
      {
	iP2(b1, b0, &t1, v); break;
      }
      case 2*II_BACKESCAPES:
      {
	iBackEscapes(b0, b1, &t0, v); break;
      }
      case 2*II_BACKESCAPES+1:
      {
	iBackEscapes(b1, b0, &t1, v); break;
      }
      case 2*II_ACONTAIN:
      {
	iAContain(b0, b1, &t0, v); break;
      }
      case 2*II_ACONTAIN+1:
      {
	iAContain(b1, b0, &t1, v); break;
      }
      case 2*II_CONTAIN:
      {
	iContain(b0, &t0, v); break;
      }
      case 2*II_CONTAIN+1:
      {
	iContain(b1, &t1, v); break;
      }
      case 2*II_MOBILITY:
      {
	iMobility(b0, b1, v, 2); break;
      }
      case 2*II_MOBILITY+1:
      {
	iMobility(b1, b0, v, 2); break;
      }
      case 2*II_MOMENT2:
      {
	iMoment2(b0, v, 2); break;
      }
      case 2*II_MOMENT2+1:
      {
	iMoment2(b1, v, 2); break;
      }
      case 2*II_ENTER:
      {
	iEnter(b0, b1, v); break;
      }
      case 2*II_ENTER+1:
      {
	iEnter(b1, b0, v); break;
      }
      case 2*II_ENTER2:
      {
	iEnter2(b1, v); break;
      }
      case 2*II_ENTER2+1:
      {
	iEnter2(b0, v); break;
      }
      case 2*II_TIMING:
      {
	timing(b0, b1, &t0, v, 2); break;
      }
      case 2*II_TIMING+1:
      {
	timing(b1, b0, &t1, v, 2); break;
      }
      case 2*II_BACKBONE:
      {
	iBackbone(b0, v); break;
      }
      case 2*II_BACKBONE+1:
      {
	iBackbone(b1, v); break;
      }
      case 2*II_BACKG:
      {
	iBackG(b0, &t0, v, 2); break;
      }
      case 2*II_BACKG+1:
      {
	iBackG(b1, &t1, v, 2); break;
      }
      case 2*II_BACKG1:
      {
	iBackG1(b0, &t0, v, 2); break;
      }
      case 2*II_BACKG1+1:
      {
	iBackG1(b1, &t1, v, 2); break;
      }
      case 2*II_FREEPIP:
      {
	freePip(b0, b1, &t0, v, 2); break;
      }
      case 2*II_FREEPIP+1:
      {
	freePip(b1, b0, &t1, v, 2); break;
      }
      case 2*II_BACKRESCAPES:
      {
	iBackEscapesR(b0, b1, &t0, v); break;
      }
      case 2*II_BACKRESCAPES+1:
      {
	iBackEscapesR(b1, b0, &t1, v); break;
      }
      case 2*II_BURIED:
      {
	iBuried(b0, v); break;
      }
      case 2*II_BURIED+1:
      {
	iBuried(b1, v); break;
      }
      case 2*II_RMOBILITY:
      {
	iRMobility(b0, b1, v, 2); break;
      }
      case 2*II_RMOBILITY+1:
      {
	iRMobility(b1, b0, v, 2); break;
      }
      case 2*II_ENTERLOSS:
      {
	iEnterLoss(b1, v); break;
      }
      case 2*II_ENTERLOSS+1:
      {
	iEnterLoss(b0, v); break;
      }
      case 2*II_CESCAPES:
      {
	iEscapesContainment(b1, b0, &t1, v); break;
      }
      case 2*II_CESCAPES+1:
      {
	iEscapesContainment(b0, b1, &t0, v); break;
      }
      case 2*II_SBREAK_CONTACT:
      {
	sBreakContact(b0, b1, &t0, v, 2); break;
      }
      case 2*II_SBREAK_CONTACT+1:
      {
	sBreakContact(b1, b0, &t1, v, 2); break;
      }
      default:
      {
	printf("*j = %d\n", *j);
	assert(0);
      }
    }
  }
}

#if defined( HAVE_DLFCN_H )

#include <dlfcn.h>

const NetInputFuncs*
ifFromFile(const char* name)
{
  void* i = dlopen(name, RTLD_GLOBAL|RTLD_NOW);

  if( ! i ) {
    return 0;
  }

  void* s = dlsym(i, "inputs");

  if( ! s ) {
    dlclose(i);
    
    return 0;
  }

  ((NetInputFuncs*)s)->lib = i;
  
  return s;
} 

int
closeInputs(const NetInputFuncs* f)
{
  if( f != NULL && f->lib ) {
    if( dlclose(f->lib) != 0 ) {
      printf("%s\n", dlerror());
      return 1;
    }
  }
  return 0;
}

#endif

