/*
 * eval.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
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
 * $Id: eval.c,v 1.16 2019/02/03 14:50:29 plm Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <assert.h>
#include <cache.h>
#include <errno.h>
#include <ctype.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <math.h>
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neuralnet.h>

#include "eval.h"
#include "positionid.h"
#include "inputs.h"
#include "mt19937int.h"
#include "bearoffgammon.h"
#include "racebg.h"


#undef VERSION
#define VERSION "1.01"

char* weightsVersion = VERSION;
char* compatibleVersions[] = {NULL};

#include "osr.h"
#include "br.h"

#if defined( OS_BEAROFF_DB )
#include "bearoffdb.h"
bearoffcontext* osDB = 0;
bearoffcontext* savedosDB = 0;
#endif

/* From pub_eval.c: */
extern float pubeval( int race, int pos[] );

typedef void ( *classevalfunc )(CONST int anBoard[2][25], float arOutput[] /*, int*/);
typedef void ( *classdumpfunc )(int anBoard[2][25], char *szOutput );

#define MINPPERPOINT 4

#define CEVAL_NUM_INPUTS (25 * MINPPERPOINT * 2)
#define REVAL_NUM_INPUTS (25 * MINPPERPOINT * 2)


#define SEARCH_CANDIDATES 8
#define SEARCH_TOLERANCE 0.16


typedef CONST int (*ConstBoard)[25];


typedef struct EvalNets_ {
  CONST char*   	name;
  neuralnet* 		net;
  // prune net - a fast net used to prune moves for 0ply evaluation
  neuralnet* 		pnet;
  // cache for main net
  cache*		ncache;
  // cache for prune net
  cache*		pcache;
  // Prune leaves 'nMoves' moves
  unsigned int		nMoves;
  // Do not prune if number of moves is less than 'minNmoves'
  int           	minNmoves;
  // Function which computes the net inputs from the board
  CONST NetInputFuncs*	netInputs;
} EvalNets;

/* indexed on positionclass. don't change entries order */

static EvalNets
evalNetsGen[N_CLASSES] =
{ {"over",    0, 0, 0, 0, 0, 0, 0},
  {0,         0, 0, 0, 0, 0, 0, 0},
  {"bearoff", 0, 0, 0, 0, 0, 0, &inputFuncs[0]},
  {"race",    0, 0, 0, 0, 0, 0, &inputFuncs[1]},
  {"crashed", 0, 0, 0, 0, 0, 0, &inputFuncs[2]},
  {"contact", 0, 0, 0, 0, 0, 0, &inputFuncs[2]}
#if defined( CONTAINMENT_CODE )
  ,
  {"backcontain", 0, 0, 0, 0, 0, 0, &inputFuncs[2]}
#endif
};

/* have bearoff as alternate for race so we can train a race+bearoff net as
   bearoff with race+bearoff data
*/
   
static int CONST alternate[N_CLASSES] =
{ -1, CLASS_BEAROFF1, -1,  CLASS_BEAROFF1, CLASS_CONTACT, -1
#if defined( CONTAINMENT_CODE )
  ,CLASS_CRASHED
#endif
};

static EvalNets* nets = 0;

typedef struct {
  unsigned int  n;
  neuralnet*	net;
  cache*	ncache;

  neuralnet*	pnet;
  cache*	pcache;
} Shared;

static Shared
shared[N_CLASSES] =
{ {0, NULL, NULL, NULL, NULL},
  {0, NULL, NULL, NULL, NULL},
  {0, NULL, NULL, NULL, NULL},
};

#if defined( LOADED_BO )
bearoffcontext* pbc1 = NULL;
bearoffcontext* pbc2 = NULL;
#endif


#ifdef _MSC_VER
#  ifndef __cplusplus
#    define inline __inline
#  endif
#endif

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif
static inline int
max(int a, int b)
{
  return a > b ? a : b;
}
  
#if defined( GARY_CODE )
typedef struct _evalcache
{
  unsigned char auchKey[10];
  int nPlies;
  float ar[NUM_OUTPUTS];
} evalcache;
#endif

#if defined( ppp )

int
CacheAddTmp(cache* c, unsigned long l, evalcache* ec, size_t cb)
{
  if( ec->nPlies == 0 ) {
    unsigned int k;
    
    printf("A 0 ");
    for(k = 0; k < sizeof(ec->auchKey); ++k) {
      putchar('A' + ((ec->auchKey[k] & 0xf0) >> 4));
      putchar('A' + (ec->auchKey[k] & 0xf));
    }
    putchar('\n');
  }
  return CacheAdd(c, l, ec, cb);
}

void*
CacheLookupTmp(cache* c, unsigned long l, evalcache* ec)
{
  if( ec->nPlies != 0 ) {
    return NULL;
  }

  {
    unsigned int k;
    
    printf("L 0 ");
    for(k = 0; k < sizeof(ec->auchKey); ++k) {
      putchar('A' + ((ec->auchKey[k] & 0xf0) >> 4));
      putchar('A' + (ec->auchKey[k] & 0xf));
    }
    putchar('\n');
  }
  return CacheLookup(c, l, ec);
}

#define CacheLookup CacheLookupTmp
#define CacheAdd CacheAddTmp
#endif


#if defined( GARY_CODE )
static int
EvalCacheCompare(evalcache* p0, evalcache* p1)
{
  return !EqualKeys( p0->auchKey, p1->auchKey ) || p0->nPlies != p1->nPlies;
}

static long
EvalCacheHash(evalcache* pec)
{
  long l = 0;
  int i;
    
  l = pec->nPlies << 9;

  for( i = 0; i < 10; i++ )
    l = ( ( l << 8 ) % 8388593 ) ^ pec->auchKey[i];

  return l;    
}
#endif

void
DestroyNets(EvalNets* evalNets)
{
  positionclass k;

  if( ! evalNets ) {
    return;
  }

  {
    positionclass p[3] = {CLASS_RACE, CLASS_CRASHED, CLASS_CONTACT};
    int k;
    for(k = 0; k < 3; ++k) {
      EvalNets* e = &evalNets[p[k]];
      Shared* s = &shared[k];
      if( e->net && (s->net == e->net) ) {
	-- s->n;
	if( s->n == 0 ) {
	  s->net = NULL;
	  s->ncache = NULL;
	  s->pnet = NULL;
	  s->pcache = NULL;
	} else {
	  e->net = NULL;
	  e->ncache = NULL;
	  e->pnet = NULL;
	  e->pcache = NULL;
	}
      }
    }
  }
  
  for(k = 0; k < N_CLASSES; ++k) {
    if( evalNets[k].net != NULL ) {
      NeuralNetDestroy(evalNets[k].net);
      free(evalNets[k].net); evalNets[k].net = 0;
	
      if( evalNets[k].ncache != NULL ) {
	CacheDestroy(evalNets[k].ncache);
	free(evalNets[k].ncache); evalNets[k].ncache = 0;
      }
    }
      
    if( evalNets[k].pnet != NULL ) {
      NeuralNetDestroy(evalNets[k].pnet);
      free(evalNets[k].pnet); evalNets[k].pnet = 0;

      if( evalNets[k].pcache != NULL ) {
	CacheDestroy(evalNets[k].pcache);
	free(evalNets[k].pcache); evalNets[k].pcache = 0;
      }
    }

#if defined( HAVE_DLFCN_H )
    closeInputs(evalNets[k].netInputs);
#endif
  }

  free(evalNets);
}

EvalNets*
setNets(EvalNets* evalNets)
{
  if( ! evalNets ) {
    return nets;
  }

  {
    EvalNets* save = nets;
    nets = evalNets;
    return save;
  }
}

/* Version of last loaded net */
static char szFileVersion[16];

extern EvalNets*
LoadNet(CONST char* szWeights, long cSize)
{
  FILE* pfWeights;

  EvalNets* evalNets = 0;
  
  int compatible = 0, i = 0;

  if( !( pfWeights = fopen( szWeights, "r" ) ) ) {
    return NULL;
  }

  if( fscanf( pfWeights, "GNU Backgammon %15s\n", szFileVersion ) != 1 ||
      strncmp( szFileVersion, VERSION, strlen(VERSION) ) != 0 ) {
    fprintf(stderr, "expecting version %s, file %s", VERSION, szFileVersion);
    while (compatibleVersions[i]) {
      char *s = compatibleVersions[i++];

      if (strncmp( szFileVersion, s, strlen(s) ) == 0) {
	compatible = 1;
	fprintf(stderr, " is compatible\n");
	break;
      }
    }
  }
  else compatible = 1;
  
  if (!compatible) {
    fprintf(stderr, " is not compatible\n");
    return NULL;
  }

  evalNets = malloc(N_CLASSES * sizeof(EvalNets));
  memcpy(evalNets, evalNetsGen, N_CLASSES * sizeof(EvalNets));
  
  {
    positionclass p;
    char netType[256];
    int prune = 0;
    
    while( fgets(netType, sizeof(netType), pfWeights) ) {
      netType[strlen(netType)-1] = 0;

      prune = 0;
      if( strncmp(netType, "prune ", strlen("prune ")) == 0 ) {
	prune = 1;
	memmove(netType, netType + strlen("prune "),
		1 + strlen(netType) - strlen("prune ") );
      }

      for(p = 0; p < N_CLASSES; ++p) {
	CONST char* CONST name = evalNetsGen[p].name;
	if( name ) {
	  if( strncmp(netType, name, strlen(name)) == 0 ) {
	    break;
	  }
	}
      }

      if( p < N_CLASSES ) {
	CONST NetInputFuncs* inpFunc = 0;
	char* t = netType + strlen(evalNetsGen[p].name);
	while( *t && *t == ' ' ) { ++t; }
	if( *t ) {
	  char* x = t;
	  while( *x && !isspace(*x) ) {
	    ++x;
	  }
	  *x = 0;
	}
	
	if( *t ) {
	  inpFunc = ifByName(t);
#if defined( HAVE_DLFCN_H )
	  if( ! inpFunc ) {
	    inpFunc = ifFromFile(t);
	  }
#endif
	  if( ! inpFunc ) {
	    fprintf(stderr, "net input function %s not found", t);
	    
	    DestroyNets(evalNets);
	    return NULL;
	  }
	}
	
	  
	if( ! prune ) {
	  neuralnet* n = evalNets[p].net = malloc(sizeof(neuralnet));
	  NeuralNetInit(n);
	  
	  if( NeuralNetLoad(n, pfWeights ) == -1 ) {
	    fprintf(stderr, "failed in loading %s net", evalNets[p].name);
	    
	    DestroyNets(evalNets);
	    return NULL;
	  }

	  if( inpFunc ) {
	    evalNets[p].netInputs = inpFunc;

	    if( n->cInput != (int)inpFunc->nInputs ) {
	      DestroyNets(evalNets);
	      return NULL;
	    }
	  } else {
	    if( (int)evalNets[p].netInputs->nInputs != n->cInput ) {
	      DestroyNets(evalNets);
	      return NULL;
	    }
	  }
	  
	  switch( p ) {
	    case CLASS_CONTACT: break;
	    case CLASS_RACE: break;
	    case CLASS_CRASHED: break;
#if defined( CONTAINMENT_CODE )
	  case CLASS_BACKCONTAIN: break;
#endif
	      
	    case CLASS_BEAROFF1: break;
	    {
	      int num_bearoff_inputs = evalNets[p].netInputs->nInputs;
	      
	      if( n->cInput != num_bearoff_inputs ||
		  n->cOutput != NUM_OUTPUTS ) {
		DestroyNets(evalNets);
		return NULL;
	      }
	      break;
	    }
	    default: break;
	  }
	  
	} else {
	    
	  neuralnet* n = evalNets[p].pnet = malloc(sizeof(neuralnet));
	  NeuralNetInit(n);
	    
	  if( NeuralNetLoad(n, pfWeights ) == -1 ||
	      n->cInput != CEVAL_NUM_INPUTS ||
	      n->cOutput != NUM_OUTPUTS ) {
	    DestroyNets(evalNets);
	    return NULL;
	  }
	}
      }
    }
  }

  if( cSize < 0 ) {
#if defined( GARY_CODE )
    cSize = 4*8192;
#else
    cSize = 1L << 18;
#endif
  }
  
  {
    int k;
    for(k = 0; k < N_CLASSES; ++k) {
      if( evalNets[k].pnet ) {
	cache* c = evalNets[k].pcache = malloc(sizeof(*evalNets[k].pcache));

	if( 
#if defined( GARY_CODE )
	 CacheCreate(c, cSize, (cachecomparefunc) EvalCacheCompare)
#else
	 CacheCreate(c, cSize)
#endif
	 < 0 ) {
	  DestroyNets(evalNets);
	  return 0;
	}
      }
    }
  }

  {
    cache* c = evalNets[CLASS_CONTACT].ncache = malloc(sizeof(cache));
    if(
#if defined( GARY_CODE )
	 CacheCreate(c, cSize, (cachecomparefunc) EvalCacheCompare)
#else
	 CacheCreate(c, cSize)
#endif
	 < 0 ) {
      DestroyNets(evalNets);
      return 0;
    }
  }
  
  fclose( pfWeights );

  // can't do sharing before nets are loaded
  if( nets ) {
    positionclass p[3] = {CLASS_RACE, CLASS_CRASHED, CLASS_CONTACT};
    int k;
    for(k = 0; k < 3; ++k) {
      positionclass CONST i = p[k];
      EvalNets* e = &evalNets[i];

      if( ! e->net ) {
	int const a = alternate[i];
	if( a >= 0 ) {
	  // alternate found, no need for sharing
	  continue;
	}
	
	if( ! shared[k].net ) {
	  EvalNets* p = &nets[i]; 
	  shared[k].net = p->net;
	  shared[k].ncache = p->ncache;
	  shared[k].pnet = p->pnet;
	  shared[k].pcache = p->pcache;
	  /* original user */
	  shared[k].n = 1;
	}

	e->net = shared[k].net;
	assert( ! e->ncache && ! e->pnet && ! e->pcache);
	  
	e->ncache = shared[k].ncache;
	e->pnet = shared[k].pnet;
	e->pcache = shared[k].pcache;
	++ shared[k].n;
      }

      if( ! e->net ) {
	DestroyNets(evalNets);
	return NULL;
      }
    }
  }
      
  return evalNets;
}

extern void 
EvalShutdown(void)
{
#if defined( OS_BEAROFF_DB )
  BearoffClose(osDB);
#endif
#if defined( LOADED_BO )
  BearoffClose(pbc1);
  BearoffClose(pbc2);
#endif
}

extern CONST char*
EvalInitialise(CONST char* szWeights
#if defined( LOADED_BO )
	       , CONST char* osDataBase, CONST char* tsDataBase
#endif 
#if defined( OS_BEAROFF_DB )
	       , CONST char* osDBfilename
#endif
)
{
  /* FIXME allow starting without bearoff database (to generate it later!) */

#if defined( LOADED_BO )
  if( ! pbc1 ) {
    pbc1 = BearoffInit(osDataBase, BO_IN_MEMORY);

    if( ! pbc1 ) {
      return NULL;
    }
  }
  
  if( ! pbc2 ) {
    pbc2 = BearoffInit(tsDataBase, BO_IN_MEMORY);

    if( ! pbc2 ) {
      return NULL;
    }
  }
#endif

#if defined( OS_BEAROFF_DB )
  osDB = BearoffInit(osDBfilename, BO_NONE);
#endif
  
  ComputeTable();
    
  if( szWeights ) {
    nets = LoadNet(szWeights, -1);

    if( nets == NULL ) {
      return NULL;
    }
      
  } else {
    neuralnet* n = nets[CLASS_CONTACT].net = malloc(sizeof(neuralnet));
    NeuralNetCreate( n , evalNetsGen[CLASS_CONTACT].netInputs->nInputs,
		     128 /* FIXME */, NUM_OUTPUTS, 0.1, 1.0 );

    n = nets[CLASS_RACE].net = malloc(sizeof(neuralnet)); 
    NeuralNetCreate(n , evalNetsGen[CLASS_RACE].netInputs->nInputs,
		    128 /* FIXME */, NUM_OUTPUTS, 0.1, 1.0 );

    {
      cache* c = nets[CLASS_CONTACT].ncache = malloc(sizeof(cache));
      if(
#if defined( GARY_CODE )
	 CacheCreate(c, 4*8192, (cachecomparefunc) EvalCacheCompare)
#else
	 CacheCreate(c, (1L << 18))
#endif
	  < 0 ) {
	return NULL;
      }
    }
  }

  return szFileVersion;
}

extern int
EvalSave(CONST char* szWeights, int c)
{
  FILE *pfWeights;
    
  if( !( pfWeights = fopen( szWeights, "w" ) ) )
    return -1;
    
  fprintf( pfWeights, "GNU Backgammon %s\n", VERSION );

  {
    int k;
    for(k = 0; k < N_CLASSES; ++k) {
      if( c != -1 && c != k ) {
	continue;
      }
      
      if( nets[k].net ) {
	const char* iName = "";
	if( nets[k].netInputs != evalNetsGen[k].netInputs ) {
	  iName = nets[k].netInputs->name;
	}
	
	fprintf(pfWeights, "%s %s\n", evalNetsGen[k].name, iName);
	NeuralNetSave( nets[k].net, pfWeights );
      }

      if( c != -1 ) {
	/* no prune for partial save */
	continue;
      }
      
      if( nets[k].pnet ) {
	fprintf(pfWeights, "prune %s\n", evalNetsGen[k].name);
	NeuralNetSave( nets[k].pnet, pfWeights );
      }
    }
  }
    
  fclose( pfWeights );

  return 0;
}

    
static inline void
swap( int *p0, int *p1 )
{
  int n = *p0;

  *p0 = *p1;
  *p1 = n;
}

extern void
SwapSides( int anBoard[2][25] )
{
  int i, n;

  for( i = 0; i < 25; i++ ) {
    n = anBoard[ 0 ][ i ];
    anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ];
    anBoard[ 1 ][ i ] = n;
  }
}

extern void
RollDice( int anDice[2] )
{
  unsigned long tmprnd;
  const unsigned long exp232_q = 715827882;
  const unsigned long exp232_l = 4294967292U;

  while ((tmprnd = genrand()) >= exp232_l)
    ; /* Try again */
  anDice[0] = 1 + tmprnd / exp232_q;
  while ((tmprnd = genrand()) >= exp232_l)
    ;
  anDice[1] = 1 + tmprnd / exp232_q;
}

extern void
RollDiceUnSafe( int anDice[2] )
{
  RollDice(anDice);
  
  if( anDice[0] < anDice[1] ) {
    swap(anDice, anDice + 1);
  }
}


static void
SanityCheck(CONST int anBoard[2][25], float arOutput[] )
{
  int i, j, ac[2], anBack[2], fContact;

  ac[ 0 ] = ac[ 1 ] = anBack[ 0 ] = anBack[ 1 ] = 0;
    
  for( i = 0; i < 25; i++ )
    for( j = 0; j < 2; j++ )
      if( anBoard[ j ][ i ] ) {
	anBack[ j ] = i;
	ac[ j ] += anBoard[ j ][ i ];
      }

  fContact = anBack[ 0 ] + anBack[ 1 ] >= 24;
    
  if( ac[ 0 ] < 15 )
    /* Opponent has borne off; no gammons or backgammons possible */
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;
  else if( !fContact && anBack[ 0 ] < 18 )
    /* Opponent is out of home board; no backgammons possible */
    arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

  if( ac[ 1 ] < 15 )
    /* Player has borne off; no gammons or backgammons possible */
    arOutput[ OUTPUT_LOSEGAMMON ] = arOutput[ OUTPUT_LOSEBACKGAMMON ] =
      0.0;
  else if( !fContact && anBack[ 1 ] < 18 )
    /* Player is out of home board; no backgammons possible */
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

  if( arOutput[ OUTPUT_WIN ] < 0.0f )
    arOutput[ OUTPUT_WIN ] = 0.0f;
  else if( arOutput[ OUTPUT_WIN ] > 1.0f )
    arOutput[ OUTPUT_WIN ] = 1.0f;

  if( arOutput[ OUTPUT_WINGAMMON ] > arOutput[ OUTPUT_WIN ] ) {
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_WIN ];
  }

  {
    float lose = 1.0 - arOutput[ OUTPUT_WIN ];
    if( arOutput[ OUTPUT_LOSEGAMMON ] > lose ) {
      arOutput[ OUTPUT_LOSEGAMMON ] = lose;
    }
  }
    
  /* Backgammons cannot exceed gammons */
  if( arOutput[ OUTPUT_WINBACKGAMMON ] > arOutput[ OUTPUT_WINGAMMON ] )
    arOutput[ OUTPUT_WINBACKGAMMON ] = arOutput[ OUTPUT_WINGAMMON ];
    
  if( arOutput[ OUTPUT_LOSEBACKGAMMON ] > arOutput[ OUTPUT_LOSEGAMMON ] )
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ];

  {
    float noise = 1/10000.0;

    for(i = OUTPUT_WINGAMMON; i < 5; ++i) {
      if( arOutput[i] < noise ) {
	arOutput[i] = 0.0;
      }
    }
  }
}


positionclass
ClassifyPosition(CONST int anBoard[2][25])
{
  int nOppBack = -1, nBack = -1;

  for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
    if( anBoard[0][nOppBack] ) {
      break;
    }
  }

  for(nBack = 24; nBack >= 0; --nBack) {
    if( anBoard[1][nBack] ) {
      break;
    }
  }

  if( nBack < 0 || nOppBack < 0 )
    return CLASS_OVER;

  if( nBack + nOppBack > 22 ) {
    unsigned int const N = 6;
    unsigned int i;
    unsigned int side;

    positionclass c = CLASS_CONTACT;
    
    for(side = 0; side < 2; ++side) {
      unsigned int tot = 0;
	
      const int* board = anBoard[side];
      
      for(i = 0;  i < 25; ++i) {
	tot += board[i];
      }

      if( tot <= N ) {
	c = CLASS_CRASHED; break;
      } else {
	if( board[0] > 1 ) {
	  if( (tot - board[0]) <= N ) {
	    c = CLASS_CRASHED; break;
	  } else {
	    if( board[1] > 1 && (1 + tot - (board[0] + board[1])) <= N ) {
	      c = CLASS_CRASHED; break;
	    }
	  }
	} else {
	  if( ((int)tot - (board[1] - 1)) <= (int)N ) {
	    c = CLASS_CRASHED; break;
	  }
	}
      }
    }
#if defined( CONTAINMENT_CODE )
    if( c == CLASS_CRASHED ) {
#if !defined(TRY2)
      for(side = 0; side < 2; ++side) {
	unsigned int tot = 0;
	const int* board = anBoard[side];
	for(i = 6;  i < 25; ++i) {
	  tot += board[i];
	}
	if( tot >= 9 ) {
	  c = CLASS_BACKCONTAIN; break;
	}
      }
#else
      {
	unsigned int  tot = 0;
	for(i = 12; i < 25; ++i) {
	  tot += anBoard[1-side][i];
	}
	if( tot > 4 ) {
	  c = CLASS_BACKCONTAIN;
	}
      }
#endif
    }
#endif
   
    return c;
  }
  else if( nBack > 5 || nOppBack > 5 )
    return CLASS_RACE;

  if( PositionBearoff( anBoard[ 0 ] ) > 923 ||
      PositionBearoff( anBoard[ 1 ] ) > 923 )
    return CLASS_BEAROFF1;

  return CLASS_BEAROFF2;
}

static void
EvalBearoff1( CONST int anBoard[2][25], float arOutput[] /*, int ignore*/);

static void
EvalBearoff2( CONST int anBoard[2][25], float arOutput[] /*, int ignore */ )
{
  if( nets[CLASS_BEAROFF1].net ) {
    EvalBearoff1(anBoard, arOutput /*, ignore*/);
    return;
  }
  
#if defined(LOADED_BO)
  assert ( pbc2 );

  BearoffEval ( pbc2, anBoard, arOutput );
#elif !defined(HAVE_BEAROFF2)
  EvalBearoff1(anBoard, arOutput, ignore);
#else
  int n, nOpp;
  
  nOpp = PositionBearoff( anBoard[ 0 ] );
  n = PositionBearoff( anBoard[ 1 ] );

  arOutput[ OUTPUT_WINGAMMON ] =
    arOutput[ OUTPUT_LOSEGAMMON ] =
    arOutput[ OUTPUT_WINBACKGAMMON ] =
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

  arOutput[ OUTPUT_WIN ] = getBearoffProbs2(n, nOpp) / 65535.0;
#endif
}

#if defined( LOADED_BO )
void
getBearoffProbs(unsigned int n, int aaProb[32])
{
  float p[32];
  int i;

  BearoffDist(pbc1, n, p, 0, 0, 0, 0);
  for(i = 0; i < 32; ++i) {
    aaProb[i] = (int)(0.5 + p[i] * 65535);
  }
}

void
getBearoff(unsigned int n, struct B* b)
{
  int aaProb[32];
  getBearoffProbs(n, aaProb);

  b->start = 0;
  for(/**/; b->start < 31; b->start += 1) {
    if( aaProb[b->start] != 0 ) {
      break;
    }
  }

  b->len = 32 - b->start;

  {
    int i;
    for(i = 31; i >= 0; --i) {
      if( aaProb[i] != 0 ) {
	break;
      }

      b->len -= 1;
    }
  }
  
  {
    float* const f = b->p;
    unsigned int i;
    
    for(i = b->start; i < b->start + b->len; ++i) {
      int const p = aaProb[i];

      f[i - b->start] = p / 65535.0;
    }
  }
}

#endif


/*static*/ void
setGammonProb(CONST int anBoard[2][25], int bp0, int bp1, float* g0, float* g1)
{
  int i;
  int prob[32];

  int tot0 = 0;
  int tot1 = 0;
  
  for(i = 5; i >= 0; --i) {
    tot0 += anBoard[0][i];
    tot1 += anBoard[1][i];
  }

  {                                       assert( tot0 == 15 || tot1 == 15 ); }

  *g0 = 0.0;
  *g1 = 0.0;

  if( tot0 == 15 ) {
    double make[3];

    CONST struct GammonProbs* gp = getBearoffGammonProbs(anBoard[0]);

    getBearoffProbs(bp1, prob);
    
    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));
    
    *g1 = ((prob[1] / 65535.0) +
	   (1 - make[0]) * (prob[2] / 65535.0) +
	   (1 - make[1]) * (prob[3] / 65535.0) +
	   (1 - make[2]) * (prob[4] / 65535.0));
  }

  if( tot1 == 15 ) {
    double make[3];

    CONST struct GammonProbs* gp = getBearoffGammonProbs(anBoard[1]);

    getBearoffProbs(bp0, prob);

    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));

    *g0 = ((prob[1] / 65535.0) * (1-make[0]) + (prob[2]/65535.0) * (1-make[1])
	   + (prob[3]/65535.0) * (1-make[2]));
  }
}  

static void /*unsigned long*/
EvalBearoff1Full(CONST int anBoard[2][25], float arOutput[])
{
  int i, j, n, nOpp, aaProb[2][ 32 ];
  unsigned long x;
    
  nOpp = PositionBearoff( anBoard[ 0 ] );
  n = PositionBearoff( anBoard[ 1 ] );

  if( n > 38760 || nOpp > 38760 )
    /* a player has no chequers off; gammons are possible */
    
    setGammonProb(anBoard, nOpp, n,
		  &arOutput[ OUTPUT_LOSEGAMMON ],
		  &arOutput[ OUTPUT_WINGAMMON ] );
  else {
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] = 0.0;
  }

  arOutput[ OUTPUT_WINBACKGAMMON ] = arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;
	  
  getBearoffProbs(n, aaProb[0]);
  getBearoffProbs(nOpp, aaProb[1]);
    
  x = 0;
  for( i = 0; i < 32; i++ )
    for( j = i; j < 32; j++ )
      x += aaProb[ 0 ][ i ] * aaProb[ 1 ][ j ];

  arOutput[ OUTPUT_WIN ] = x / ( 65535.0 * 65535.0 );
}

extern void
EvalBearoffOneSided(CONST int anBoard[2][25], float arOutput[])
{
  EvalBearoff1Full(anBoard, arOutput);
}

  


static void
EvalBearoff1(CONST int anBoard[2][25], float arOutput[]
	     /*, int ignore __attribute__((unused))*/)
{
  const EvalNets* n = &nets[CLASS_BEAROFF1];
  if( n->net ) {
    float arInput[MAX_NUM_INPUTS];
    
    // CalculateBearoffInputs(anBoard, arInput);
    n->netInputs->func(anBoard, arInput);
    // assert( n->netInputs->func  == CalculateBearoffInputs );

    NeuralNetEvaluate(n->net, arInput, arOutput/*, NNEVAL_NONE*/);

    SanityCheck(anBoard, arOutput);
  } else {
    EvalBearoff1Full( anBoard, arOutput );
  }
}

  

enum {
  G_POSSIBLE = 0x1,
  BG_POSSIBLE = 0x2,
  OG_POSSIBLE = 0x4,
  OBG_POSSIBLE = 0x8,
};


/* side - side that potentially can win a backgammon */
/* Return - Probablity that side will win a backgammon */

float
raceBGprob(CONST int anBoard[2][25], unsigned int side)
{
  int totMenHome = 0;
  int totPipsOp = 0;
  unsigned int i;
  int dummy[2][25];
  
  for(i = 0; i < 6; ++i) {
    totMenHome += anBoard[side][i];
  }
  // its a race
  {                 assert( anBoard[1-side][23] == 0 && anBoard[1-side][24] == 0); } 
  for(i = 22; i >= 18; --i) {
    totPipsOp += anBoard[1-side][i] * (i-17);
  }

  if( totPipsOp == 0 ||
      ! ((totMenHome + 3) / 4 - (side == 1 ? 1 : 0) <= (totPipsOp + 2) / 3) ) {
    return 0.0;
  }

  for(i = 0; i < 25; ++i) {
    dummy[side][i] = anBoard[side][i];
  }

  for(i = 0; i < 6; ++i) {
    dummy[1-side][i] = anBoard[1-side][18+i];
  }

  for(i = 6; i < 25; ++i) {
    dummy[1-side][i] = 0;
  }

  {
    const long* bgp = getRaceBGprobs(dummy[1-side]);
    if( bgp ) {
      int k = PositionBearoff(anBoard[side]);
      int aProb[32];

      float p = 0.0;
      unsigned int j;

      unsigned long scale = (side == 0) ? 36 : 1;

      getBearoffProbs(k, aProb);

      for(j = 1-side; j < (int)RBG_NPROBS; j++) {
	unsigned long sum = 0;
	scale *= 36;
	for(i = 1; i <= j + side; ++i) {
	  sum += aProb[i];
	}
	p += ((float)bgp[j])/scale * sum;
      }

      p /= 65535.0;
	
      return p;
	  
    } else {
      float p[5];
      
      if( PositionBearoff( dummy[ 0 ] ) > 923 ||
	  PositionBearoff( dummy[ 1 ] ) > 923 ) {
	EvalBearoff1((ConstBoard)dummy, p /*, -1*/);
      } else {
	EvalBearoff2((ConstBoard)dummy, p /*, -1*/);
      }

      return side == 1 ? p[0] : 1 - p[0];
    }
  }
}  

static void
raceImprovement(CONST int anBoard[2][25], float arOutput[])
{
  /* anBoard[1] is on roll */
  int totMen0 = 0, totMen1 = 0;
  int any = 0;
  int i;
    
  {                      assert(anBoard[0][23] == 0 && anBoard[0][24] == 0); }
  {                      assert(anBoard[1][23] == 0 && anBoard[1][24] == 0); }
    
  for(i = 22; i >= 0; --i) {
    totMen0 += anBoard[0][i];
    totMen1 += anBoard[1][i];
  }

  if( totMen1 == 15 ) {
    any |= OG_POSSIBLE;
  }

  if( totMen0 == 15 ) {
    any |= G_POSSIBLE;
  }

  if( any ) {
    if( any & OG_POSSIBLE ) {
      for(i = 23; i >= 18; --i) {
	if( anBoard[1][i] > 0 ) {
	  break;
	}
      }

      if( i >= 18 ) {
	any |= OBG_POSSIBLE;
      }
    }

    if( any & G_POSSIBLE ) {
      for(i = 23; i >= 18; --i) {
	if( anBoard[0][i] > 0 ) {
	  break;
	}
      }

      if( i >= 18 ) {
	any |= BG_POSSIBLE;
      }
    }
  }
    
  if( any & (BG_POSSIBLE | OBG_POSSIBLE) ) {
    int side = (any & BG_POSSIBLE) ? 1 : 0;

    float pr = raceBGprob(anBoard, side);

    if( pr > 0.0 ) {
      if( side == 1 ) {
	arOutput[OUTPUT_WINBACKGAMMON] = pr;

	if( arOutput[OUTPUT_WINGAMMON] < arOutput[OUTPUT_WINBACKGAMMON] ) {
	  arOutput[OUTPUT_WINGAMMON] = arOutput[OUTPUT_WINBACKGAMMON];
	}
      } else {
	arOutput[OUTPUT_LOSEBACKGAMMON] = pr;

	if( arOutput[OUTPUT_LOSEGAMMON] < arOutput[OUTPUT_LOSEBACKGAMMON] ) {
	  arOutput[OUTPUT_LOSEGAMMON] = arOutput[OUTPUT_LOSEBACKGAMMON];
	}
      }
    } else {
      if( side == 1 ) {
	arOutput[OUTPUT_WINBACKGAMMON] = 0.0;
      } else {
	arOutput[OUTPUT_LOSEBACKGAMMON] = 0.0;
      }
    }
  }

  if( any & (G_POSSIBLE|OG_POSSIBLE) ) {
    int side = (any & G_POSSIBLE) ? 1 : 0;

    int totPip = 0;
    int nCross = 0;
    int k;
      
    for(i = 0; i < 23; ++i) {
      int p = anBoard[side][i];
      if( p ) {
	totPip += (i+1) * p;
      }
    }

    for(k = 3; k >= 1; --k) {
      int p = 6*k + 5;
      for(i = 0; i < 6; ++i) {
	int c = anBoard[1-side][p-i];
	if( c ) {
	  nCross += k * c;
	}
      }
    }

    {
      int saverAtMostMoves = 1 + (nCross / 4);
      int opAtLeast = max(((side ? totMen1 : totMen0)+1)/2, (totPip + 2)/3);
      
      
      if( opAtLeast < saverAtMostMoves + side ) {
	/* certain gammon */
	arOutput[side ? OUTPUT_WINGAMMON : OUTPUT_LOSEGAMMON] = 1.0;
	arOutput[OUTPUT_WIN] = side ? 1.0 : 0.0;
      }
    }
  }
}

#if defined( OS_BEAROFF_DB )
void
disableOSdb(void)
{
  savedosDB = osDB;
  osDB = NULL;
}

void
enableOSdb(void)
{
  osDB = savedosDB;
  savedosDB = NULL;
}

int
EvalOSrace(CONST int anBoard[2][25], float arOutput[])
{
  if( isBearoff(osDB, anBoard) ) { 
    BearoffEval(osDB, anBoard, arOutput);
    /* detect certain wins/ gammons */
    raceImprovement(anBoard, arOutput);
    return 1;
  }
  
  return 0;
}
#endif

static void
EvalClass(positionclass pc, CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
  float arInput[MAX_NUM_INPUTS];

  /* NNEvalType t = ((nm > 0) ? NNEVAL_FROMBASE : */
  /* 		  ((nm == 0) ? NNEVAL_SAVE : NNEVAL_NONE)); */
  
  const EvalNets* n = &nets[pc];
  while( ! n->net ) {
    int const a = alternate[pc];
    {                                                        assert( a >= 0 ); }
    n = &nets[a];
    pc = a;
  }
  
  n->netInputs->func(anBoard, arInput);
    
  NeuralNetEvaluate(n->net, arInput, arOutput /*,t*/);
}

static void
EvalRace(CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
#if defined( OS_BEAROFF_DB )
  if( osDB && isBearoff(osDB, anBoard) ) { 
    BearoffEval(osDB, anBoard, arOutput);
  } else {
#endif

    EvalClass(CLASS_RACE, anBoard, arOutput /*, nm*/);

#if defined( OS_BEAROFF_DB )
  }
#endif
  
  raceImprovement(anBoard, arOutput);
  
  /* sanity check will take care of rest */
}

void
NetEvalRace(CONST int anBoard[2][25], float arOutput[])
{
#if defined( OS_BEAROFF_DB )
  bearoffcontext* tmp = osDB;
  osDB = 0;
#endif

  EvalRace(anBoard, arOutput /*, -1*/);
  
#if defined( OS_BEAROFF_DB )
  osDB = tmp;
#endif
  
  SanityCheck(anBoard, arOutput);
}

static void
EvalContact(CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
  EvalClass(CLASS_CONTACT, anBoard, arOutput/*, nm*/);
}

static void
EvalCrashed(CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
  if( nets[CLASS_CRASHED].net ) {
    EvalClass(CLASS_CRASHED, anBoard, arOutput/*, nm*/);
  } else {
    EvalContact(anBoard, arOutput /*, nm*/);
  }
}

#if defined( CONTAINMENT_CODE )
static void
EvalBackContain(CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
  if( nets[CLASS_BACKCONTAIN].net ) {
    EvalClass(CLASS_BACKCONTAIN, anBoard, arOutput /*, nm*/);
  } else {
    EvalCrashed(anBoard, arOutput /*, nm*/);
  }
}
#endif

static void
EvalOver( CONST int anBoard[2][25], float arOutput[] /*, int n*/)
{
  int i, c;

  //(void) n;
  
  for( i = 0; i < 25; i++ )
    if( anBoard[ 0 ][ i ] )
      break;

  if( i == 25 ) {
    /* opponent has no pieces on board; player has lost */
    arOutput[ OUTPUT_WIN ] = arOutput[ OUTPUT_WINGAMMON ] =
      arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

    for( i = 0, c = 0; i < 25; i++) {
      c += anBoard[1][i];
    }

    if( c > 14 ) {
      /* player still has all pieces on board; loses gammon */
      arOutput[ OUTPUT_LOSEGAMMON ] = 1.0;

      for( i = 18; i < 25; i++ )
	if( anBoard[ 1 ][ i ] ) {
	  /* player still has pieces in opponent's home board;
	     loses backgammon */
	  arOutput[ OUTPUT_LOSEBACKGAMMON ] = 1.0;

	  return;
	}
	    
      arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

      return;
    }

    arOutput[ OUTPUT_LOSEGAMMON ] =
      arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

    return;
  }
    
  for( i = 0; i < 25; i++ )
    if( anBoard[ 1 ][ i ] )
      break;

  if( i == 25 ) {
    /* player has no pieces on board; wins */
    arOutput[ OUTPUT_WIN ] = 1.0;
    arOutput[ OUTPUT_LOSEGAMMON ] = arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

    for( i = 0, c = 0; i < 25; i++ )
      c += anBoard[ 0 ][ i ];

    if( c > 14 ) {
      /* opponent still has all pieces on board; win gammon */
      arOutput[ OUTPUT_WINGAMMON ] = 1.0;

      for( i = 18; i < 25; i++ )
	if( anBoard[ 0 ][ i ] ) {
	  /* opponent still has pieces in player's home board;
	     win backgammon */
	  arOutput[ OUTPUT_WINBACKGAMMON ] = 1.0;

	  return;
	}
	    
      arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

      return;
    }

    arOutput[ OUTPUT_WINGAMMON ] =
      arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;
  }
}



  
static classevalfunc acef[ N_CLASSES ] = {
  EvalOver, EvalBearoff2, EvalBearoff1, EvalRace, EvalCrashed, EvalContact
#if defined( CONTAINMENT_CODE )
  ,EvalBackContain
#endif
};


static void
evalPly(positionclass pc, CONST int anBoard[2][25], float arOutput[] /*, int nm*/)
{
  float arInput[CEVAL_NUM_INPUTS];
  neuralnet* n = nets[pc].pnet;                                assert( n ); 
  
  baseInputs(anBoard, arInput);
    
  //(void) nm;
  NeuralNetEvaluate(n, arInput, arOutput /*, NNEVAL_NONE*/);

  if( pc == CLASS_RACE ) {
    raceImprovement(anBoard, arOutput);
  }
}

int
evalPrune(CONST int anBoard[2][25], float arOutput[])
{
  positionclass pc = ClassifyPosition(anBoard);
  
  if( ! nets[pc].pnet ) {
    acef[pc](anBoard, arOutput/*, 0*/);
    return 0;
  }
  
  evalPly(pc, anBoard, arOutput /*, 0*/);

  if( pc == CLASS_RACE ) {
    raceImprovement(anBoard, arOutput);
  }
  return 1;
}

static float Utility( float ar[ NUM_OUTPUTS ] )
{
  return (ar[ OUTPUT_WIN ] * 2.0 - 1.0 + ar[ OUTPUT_WINGAMMON ] +
	  ar[ OUTPUT_WINBACKGAMMON ] - ar[ OUTPUT_LOSEGAMMON ] -
	  ar[ OUTPUT_LOSEBACKGAMMON ]);
}

typedef int ( *cfunc )( const void *, const void * );

typedef float (*UF)(float p[NUM_OUTPUTS], int direction);
UF equityFunc = 0;


static int CompareMoves( const move *pm0, const move *pm1 )
{
  return pm1->rScore > pm0->rScore ? 1 : -1;
}

float
pubEvalVal(int race, int b[2][25])
{
  int anPubeval[28], j;

  int* b0 = b[1];
  int* b1 = b[0];

  anPubeval[ 26 ] = 15; 
  anPubeval[ 27 ] = -15;

  for(j = 0; j < 24; j++) {
    int nc = b1[j];
    
    if( nc ) {
      anPubeval[j + 1] = nc;
      anPubeval[26] -= nc; 
    }
    else {
      nc = b0[23 - j];
      anPubeval[j + 1] = -nc;
      anPubeval[27] += nc;
    }
  }
  
  anPubeval[26] -= b1[24];
  anPubeval[27] += b0[24];

  anPubeval[0] = -b0[24];
  anPubeval[25] = b1[24];

  return pubeval(race, anPubeval);
}


void
sortPubEval(movelist* pml, int nDice0, int nDice1, CONST int anBoard[2][25])
{
  unsigned int i;

  GenerateMoves(pml, anBoard, nDice0, nDice1);
    
  if( pml->cMoves == 0 ) {
    /* no legal moves */
    return;
  }
  else if( pml->cMoves == 1 ) {
    /* forced move */
  } else {
    int fRace = ClassifyPosition( anBoard ) <= CLASS_RACE;

    int board[2][25];

    for(i = 0; (int)i < pml->cMoves; i++) {
      PositionFromKey(board, pml->amMoves[i].auch);

      pml->amMoves[i].rScore = pubEvalVal(fRace, board);

      SwapSides(board);
      PositionKey((ConstBoard)board, pml->amMoves[i].auch);
    }
    
    qsort( pml->amMoves, pml->cMoves, sizeof(move), (cfunc) CompareMoves);
  }
}


static int preFilter = 0;
#if defined( PUBEVALFILTER )
static int nMovesPubEvalfilter = 0;
#endif


int
setShortCuts(int use)
{
  int prev = preFilter;
  preFilter = use;
  return prev;
}

#if defined( PUBEVALFILTER )
int
setPubevalShortCuts(unsigned int nMoves)
{
  int prev = nMovesPubEvalfilter;
  nMovesPubEvalfilter = nMoves;
  return prev;
}
#endif

#if defined( nono )
void
setNetShortCuts(unsigned int nMovesContact, unsigned int nMovesRace)
{
  int k;
  
  for(k = 0; k < 2; ++k) {
    EvalNets* e = &nets[ (k == 0) ? CLASS_CONTACT : CLASS_RACE];

    neuralnet* n = e->pnet;
    
    if( n ) {
      float const ratio =
	0.5 * (n->cInput * n->cHidden / (1.0 * 200 * 128));
      
      e->nMoves = (k == 0 ? nMovesContact : nMovesRace);

      e->minNmoves = e->nMoves / (1 - ratio);
    }
  }
}
#endif

void
setNetShortCuts(unsigned int nsc)
{
  int k;
  int c[3] = {CLASS_CONTACT, CLASS_CRASHED, CLASS_RACE};
  
  for(k = 0; k < 3; ++k) {
    EvalNets* e = &nets[ c[k] ];

    neuralnet* n = e->pnet;
    
    if( n ) {
      float const ratio =
	0.5 * (n->cInput * n->cHidden / (1.0 * 200 * 128));
      
      e->nMoves = nsc;

      e->minNmoves = e->nMoves / (1 - ratio);
    }
  }
}

#define min(x,y)   (((x) > (y)) ? (y) : (x))

extern void
FindBestMoveInEval(int nDice0, int nDice1, int anBoard[2][25], int direction)
{
  unsigned int i;
  unsigned int pass;
  movelist ml;

  GenerateMoves(&ml, (ConstBoard)anBoard, nDice0, nDice1);
    
  if( ml.cMoves == 0 ) {
    /* no legal moves */
    return;
  }

  if( ml.cMoves == 1 ) {
    /* forced move */
    ml.iMoveBest = 0;
  } else {
    /* choice of moves */

    positionclass posClass = ClassifyPosition((ConstBoard)anBoard);
    
#if defined( PUBEVALFILTER )
    if( nMovesPubEvalfilter && (ml.cMoves > nMovesPubEvalfilter)
	&& posClass == CLASS_RACE ) {
      //int fRace = ClassifyPosition(anBoard) <= CLASS_RACE;
	
      for(i = 0; (int)i < ml.cMoves; i++) {
	PositionFromKey(anBoard, ml.amMoves[i].auch);

	ml.amMoves[i].rScore = pubEvalVal(1, anBoard);
      }
	  
      qsort(ml.amMoves, ml.cMoves, sizeof(move), (cfunc)CompareMoves);
	  
      ml.cMoves = nMovesPubEvalfilter;
    }
#endif

    for(pass = 0; pass < 2; ++pass) {
      unsigned int const nMoves = nets[posClass].nMoves;
      
      int const useEvalNet =
	(pass == 0 && nMoves != 0 && ml.cMoves > nets[posClass].minNmoves );
      
      //int first = 1;

      if( pass == 1 ) {
	qsort(ml.amMoves, ml.cMoves, sizeof(move), (cfunc)CompareMoves);
	ml.cMoves = min(ml.cMoves, (int)nMoves);
      }
    
      ml.rBestScore = -99999.9;
    
      for(i = 0; (int)i < ml.cMoves; i++) {
	PositionFromKey(anBoard, ml.amMoves[i].auch);

	SwapSides(anBoard);

	{
#if defined( GARY_CODE )
	  evalcache ec, *pec;
#else
	  cacheNode ec, *pec;
#endif

	  unsigned long l;
	  cache* c =
	    useEvalNet ? nets[posClass].pcache : nets[CLASS_CONTACT].ncache;
      
	  positionclass pc = ClassifyPosition((ConstBoard)anBoard);

	  if( pass == 0 || posClass == pc )  {
	    // For non contact from contact position,
	    // first and second pass are the same
	  
	    PositionKey((ConstBoard)anBoard, ec.auchKey);
	    ec.nPlies = 0;
      
#if defined( GARY_CODE )
	    l = EvalCacheHash(&ec);
    
	    if( ( pec = CacheLookup( c, l, &ec ) ) ) 
#else
	    if( ( pec = CacheLookup( c, &ec, &l ) ) )   
#endif
	      {
	      memcpy( ec.ar, pec->ar, sizeof( pec->ar ) );

	    } else {
	      if( useEvalNet && pc == posClass ) {
		evalPly(pc, (ConstBoard)anBoard, ec.ar /*, i*/);
	      } else {
		//int b = (pc != posClass) ? -1 : (first ? 0 : 1);
		//if( b == 0 ) {
		//	  first = 0;
		//	}
		
		acef[pc]((ConstBoard)anBoard, ec.ar /*, b*/);
	      }

	      SanityCheck((ConstBoard)anBoard, ec.ar);

#if defined( Gary )
	      CacheAdd(c, l, &ec, sizeof ec);
#else
	      CacheAdd(c, &ec, l);
#endif
	    }

	    InvertEvaluation(ec.ar);

	    ml.amMoves[i].rScore
	      = equityFunc ? (*equityFunc)(ec.ar, direction) : Utility(ec.ar);
	  }
	
	  if( ml.amMoves[i].rScore > ml.rBestScore ) {
	    ml.iMoveBest = i;
	    ml.rBestScore = ml.amMoves[i].rScore;
	  }
	}
      }

      if( ! useEvalNet || nMoves == 1 ) {
	break;
      }
    }
  }
  
  PositionFromKey(anBoard, ml.amMoves[ml.iMoveBest].auch);
}


void
EvaluatePositionFast(CONST int anBoard[2][25], float arOutput[])
{
  positionclass pc = ClassifyPosition(anBoard);

  acef[pc] (anBoard, arOutput /*, -1*/);

  SanityCheck(anBoard, arOutput);
}
 

static int
EvaluatePositionFull(CONST int anBoard[2][25], float arOutput[],
		     int nPlies, int wide,
		     int direction, float* p, unsigned int snp,
		     unsigned char* pauch)
{
  int i, n0, n1;
  positionclass const pc = ClassifyPosition(anBoard);

  // (FIXME) add this again after we get rid of reduced
  //{                                                        assert( p == 0 ); }
  
  if( (pc > CLASS_PERFECT || (p && snp > 0 && pc != CLASS_OVER))
      && nPlies > 0 ) {
    /* internal node; recurse */

    float ar[ NUM_OUTPUTS ];
    int anBoardNew[2][25];

    int nIncrp = NUM_OUTPUTS;
    if( p && snp > 1 ) {
      unsigned int k;
      for(k = 1; k < snp; ++k) {
	nIncrp *= 21;
      }
    }
    
    for( i = 0; i < NUM_OUTPUTS; i++ ) {
      arOutput[i] = 0.0;
    }
    
    
    for( n0 = 1; n0 <= 6; n0++ ) {
      for( n1 = 1; n1 <= n0; n1++ ) {
	for( i = 0; i < 25; i++ ) {
	  anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	  anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}

/*  	if( fInterrupt ) { */
/*  	  errno = EINTR; */
/*  	  return -1; */
/*  	} */

	if( preFilter && ! wide ) {
	  FindBestMoveInEval(n0, n1, anBoardNew, direction);
	} else {
	  FindBestMove(wide ? 1 : 0,
		       0, n0, n1, anBoardNew, 0 ,direction);
	}

	SwapSides( anBoardNew );

	if( EvaluatePosition( (ConstBoard)anBoardNew, ar, nPlies - 1, 0
			      ,!direction,
			      snp > 0 ? p : 0,
			      snp > 0 ? snp - 1 : 0,
			      snp > 0 ? pauch : 0) ) {
	  return -1;
	}

	if( p && snp > 0 ) {
	  p += nIncrp;
	  if( pauch ) {
	    pauch += nIncrp * (10/NUM_OUTPUTS);
	  }
	}

	if( n0 == n1 ) {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ];
	  }
	} else {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ] * 2.0;
	  }
	}
      }
    }

    arOutput[ OUTPUT_WIN ] = 1.0 - arOutput[ OUTPUT_WIN ] / 36.0;

    ar[ 0 ] = arOutput[ OUTPUT_WINGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEGAMMON ] = ar[ 0 ];
	
    ar[ 0 ] = arOutput[ OUTPUT_WINBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINBACKGAMMON ] =
      arOutput[ OUTPUT_LOSEBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = ar[ 0 ];

    if( p && snp == 0 ) {
      memcpy(p, arOutput, NUM_OUTPUTS * sizeof(*p));
      if( pauch ) {
	PositionKey((ConstBoard)anBoard, pauch);
      }
    }
    
  } else {
    /* at leaf node; use static evaluation */

    acef[pc] (anBoard, arOutput /*, -1*/);

    SanityCheck( anBoard, arOutput );

    if( p ) {
      if( snp == 0 ) {
	memcpy(p, arOutput, NUM_OUTPUTS * sizeof(*p));
	if( pauch ) {
	  PositionKey((ConstBoard)anBoard, pauch);
	}
      } else {
	if( pc == CLASS_OVER ) {
	  unsigned int k;
	  float* p0 = p;
	  unsigned char* pauch0 = pauch;
	  
	  memcpy(p0, arOutput, NUM_OUTPUTS * sizeof(*p));
	  
	  if( pauch ) {
	    assert(0);
/* 	    if( snp & 1 ) { */
/* 	      SwapSides(anBoard); */
/* 	      PositionKey((ConstBoard)anBoard, pauch); */
/* 	      SwapSides(anBoard); */
/* 	    } */
	  }

	  if( snp & 1 ) {
	    InvertEvaluation(p0);
	  }

	  for(k = 0; k < snp; ++k) {
	    for(i = 0; i < 21; ++i) {
	      memcpy(p, p0, NUM_OUTPUTS * sizeof(*p));
	      p += NUM_OUTPUTS;
	      
	      if( pauch ) {
		memcpy(pauch, pauch0, 10 * sizeof(*pauch));
		pauch += 10;
	      }
	    }
	  }
	}
      }
    }
  }

  return 0;
}

extern int
EvaluatePosition(CONST int anBoard[2][25], float arOutput[],
		 int nPlies, int wide, int direction,
		 float* p, unsigned int snp,
		 unsigned char* pauch)
{
  evalcache ec, *pec;
  unsigned long l = (unsigned long)-1;

  {                                       assert( !p || (int)snp <= nPlies ); }
	
  PositionKey(anBoard, ec.auchKey);

  // when using search shortcuts, note this in cache key for plies > 0
  ec.nPlies = (nPlies && preFilter) ?
    ((nets[CLASS_CONTACT].nMoves << 8) | nPlies) : (unsigned int)nPlies;

#if defined( GARY_CODE )
  l = EvalCacheHash( &ec );
    
  if( (!p || (p && snp == 0)) &&
      ( pec = CacheLookup( nets[CLASS_CONTACT].ncache, l, &ec ) ) )
#else
  if( (!p || (p && snp == 0)) &&
      ( pec = CacheLookup( nets[CLASS_CONTACT].ncache, &ec, &l ) ) )
    
#endif
    {
    memcpy( arOutput, pec->ar, sizeof( pec->ar ) );

    if( p && snp == 0 ) {
      memcpy(p, arOutput, NUM_OUTPUTS * sizeof(*p));
      if( pauch ) {
	memcpy(pauch, ec.auchKey, sizeof(ec.auchKey));
      }
    }

    return 0;
  }
    
  if( EvaluatePositionFull(anBoard, arOutput, nPlies, wide,
			   direction, p, snp, pauch) ) {
    return -1;
  }
  
  memcpy(ec.ar, arOutput, sizeof(ec.ar));

#if defined( GARY_CODE )
  return CacheAdd(nets[CLASS_CONTACT].ncache, l, &ec, sizeof(ec));
#else
  if( l != (unsigned long)-1 ) CacheAdd(nets[CLASS_CONTACT].ncache, &ec, l);
  return 0;
#endif
}

#include "eggmove.h"

void
EvaluatePositionPart(CONST int anBoard[2][25], int nPlies, float arOutput[])
{
  int i, n0, n1;
  positionclass pc;

  pc = ClassifyPosition(anBoard);
  
  if( pc != CLASS_OVER && nPlies > 0 ) {
    float ar[ NUM_OUTPUTS ];
    int bestMove[2][25];
    
    int race = pc <= CLASS_RACE;
      
    for(i = 0; i < NUM_OUTPUTS; i++) {
      arOutput[i] = 0.0;
    }
    
    for( n0 = 1; n0 <= 6; n0++ ) {
      for( n1 = 1; n1 <= n0; n1++ ) {

 	getPBMove(anBoard, race, bestMove, n0, n1);

	SwapSides(bestMove);

	EvaluatePositionPart((ConstBoard)bestMove, nPlies - 1, ar);

	if( n0 == n1 ) {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ];
	  }
	} else {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ] * 2.0;
	  }
	}
      }
    }

    arOutput[ OUTPUT_WIN ] = 1.0 - arOutput[ OUTPUT_WIN ] / 36.0;

    ar[ 0 ] = arOutput[ OUTPUT_WINGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEGAMMON ] = ar[ 0 ];
	
    ar[ 0 ] = arOutput[ OUTPUT_WINBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINBACKGAMMON ] =
      arOutput[ OUTPUT_LOSEBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = ar[ 0 ];

  } else {
    /* at leaf node; use static evaluation */

    acef[ pc ]( anBoard, arOutput /*, -1*/);

    SanityCheck( anBoard, arOutput );
  }
}

extern void
InvertEvaluation(float ar[NUM_OUTPUTS] )
{		
  float r;
    
  ar[ OUTPUT_WIN ] = 1.0 - ar[ OUTPUT_WIN ];
	
  r = ar[ OUTPUT_WINGAMMON ];
  ar[ OUTPUT_WINGAMMON ] = ar[ OUTPUT_LOSEGAMMON ];
  ar[ OUTPUT_LOSEGAMMON ] = r;
	
  r = ar[ OUTPUT_WINBACKGAMMON ];
  ar[ OUTPUT_WINBACKGAMMON ] = ar[ OUTPUT_LOSEBACKGAMMON ];
  ar[ OUTPUT_LOSEBACKGAMMON ] = r;
}

extern int
GameStatus(CONST int anBoard[2][25])
{
  float ar[NUM_OUTPUTS] = {0, 0, 0, 0, 0};
    
  if( ClassifyPosition( anBoard ) != CLASS_OVER )
    return 0;

  EvalOver(anBoard, ar /*, -1*/);

  if( ar[ OUTPUT_WINBACKGAMMON ] || ar[ OUTPUT_LOSEBACKGAMMON ] )
    return 3;

  if( ar[ OUTPUT_WINGAMMON ] || ar[ OUTPUT_LOSEGAMMON ] )
    return 2;

  return 1;
}

float*
NetInputs(CONST int anBoard[2][25], positionclass* pc, unsigned int* n)
{
  static float arInput[MAX_NUM_INPUTS];
  
  *pc = ClassifyPosition(anBoard);
  {
    EvalNets* i = &nets[*pc];

    if( ! i->net ) {
      int a = *pc;
      do {
	a = alternate[a];
	{                                                    assert( a >= 0 ); }
	i = &nets[a];
      } while( ! i->net );
    }
    
    *n = i->net->cInput;
    i->netInputs->func(anBoard, arInput);
  }
  
  return arInput;
}

void
NetEval(float* p, CONST int anBoard[2][25], positionclass pc, float* inputs)
{
  EvalNets* i = &nets[pc];

  NeuralNetEvaluate(i->net, inputs, p /*, NNEVAL_NONE*/);

  if( pc == CLASS_RACE ) {
    raceImprovement(anBoard, p);
  }
  
  SanityCheck(anBoard, p);
}

extern int
TrainPosition(CONST int anBoard[2][25], float arDesired[], float a,
	      CONST int* tList)
{
  float arInput[MAX_NUM_INPUTS], arOutput[NUM_OUTPUTS];

  int pc = ClassifyPosition(anBoard);
  
  neuralnet* nn = nets[pc].net;

  if( ! nn ) {
    int CONST a = alternate[pc];
    if( a >= 0 ) {
      pc = a;
      nn = nets[pc].net;
    }

    if( ! nn ) {
      errno = EDOM;
      return -1;
    }
  }
    
  SanityCheck(anBoard, arDesired);

  nets[pc].netInputs->func(anBoard, arInput);
  
  if( a < 0 ) {
    a = 2.0 / pow( 100.0 + nn->nTrained, 0.25 );
  }
  
  if( ! tList ) {
    NeuralNetTrain(nn, arInput, arOutput, arDesired, a);
  } else {
    NeuralNetTrainS(nn, arInput, arOutput, arDesired, a, tList);
  }

  return 0;
}


extern int
PruneTrainPosition(CONST int anBoard[2][25], float arDesired[], float a)
{
  float arInput[CEVAL_NUM_INPUTS], arOutput[NUM_OUTPUTS];

  int pc = ClassifyPosition(anBoard);
  
  neuralnet* pnn = nets[pc].pnet;

  if( ! pnn ) {
    errno = EDOM;
    return -1;
  }
    
  baseInputs(anBoard, arInput);
  
  {                                                          assert( a > 0 ); }
  
  NeuralNetTrain(pnn, arInput, arOutput, arDesired, a);

  return 0;
}


static int
ScoreMoves(movelist* pml, int nPlies, int direction)
{
  int i, anBoardTemp[2][25];
  float arEval[ NUM_OUTPUTS ];

  pml->rBestScore = -99999.9;

  for( i = 0; i < pml->cMoves; i++ ) {
    PositionFromKey( anBoardTemp, pml->amMoves[ i ].auch );

    SwapSides( anBoardTemp );
	
    if( EvaluatePosition( (ConstBoard)anBoardTemp, arEval, nPlies,
			  0, !direction,0,0,0) )
      return -1;
	
    InvertEvaluation( arEval );
	
    if( pml->amMoves[ i ].pEval )
      memcpy( pml->amMoves[ i ].pEval, arEval, sizeof( arEval ) );

    pml->amMoves[i].rScore =
      equityFunc ? (*equityFunc)(arEval, direction) : Utility(arEval);
	
    if( pml->amMoves[i].rScore > pml->rBestScore ) {
      pml->iMoveBest = i;
      pml->rBestScore = pml->amMoves[ i ].rScore;
    }
  }

  return 0;
}

#if defined( HACK1 )
static int
opCrashed(int anBoard[2][25])
{
  int i, t = 0;
  int* b = anBoard[0];
  
  for(i = 3; i < 25; ++i) {
    t += b[i];
  }
  
  if( t > 3 ) {
    return 0;
  }

  if( b[0] <= 1 ) {
    t += b[0] + b[1] + b[2];
  } else {
    if( b[1] <= 1 ) {
      t += b[1] + b[2];
    } else {
      t += (b[1] != 0) + (b[2] != 0);
    }
  }

  return t <= 3;
}

int hack1 = 0;

int
applicable(int anBoard[2][25])
{
  int i;
  
  if( ! opCrashed(anBoard) ) {
    return 0;
  }

  /* all in home -> no */
  
  for(i = 6; i < 25; ++i) {
    if( anBoard[1][i] ) {
      break;
    }
  }

  if( i == 25 ) {
    return 0;
  }

  if( ((anBoard[1][0] >= 2) + (anBoard[1][1] >= 2) +
       (anBoard[1][2] >= 2) + (anBoard[1][3] >= 2) +
       (anBoard[1][4] >= 2) + (anBoard[1][5] >= 2)) >= 4 ) {
    return 0;
  }

  {
    int na = ((anBoard[1][0] >= 2) + (anBoard[1][1] >= 2) +
	      (anBoard[1][2] >= 2));
    
    for(i = 3; i < 24; ++i) {
      na += (anBoard[1][i] >= 2);

      if( na == 4 ) {
	return 0;
      }
      
      na -= (anBoard[1][i-3] >= 2);
    }

  }

  for(i = 24; i > 0; --i) {
    if( anBoard[0][i] != 0 ) {
      break;
    }
  }

  {
    int n = 0;
    int j;
    for(j = 24; j >= 24 - i && j > 0; --j) {
      n += anBoard[1][j];
    }

    if( n < 6 ) {
      return 0;
    }
  }

  
  return 1;
}

static int
countContained(int* bcr, int* bco)
{
  int n, p, i, lastChecker;

  for(lastChecker = 24; lastChecker >= 0; --lastChecker) {
    if( bco[lastChecker] != 0 ) {
      break;
    }
  }

  lastChecker = 24 - lastChecker;
  
  n = 0;
  p = 0;

  for(i = 24; i >= lastChecker && i > 0; --i) {
    if( (i >= 3) && bcr[i] ) {
      n += bcr[i]; // (bcr[i] == 1);
      //p += (lastChecker == 0  ? i : ((i - lastChecker) + 1)) * bcr[i];
/*        if( i > 6 ) { */
/*  	p += bcr[i]; */
/*        } */
    }
  }

  for(lastChecker = 24; lastChecker >= 0; --lastChecker) {
    if( bcr[lastChecker] != 0 ) {
      break;
    }
  }

  lastChecker = 24 - lastChecker;

  for(i = 23; i >= lastChecker && i > 2; --i) {
    if( bco[i] >= 2 ) {
      ++p;
    }
  }

  return 10 * n + (p > 6 ? 6 : p);
}

static float
avgEscapeRolls(int anBoard[2][25])
{
  int d0, d1, i;
  //int lastChecker;
  float e = 0.0;
  int b[2][25];

  movelist ml;
  
  int nInitial = countContained(anBoard[1], anBoard[0]);

  if( nInitial == 0 ) {
    return 1.0;
  }
  
  for(d0 = 1; d0 <= 6; ++d0) {
    for(d1 = d0; d1 <= 6; ++d1) {
      int nContained = nInitial;
      
      GenerateMoves(&ml, anBoard, d0, d1);
      
      for(i = 0; (int)i < ml.cMoves; i++) {
	PositionFromKey(b, ml.amMoves[i].auch);
	
	nContained = min(countContained(b[1], b[0]), nContained);
      }

      e += nContained * ((d0 == d1) ? 1.0/36.0 : 2.0/36.0);
      
      //assert( nContained <= nInitial );
      
      //if( nContained < nInitial ) {
      //e += (((1.0 * (nInitial - nContained)) / nInitial) *
      //((d0 == d1) ? 1.0/36.0 : 2.0/36.0));
      //}
    }
  }
  return e;
}

void
filter(movelist* pml)
{
  int i, anBoard[2][25];

  static move* am = 0;
  int nMoves = pml->cMoves;
  
  am = realloc(am, nMoves * sizeof(*am));
  memcpy(am, pml->amMoves, nMoves * sizeof(*am));
  
  pml->rBestScore = -99999.9;

  for(i = 0; i < nMoves; ++i) {
    PositionFromKey(anBoard, am[i].auch);

    SwapSides(anBoard);

    am[i].rScore = avgEscapeRolls(anBoard);
    
    if( am[i].rScore > pml->rBestScore ) {
      pml->iMoveBest = i;
      pml->rBestScore = am[i].rScore;
    }
  }

  pml->cMoves = 0;
  for(i = 0; i < nMoves; ++i) { 
    if( am[i].rScore >= pml->rBestScore ) {
      pml->amMoves[pml->cMoves] = am[i];
      ++ pml->cMoves;
    }
  }
}

void
hackFilter(int anBoard[2][25], movelist* pml)
{
  if( hack1 && applicable(anBoard) ) {
    filter(pml);
  }
}

#endif


extern int
FindBestMove(unsigned int nPlies, int anMove[8], int nDice0, int nDice1,
	     int anBoard[2][25], unsigned int nc, int direction)
{
  unsigned int i /*,j, iPly*/;
  movelist ml;
  move amCandidates[ SEARCH_CANDIDATES ];

  if( nc == 0 ) {
    nc = SEARCH_CANDIDATES;
  }
  
  /* FIXME return -1 and do not change anBoard on interrupt */
    
  if( anMove ) {
    for( i = 0; i < 8; i++ ) {
      anMove[ i ] = -1;
    }
  }

  GenerateMoves( &ml, (ConstBoard)anBoard, nDice0, nDice1 );
    
  if( !ml.cMoves ) {
    /* no legal moves */
    return 0;
  }
  else if( ml.cMoves == 1 ) {
    /* forced move */
    ml.iMoveBest = 0;
  } else {

#if defined( HACK1 )
    hackFilter(anBoard, &ml);
#endif
    
    /* choice of moves */
    
    if( ScoreMoves(&ml, 0, direction) )
      return -1;

    qsort( ml.amMoves, ml.cMoves, sizeof( move ), (cfunc) CompareMoves );
    ml.iMoveBest = 0;
    
    if( nPlies > 0 ) {
      int keep = nPlies == 1 ? 8 : 4;

      ml.cMoves = min(keep, ml.cMoves);

      if( ml.amMoves != amCandidates ) {
	memcpy( amCandidates, ml.amMoves, ml.cMoves * sizeof( move ) );
	    
	ml.amMoves = amCandidates;
      }

      if( ScoreMoves( &ml, nPlies, direction ) ) {
	return -1;
      }
    }
  }

  if( anMove ) {
    move* bm = &ml.amMoves[ ml.iMoveBest ];

    for( i = 0; (int)i < bm->cMoves; i++ ) {
      anMove[2*i] = bm->anMove[2*i];
      anMove[2*i+1] = bm->anMove[2*i+1];
    }
    ml.cMaxMoves =  bm->cMoves;
  }
	
  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  return ml.cMaxMoves * 2;
}

extern int
FindBestMoves( movelist *pml, float ar[][ NUM_OUTPUTS ], int nPlies,
	       int nDice0, int nDice1, CONST int anBoard[2][25],
	       int c, float d)
{
  int i, j;
  int k;
  static move amCandidates[ 32 ];

  if( c > 32 )
    c = 32;
    
  GenerateMoves( pml, anBoard, nDice0, nDice1 );

  if( ScoreMoves( pml, 0, 0/*fixme*/) )
    return -1;

  for( i = 0, j = 0; i < pml->cMoves; i++ )
    if( pml->amMoves[ i ].rScore >= pml->rBestScore - d ) {
      if( j == 32 ) {
	/* list is filled, find an exisiting entry with lower equity and */
	/* replace it. If not found just ignore entry, since it will     */
	/* never be considered anyway ('c < 32' always)                  */
	    
	float e = pml->amMoves[ i ].rScore;
	    
	/* find an exisiting entry with lower equity and replace it */
	    
	for(k = 0; k < 32; ++k) {
	  if( e < pml->amMoves[ k ].rScore ) {
	    /* preserve pEval */
	    pml->amMoves[ i ].pEval = pml->amMoves[ k ].pEval;
		
	    /* copy over everyting else */
	    pml->amMoves[ k ] = pml->amMoves[ i ];
	    break;
		
	    /* has pEval already*/
	  }
	}
      } else {
	/* still in range */
	if( i != j )
	  pml->amMoves[ j ] = pml->amMoves[ i ];
		    
	pml->amMoves[ j ].pEval = ar[ j ]; j++ ;
      }
    }
	    
  qsort( pml->amMoves, j, sizeof( move ), (cfunc) CompareMoves );

  pml->iMoveBest = 0;

  pml->cMoves = ( j < c ) ? j : c;
    
  memcpy( amCandidates, pml->amMoves, pml->cMoves * sizeof( move ) );    
  pml->amMoves = amCandidates;

  if( nPlies ) {
    if( ScoreMoves( pml, nPlies ,0 /*fixme*/ ) )
      return -1;
	
    qsort( pml->amMoves, pml->cMoves, sizeof( move ),
	   (cfunc) CompareMoves );
  }

  return 0;
}

extern int
EvaluatePositionToBO(CONST int anBoard[2][25], float arOutput[],
		     int direction);

static int
EvaluatePositionFullToBO(CONST int anBoard[2][25], float arOutput[],
			 int direction)
{
  int i, n0, n1;
  positionclass pc = ClassifyPosition(anBoard);

  if( pc >= CLASS_RACE ) {

    /* internal node; recurse */

    float ar[ NUM_OUTPUTS ];
    int anBoardNew[2][25];

    for( i = 0; i < NUM_OUTPUTS; i++ ) {
      arOutput[i] = 0.0;
    }
    
    
    for( n0 = 1; n0 <= 6; n0++ ) {
      for( n1 = 1; n1 <= n0; n1++ ) {
	for( i = 0; i < 25; i++ ) {
	  anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	  anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}

	if( preFilter ) {
	  FindBestMoveInEval(n0, n1, anBoardNew, direction);
	} else {
	  FindBestMove(0, 0, n0, n1, anBoardNew, 0 ,direction);
	}

	SwapSides(anBoardNew);

	if( EvaluatePositionToBO((ConstBoard)anBoardNew, ar,!direction) ) {
	  return -1;
	}

	if( n0 == n1 ) {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ];
	  }
	} else {
	  for( i = 0; i < NUM_OUTPUTS; i++ ) {
	    arOutput[ i ] += ar[ i ] * 2.0;
	  }
	}
      }
    }

    arOutput[ OUTPUT_WIN ] = 1.0 - arOutput[ OUTPUT_WIN ] / 36.0;

    ar[ 0 ] = arOutput[ OUTPUT_WINGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEGAMMON ] = ar[ 0 ];
	
    ar[ 0 ] = arOutput[ OUTPUT_WINBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_WINBACKGAMMON ] =
      arOutput[ OUTPUT_LOSEBACKGAMMON ] / 36.0;
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = ar[ 0 ];
    
  } else {
    /* at leaf node; use static evaluation */

    acef[pc] (anBoard, arOutput /*, -1*/);

    SanityCheck( anBoard, arOutput );
  }

  return 0;
}

extern int
EvaluatePositionToBO(CONST int anBoard[2][25], float arOutput[], int direction)
{
  evalcache ec, *pec;
  unsigned long l;

  PositionKey((ConstBoard)anBoard, ec.auchKey);

  ec.nPlies = 0xff << 8;

#if !defined( GARY_CODE )
  if( (pec = CacheLookup( nets[CLASS_CONTACT].ncache, &ec, &l)) ) {

    memcpy( arOutput, pec->ar, sizeof( pec->ar ) );

    return 0;
  }
#endif
    
  EvaluatePositionFullToBO(anBoard, arOutput, direction);
  
  memcpy(ec.ar, arOutput, sizeof(ec.ar));

#if !defined( GARY_CODE )
  CacheAdd(nets[CLASS_CONTACT].ncache, &ec, l);
#endif
  return 0;
}

extern float
playsToRace(CONST int anBoard[2][25], int cGames)
{
  int anBoardEval[2][25], anDice[2], iTurn;
  int iGame, nm;
  
  positionclass pc;

  float out = 0.0;

  for(iGame = 0; iGame < cGames; ++iGame) {
    int count = 0;
    
    memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	    sizeof( anBoardEval ) );

    for(iTurn = 0;
	((pc = ClassifyPosition((ConstBoard)anBoardEval) ) > CLASS_RACE);
	++iTurn) {
    
      if( iTurn == 0 && (cGames % 36) == 0 ) {
	anDice[ 0 ] = ( iGame % 6 ) + 1;
	anDice[ 1 ] = ( ( iGame / 6 ) % 6 ) + 1;
      } else if( iTurn == 1 && ( cGames % 1296 ) == 0 ) {
	anDice[ 0 ] = ( ( iGame / 36 ) % 6 ) + 1;
	anDice[ 1 ] = ( ( iGame / 216 ) % 6 ) + 1;
      } else
	RollDice( anDice );
	
      if( anDice[0] < anDice[1] ) {
	swap( anDice, anDice + 1 );
      }

      nm = FindBestMove(0, NULL, anDice[0], anDice[1], anBoardEval, 0
			,0/*fixme*/);

      if( (iTurn & 0x1) == 0 && nm > 0 ) {
	++count;
      }
      
      SwapSides(anBoardEval);
    }

    out += count;
  }
    
  return out / cGames;
}


extern int
EvalCacheStats(unsigned int* pc, unsigned int* pcLookup, unsigned int* pcHit)
{
  cache* c = nets[CLASS_CONTACT].ncache;
  *pc = c->nAdds;
    
  CacheStats( c, pcLookup, pcHit );
  return 0;
}

static Stats stats[2 * N_CLASSES];

extern Stats*
netStats(void)
{
  int k;
  for(k = 0; k < N_CLASSES; ++k) {
    unsigned int i = 2*k;
    
    if( nets[k].net ) {
      stats[i].name = nets[k].name;
      stats[i].nEvals = nets[k].net->nEvals;
      
      if( nets[k].ncache ) {
	CacheStats( nets[k].ncache, &stats[i].lookUps, &stats[i].hits );
      } else {
	stats[i].lookUps = stats[i].hits = 0;
      }

    } else {
      stats[i].name = 0;
    }
      
    stats[i+1].name = 0;
    
    if( nets[k].pnet ) {
      stats[i+1].nEvals = nets[k].pnet->nEvals;

      if( nets[k].pcache ) {
	CacheStats( nets[k].pcache, &stats[i+1].lookUps, &stats[i+1].hits );
      } else {
	stats[i+1].lookUps = stats[i+1].hits = 0;
      }
    }
  }
  return stats;
}

extern void
EvalCacheFlush(void)
{
  int k;
  for(k = 0; k < N_CLASSES; ++k) {
    if( nets[k].net ) {
      nets[k].net->nEvals = 0;
    }
    if( nets[k].pnet ) {
      nets[k].pnet->nEvals = 0;
    }
      
    if( nets[k].pcache ) {
      CacheFlush( nets[k].pcache );
    }
    if( nets[k].ncache ) {
      CacheFlush( nets[k].ncache );
    }
  }
}

extern void
EvalCacheResize(int cSize)
{
  int k;
  for(k = 0; k < N_CLASSES; ++k) {
    if( nets[k].pcache ) {
      CacheResize( nets[k].pcache, cSize );
    }
    if( nets[k].ncache ) {
      CacheResize( nets[k].ncache, cSize );
    }
  }
}


extern int
FindPubevalMove(int nDice0, int nDice1, int anBoard[2][25], int anMove[8])
{
  movelist ml;
  int i, anBoardTemp[2][25], fRace;

  fRace = ClassifyPosition( (ConstBoard)anBoard ) <= CLASS_RACE;
    
  GenerateMoves( &ml, (ConstBoard)anBoard, nDice0, nDice1 );
    
  if( !ml.cMoves )
    /* no legal moves */
    return 0;
  else if( ml.cMoves == 1 )
    /* forced move */
    ml.iMoveBest = 0;
  else {
    /* choice of moves */
    ml.rBestScore = -99999.9;

    for( i = 0; i < ml.cMoves; i++ ) {
      PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );
      SwapSides( anBoardTemp );
      
      ml.amMoves[i].rScore = pubEvalVal(fRace, anBoardTemp);
      
      if( ml.amMoves[ i ].rScore > ml.rBestScore ) {
	ml.iMoveBest = i;
	ml.rBestScore = ml.amMoves[ i ].rScore;
      }
    }
  }
	
  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  if( anMove ) {
    move* bm = &ml.amMoves[ ml.iMoveBest ];

    for( i = 0; (int)i < bm->cMoves; i++ ) {
      anMove[2*i] = bm->anMove[2*i];
      anMove[2*i+1] = bm->anMove[2*i+1];
    }
    ml.cMaxMoves = bm->cMoves;
  }

  return ml.cMaxMoves * 2;
}


extern int
neuralNetInit(positionclass pc, const char* inputFuncName, int nHidden)
{
  int ni;
  neuralnet* nn = nets[pc].net;

  int nHid = nHidden <= 0 ? 128 : nHidden;

  const NetInputFuncs* p = 0;
  
  if( inputFuncName ) {
    p = ifByName(inputFuncName);

    if( ! p ) {
#if defined( HAVE_DLFCN_H )
      p = ifFromFile(inputFuncName);
#endif

      if( ! p ) {
	return 0;
      }
    }

    ni = p->nInputs;
  } else {
    if( ! evalNetsGen[pc].netInputs ) {
      return 0;
    }
    
    ni = evalNetsGen[pc].netInputs->nInputs;
  }

  if( ! (p || evalNetsGen[pc].netInputs) ) {
    return 0;
  }
  
  if( nn ) {
    NeuralNetDestroy(nn);
  } else {
    nn = malloc(sizeof(neuralnet));
  }

  if( NeuralNetCreate(nn, ni, nHid, NUM_OUTPUTS, 0.1, 1.0 ) < 0 ) {
    return 0;
  }

  nets[pc].net = nn;

  if( p ) {
    nets[pc].netInputs = p;
  } else {
    nets[pc].netInputs = evalNetsGen[pc].netInputs;
  }
    
  return 1;
}

extern int
neuralNetInitPrune(positionclass pc, int nHidden)
{
  neuralnet* nn = nets[pc].pnet;
  int nHid = nHidden <= 0 ? 5 : nHidden;

  if( nn ) {
    NeuralNetDestroy(nn);
  } else {
    nn = malloc(sizeof(neuralnet));
  }

  if( NeuralNetCreate(nn, 200, nHid, NUM_OUTPUTS, 0.1, 1.0 ) < 0 ) {
    return 0;
  }
  
  nets[pc].pnet = nn;

  return 1;
}

CONST char*
inputNameByClass(positionclass pc, unsigned int k)
{
  if( ! nets[pc].netInputs ) {
    return "";
  }

  return inputNameFromFunc(nets[pc].netInputs, k);
}

CONST char*
inputNameByFunc(CONST char* inpFunc, unsigned int k)
{
  const NetInputFuncs* p = ifByName(inpFunc);

  if( ! p ) {
    return 0;
  }

  if( ! (k < p->nInputs) ) {
    return 0;
  }

  return p->ifunc(k);
}
