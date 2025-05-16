/*
 * bearoffdb.c
 * Adapted from:
 *
 * bearoff.c
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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
 * $Id: bearoffdb.c,v 1.9 2012/07/05 00:34:41 joseph Exp $
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#if defined( OS_BEAROFF_DB )

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#if defined(_WIN32)
  /* Windows: use <io.h> instead of unistd.h, and map the low-level I/O calls */
  #include <io.h>          /* for _open, _read, _lseek, _close */
  #define open   _open
  #define read   _read
  #define lseek  _lseek
  #define close  _close
#else
  /* Unix/macOS */
  #include <unistd.h>
  #include <sys/mman.h>
#endif
#include <errno.h>
#include <string.h>

#include <math.h>

#include <sys/stat.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0
#define _(x) x

#include "bearoffdb.h"
#include "positionid.h"
#include "eval.h"
#include "bearoffgammon.h"

#ifdef WIN32
#define BINARY O_BINARY
#else
#define BINARY 0
#endif

/* Strip off GCC-only __attribute__ on MSVC */
#if !defined(__GNUC__)
  #define __attribute__(x)  /* nothing */
#endif

static int anCombination[ 33 ][ 18 ];
static int fCalculated = 0;

static int
InitCombination( void )
{
  int i, j;

  for( i = 0; i < 33; i++ )
    anCombination[ i ][ 0 ] = i + 1;
    
  for( j = 1; j < 18; j++ )
    anCombination[ 0 ][ j ] = 0;

  for( i = 1; i < 33; i++ )
    for( j = 1; j < 18; j++ )
      anCombination[ i ][ j ] = anCombination[ i - 1 ][ j - 1 ] +
	anCombination[ i - 1 ][ j ];

  fCalculated = 1;
    
  return 0;
}

static int
Combination( int n, int r )
{
  assert( n > 0 );
  assert( r > 0 );
  assert( n <= 33 );
  assert( r <= 18 );

  if( !fCalculated )
    InitCombination();

  return anCombination[ n - 1 ][ r - 1 ];
}

static int
PositionF( int fBits, int n, int r )
{
  if( n == r )
    return 0;

  return ( fBits & ( 1 << ( n - 1 ) ) ) ? Combination( n - 1, r ) +
    PositionF( fBits, n - 1, r - 1 ) : PositionF( fBits, n - 1, r );
}

#define PositionBearoff localPositionBearoff
static unsigned int
PositionBearoff( int CONST anBoard[], int CONST n )
{
  int i, fBits, j;

  for( j = n - 1, i = 0; i < n; i++ )
    j += anBoard[ i ];

  fBits = 1 << j;
    
  for( i = 0; i < n - 1; i++ ) {
    j -= anBoard[ i ] + 1;
    fBits |= ( 1 << j );
  }

  return PositionF( fBits, 15 + n, n );
}

static void
CopyBytes ( unsigned short int aus[ 64 ],
            const unsigned char ac[ 128 ],
            const unsigned int nz,
            const unsigned int ioff,
            const unsigned int nzg,
            const unsigned int ioffg ) {

  unsigned int i, j;

  i = 0;
  memset ( aus, 0, 64 * sizeof ( unsigned short int ) );
  for ( j = 0; j < nz; ++j, i += 2 )
    aus[ ioff + j ] = ac[ i ] | ac[ i + 1 ] << 8;

  for ( j = 0; j < nzg; ++j, i += 2 ) 
    aus[ 32 + ioffg + j ] = ac[ i ] | ac[ i + 1 ] << 8;

}

static int isExactBearoff(char* name) {
  (void)name;
  return 0;
}

typedef struct _hashentryonesided {
  unsigned int nPosID;
  unsigned short int aus[ 64 ];
} hashentryonesided;

typedef struct _hashentrytwosided {
  unsigned int nPosID;
  unsigned char auch[ 8 ];
} hashentrytwosided;

static int
hcmpOneSided( void *p1, void *p2 ) {

  hashentryonesided *ph1 = p1;
  hashentryonesided *ph2 = p2;

  if ( ph1->nPosID < ph2->nPosID )
    return -1;
  else if ( ph1->nPosID == ph2->nPosID )
    return 0;
  else
    return 1;

}

static unsigned long
HashOneSided ( const unsigned int nPosID ) {
  return nPosID;
}


static void 
AverageRolls ( const float arProb[ 32 ], float *ar )
{
  float sx;
  float sx2;
  int i;

  sx = sx2 = 0.0f;

  for ( i = 1; i < 32; ++i ) {
    sx += i * arProb[ i ];
    sx2 += i * i * arProb[ i ];
  }

  ar[ 0 ] = sx;
  ar[ 1 ] = sqrt ( sx2 - sx *sx );
}

static unsigned short int *
GetDistCompressed ( bearoffcontext *pbc, const unsigned int nPosID ) {

  unsigned char *puch;
  unsigned char ac[ 128 ];
  off_t iOffset;
  int nBytes;
  int nPos = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  static unsigned short int aus[ 64 ];
      
  unsigned int ioff, nz, ioffg, nzg;

  /* find offsets and no. of non-zero elements */
  
  if ( pbc->fInMemory )
    /* database is in memory */
    puch = ( (unsigned char *) pbc->p ) + 40 + nPosID * 8;
  else {
    /* read from disk */
    if ( lseek ( pbc->h, 40 + nPosID * 8, SEEK_SET ) < 0 ) {
      perror ( "OS bearoff database" );
      return NULL;
    }
    
    if ( read ( pbc->h, ac, 8 ) < 8 ) {
      if ( errno )
        perror ( "OS bearoff database" );
      else
        fprintf ( stderr, "error reading OS bearoff database" );
      return NULL;
    }

    puch = ac;
  }
    
  /* find offset */
  
  iOffset = 
    puch[ 0 ] | 
    puch[ 1 ] << 8 |
    puch[2] << 16 |
    puch[ 3 ] << 24;
  
  nz = puch[ 4 ];
  ioff = puch[ 5 ];
  nzg = puch[ 6 ];
  ioffg = puch[ 7 ];

  /* read prob + gammon probs */
  
  iOffset = 40     /* the header */
    + nPos * 8     /* the offset data */
    + 2 * iOffset; /* offset to current position */
  
  /* read values */
  
  nBytes = 2 * ( nz + nzg );
  
  /* get distribution */
  
  if ( pbc->fInMemory )
    /* from memory */
    puch = ( ( unsigned char *) pbc->p ) + iOffset;
  else {
    /* from disk */
    if ( lseek ( pbc->h, iOffset, SEEK_SET ) < 0 ) {
      perror ( "OS bearoff database" );
      return NULL;
    }
    
    if ( read ( pbc->h, ac, nBytes ) < nBytes ) {
      if ( errno )
        perror ( "OS bearoff database" );
      else
        fprintf ( stderr, "error reading OS bearoff database" );
      return NULL;
    }

    puch = ac;

  }
    
  CopyBytes ( aus, puch, nz, ioff, nzg, ioffg );

  return aus;
}


static unsigned short int *
GetDistUncompressed ( bearoffcontext *pbc, const unsigned int nPosID ) {

  unsigned char ac[ 128 ];
  static unsigned short int aus[ 64 ];
  unsigned char *puch;
  int iOffset;

  /* read from file */

  iOffset = 40 + 64 * nPosID * ( pbc->fGammon ? 2 : 1 );

  if ( pbc->fInMemory ) {
    /* from memory */
    puch = ( ( unsigned char *) pbc->p ) + iOffset;
  } else {
    /* from disk */

    lseek ( pbc->h, iOffset, SEEK_SET );
    int const o = read ( pbc->h, ac, pbc->fGammon ? 128 : 64 );
    puch = ac;
  }

  CopyBytes ( aus, puch, 32, 0, 32, 0 );

  return aus;

}

#if defined(_WIN32)
  #define HAVE_MMAP 0
#else
  #define HAVE_MMAP 1
#endif

static int
ReadIntoMemory ( bearoffcontext *pbc, const int iOffset, const int nSize ) {

  pbc->fMalloc = TRUE;

#if HAVE_MMAP && !defined(_WIN32)
  if ( ( pbc->p = mmap ( NULL, nSize, PROT_READ, 
                           MAP_SHARED, pbc->h, iOffset ) ) == (void *) -1 ) {
    /* perror ( "mmap" ); */
    /* mmap failed */
#endif /* HAVE_MMAP */

    /* allocate memory for database */

    if ( ! ( pbc->p = malloc ( nSize ) ) ) {
      perror ( "pbc->p" );
      return -1;
    }

    if ( lseek( pbc->h, iOffset, SEEK_SET ) == (off_t) -1 ) {
      perror ( "lseek" );
      return -1;
    }

    if ( read ( pbc->h, pbc->p, nSize ) < nSize ) {
      if ( errno )
        perror ( "read failed" );
      else
        fprintf ( stderr, _("incomplete bearoff database\n"
                            "(expected size: %d)\n"),
                  nSize );
      free ( pbc->p );
      pbc->p = NULL;
      return -1;
    }

#if HAVE_MMAP
  }
  else
    pbc->fMalloc = FALSE;
#endif /* HAVE_MMAP */

  return 0;
}

/*
 * Initialise bearoff database
 *
 * Input:
 *   szFilename: the filename of the database to open
 *
 * Returns:
 *   pointer to bearoff context on succes; NULL on error
 *
 * Garbage collect:
 *   caller must free returned pointer if not NULL.
 *
 *
 */

extern bearoffcontext *
BearoffInit ( const char* szFilename, const int bo ) {

  bearoffcontext *pbc;
  char sz[ 41 ];
  int nSize = -1;
  int iOffset = 0;

  if ( bo & BO_HEURISTIC ) {
    assert(0);
  }

  if ( ! szFilename || ! *szFilename )
    return NULL;

  /*
   * Allocate memory for bearoff context
   */

  if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {
    /* malloc failed */
    perror ( "bearoffcontext" );
    return NULL;
  }

  pbc->fInMemory = bo & BO_IN_MEMORY;

  /*
   * Open bearoff file
   */

  if( ( pbc->h = open( szFilename, O_RDONLY | BINARY) ) < 0 ) {
  
    // if ( ( pbc->h = PathOpen ( szFilename, szDir, BINARY ) ) < 0 ) {
    /* open failed */
    free ( pbc );
    return NULL;
  }


  /*
   * Read header bearoff file
   */

  /* read header */

  if ( read ( pbc->h, sz, 40 ) < 40 ) {
    if ( errno )
      perror ( szFilename );
    else {
      fprintf ( stderr, _("%s: incomplete bearoff database\n"), szFilename );
      close ( pbc->h );
    }
    free ( pbc );
    return NULL;
  }

  /* detect bearoff program */

  if ( ! strncmp ( sz, "gnubg", 5 ) )
    pbc->bt = BEAROFF_GNUBG;
  else if ( isExactBearoff ( sz ) )
    pbc->bt = BEAROFF_EXACT_BEAROFF;
  else
    pbc->bt = BEAROFF_UNKNOWN;

  switch ( pbc->bt ) {

  case BEAROFF_GNUBG:

    /* one sided or two sided? */

    if ( ! strncmp ( sz + 6, "TS", 2 ) ) 
      pbc->fTwoSided = TRUE;
    else if ( ! strncmp ( sz + 6, "OS", 2 ) )
      pbc->fTwoSided = FALSE;
    else {
      fprintf ( stderr,
                _("%s: incomplete bearoff database\n"
                  "(type is illegal: '%2s')\n"),
                szFilename, sz + 6 );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    /* number of points */

    pbc->nPoints = atoi ( sz + 9 );
    if ( pbc->nPoints < 1 || pbc->nPoints >= 24 ) {
      fprintf ( stderr, 
                _("%s: incomplete bearoff database\n"
                  "(illegal number of points is %d)"), 
                szFilename, pbc->nPoints );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    /* number of chequers */

    pbc->nChequers = atoi ( sz + 12 );
    if ( pbc->nPoints < 1 || pbc->nPoints > 15 ) {
      fprintf ( stderr, 
                _("%s: incomplete bearoff database\n"
                  "(illegal number of chequers is %d)"), 
                szFilename, pbc->nChequers );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    pbc->fCompressed = FALSE;
    pbc->fGammon = FALSE;
    pbc->fCubeful = FALSE;
    pbc->fND = FALSE;
    pbc->fHeuristic = FALSE;

    if ( pbc->fTwoSided ) {
      /* options for two-sided dbs */
      pbc->fCubeful = atoi ( sz + 15 );
    }
    else {
      /* options for one-sided dbs */
      pbc->fGammon = atoi ( sz + 15 );
      pbc->fCompressed = atoi ( sz + 17 );
      pbc->fND = atoi ( sz + 19 );
    }

    iOffset = 0;
    nSize = -1;

    /*
    fprintf ( stderr, "Database:\n"
              "two-sided: %d\n"
              "points   : %d\n"
              "chequers : %d\n"
              "compress : %d\n"
              "gammon   : %d\n"
              "cubeful  : %d\n"
              "normaldis: %d\n"
              "in memory: %d\n",
              pbc->fTwoSided,
              pbc->nPoints, pbc->nChequers,
              pbc->fCompressed, pbc->fGammon, pbc->fCubeful, pbc->fND,
              pbc->fInMemory );*/
    
    break;

  case BEAROFF_EXACT_BEAROFF: 
    {
      long l, m;
      
      l = ( sz[ 8 ] | sz[ 9 ] << 8 | sz[ 10 ] << 16 | sz[ 11 ] << 24 );
      m = ( sz[ 12 ] | sz[ 13 ] << 8 | sz[ 14 ] << 16 | sz[ 15 ] << 24 );
      printf ( "nBottom %ld\n", l );
      printf ( "nTop %ld\n", m );

      if ( m != l ) {
        fprintf ( stderr, _("GNU Backgammon can only read ExactBearoff "
                            "databases with an\n"
                            "equal number of chequers on both sides\n"
                            "(read: botton %ld, top %ld)\n"),
                  l, m );
      }

      if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {
        /* malloc failed */
        perror ( "bearoffcontext" );
        return NULL;
      }

      pbc->bt = BEAROFF_EXACT_BEAROFF;
      pbc->fInMemory = TRUE;
      pbc->nPoints = 6;
      pbc->nChequers = l;
      pbc->fTwoSided = TRUE;
      pbc->fCompressed = FALSE;
      pbc->fGammon = FALSE;
      pbc->fND = FALSE;
      pbc->fCubeful = TRUE;
      pbc->nReads = 0;
      pbc->fHeuristic = FALSE;
      pbc->fMalloc = FALSE;
      pbc->p = NULL;

      iOffset = 0;
      nSize = -1;
    
    }
    break;

  default:

    fprintf ( stderr,
              _("Unknown bearoff database\n" ) );

    close ( pbc->h );
    free ( pbc );
    return NULL;

  }

  /* 
   * read database into memory if requested 
   */

  if ( pbc->fInMemory ) {

    if ( nSize < 0 ) {
      struct stat st;
      if ( fstat ( pbc->h, &st ) ) {
        perror ( szFilename );
        close ( pbc->h );
        free ( pbc );
        return NULL;
      }
      nSize = st.st_size;
    }
    
    if ( ReadIntoMemory ( pbc, iOffset, nSize ) ) {
      
      close ( pbc->h );
      free ( pbc );
      return NULL;

    }
    
    close ( pbc->h );
    pbc->h = -1;
    
  }


  /* create cache */

  if ( ! pbc->fInMemory ) {
    if ( ! ( pbc->ph = (hash *) malloc ( sizeof ( hash ) ) ) ||
         HashCreate ( pbc->ph, ( pbc->fTwoSided ? 100000 : 10000 ), 
                      ( /*pbc->fTwoSided ? hcmpTwoSided : */hcmpOneSided ) ) < 0 )
      pbc->ph = NULL;
  }
  else
    pbc->ph = NULL;
  
  pbc->nReads = 0;
  
  return pbc;

}

static void
AssignOneSided ( float arProb[ 32 ], float arGammonProb[ 32 ],
                 float ar[ 4 ],
                 unsigned short int ausProb[ 32 ], 
                 unsigned short int ausGammonProb[ 32 ],
                 const unsigned short int ausProbx[ 32 ],
                 const unsigned short int ausGammonProbx[ 32 ] ) {

  int i;
  float arx[ 64 ];

  if ( ausProb )
    memcpy ( ausProb, ausProbx, 32 * sizeof ( ausProb[0] ) );

  if ( ausGammonProb )
    memcpy ( ausGammonProb, ausGammonProbx, 32 * sizeof ( ausGammonProbx[0] ));

  if ( ar || arProb || arGammonProb ) {
    for ( i = 0; i < 32; ++i ) 
      arx[ i ] = ausProbx[ i ] / 65535.0f;
    
    for ( i = 0; i < 32; ++i ) 
      arx[ 32 + i ] = ausGammonProbx[ i ] / 65535.0f;
  }

  if ( arProb )
    memcpy ( arProb, arx, 32 * sizeof ( float ) );
  if ( arGammonProb )
    memcpy ( arGammonProb, arx + 32, 32 * sizeof ( float ) );

  if ( ar ) {
    AverageRolls ( arx, ar );
    AverageRolls ( arx + 32, ar + 2 );
  }

}

static void
ReadBearoffOneSidedExact ( bearoffcontext *pbc, const unsigned int nPosID,
                           float arProb[ 32 ], float arGammonProb[ 32 ],
                           float ar[ 4 ],
                           unsigned short int ausProb[ 32 ], 
                           unsigned short int ausGammonProb[ 32 ] ) {

  unsigned short int *pus = NULL;

  /* look in cache */

  if ( ! pbc->fInMemory && pbc->ph ) {
    
    hashentryonesided *phe;
    hashentryonesided he;
    
    he.nPosID = nPosID;
    if ( ( phe = HashLookup ( pbc->ph, HashOneSided ( nPosID ), &he ) ) ) 
      pus = phe->aus;

  }

  /* get distribution */
  if ( ! pus ) {
    if ( pbc->fCompressed )
      pus = GetDistCompressed ( pbc, nPosID );
    else
      pus = GetDistUncompressed ( pbc, nPosID );

    if ( ! pus ) {
      printf ( "argh!\n" );
      return;
    }

    if ( ! pbc->fInMemory  && pbc->ph ) {
      /* add to cache */
      hashentryonesided *phe = 
        (hashentryonesided *) malloc ( sizeof ( hashentryonesided ) );
      if ( phe ) {
        phe->nPosID = nPosID;
        memcpy ( phe->aus, pus, sizeof ( phe->aus ) );
        HashAdd ( pbc->ph, HashOneSided ( nPosID ), phe );
      }

    }

  }

  AssignOneSided ( arProb, arGammonProb, ar, ausProb, ausGammonProb,
                   pus, pus+32 );

  ++pbc->nReads;

}

extern void
BearoffDist ( bearoffcontext *pbc, const unsigned int nPosID,
              float arProb[ 32 ], float arGammonProb[ 32 ],
              float ar[ 4 ],
              unsigned short int ausProb[ 32 ], 
              unsigned short int ausGammonProb[ 32 ] ) {

  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:

    if ( pbc->fTwoSided ) {
      assert ( FALSE );
    }
    else {
      /* one-sided db */
      if ( pbc->fND ) 
	assert(0);
      else
        ReadBearoffOneSidedExact ( pbc, nPosID, arProb, arGammonProb, ar,
	  ausProb, ausGammonProb ); 
    }
    break;

  default:
    assert ( FALSE );
    break;
  }

}

extern int
isBearoff ( bearoffcontext *pbc, CONST int anBoard[2][25] ) {

  int nOppBack = -1, nBack = -1;
  int n = 0, nOpp = 0;
  int i;

  if ( ! pbc )
    return FALSE;

  for( nOppBack = 24; nOppBack >= 0; --nOppBack)
    if( anBoard[0][nOppBack] )
      break;

  for(nBack = 24; nBack >= 0; --nBack)
    if( anBoard[1][nBack] )
      break;

  if ( nBack < 0 || nOppBack < 0 )
    /* the game is over */
    return FALSE;

  if ( nBack + nOppBack > 22 )
    /* contact position */
    return FALSE;

  for ( i = 0; i <= nOppBack; ++i )
    nOpp += anBoard[ 0 ][ i ];

  for ( i = 0; i <= nBack; ++i )
    n += anBoard[ 1 ][ i ];

  if ( n <= pbc->nChequers && nOpp <= pbc->nChequers &&
       nBack < pbc->nPoints && nOppBack < pbc->nPoints )
    return TRUE;
  else
    return FALSE;

}


static int
BearoffEvalOneSided ( bearoffcontext *pbc, 
                      CONST int anBoard[2][25], float arOutput[] ) {

  int i, j;
  float aarProb[2][ 32 ];
  float aarGammonProb[2][ 32 ];
  float r;
  int anOn[2];
  int an[2];
  float ar[2][ 4 ];

  /* get bearoff probabilities */

  for ( i = 0; i < 2; ++i ) {

    an[ i ] = PositionBearoff( anBoard[ i ], pbc->nPoints );
    BearoffDist ( pbc, an[ i ], aarProb[ i ], aarGammonProb[ i ], ar [ i ],
                  NULL, NULL );

  }

  /* calculate winning chance */

  r = 0.0;
  for ( i = 0; i < 32; ++i )
    for ( j = i; j < 32; ++j )
      r += aarProb[ 1 ][ i ] * aarProb[ 0 ][ j ];

  if( r < 0.0 ) {
    r = 0;
  } else if( r > 1.0 ) {
    r = 1.0;
  }
  
  arOutput[OUTPUT_WIN] = r;

  /* calculate gammon chances */

  for ( i = 0; i < 2; ++i )
    for ( j = 0, anOn[ i ] = 0; j < 25; ++j )
      anOn[ i ] += anBoard[ i ][ j ];

  if ( anOn[ 0 ] == 15 || anOn[ 1 ] == 15 ) {

    if ( pbc->fGammon ) {

      /* my gammon chance: I'm out in i rolls and my opponent isn't inside
         home quadrant in less than i rolls */
      
      r = 0;
      for( i = 0; i < 32; i++ )
        for( j = i; j < 32; j++ )
          r += aarProb[ 1 ][ i ] * aarGammonProb[ 0 ][ j ];
      
      arOutput[ OUTPUT_WINGAMMON ] = r;
      
      /* opp gammon chance */
      
      r = 0;
      for( i = 0; i < 32; i++ )
        for( j = i + 1; j < 32; j++ )
          r += aarProb[ 0 ][ i ] * aarGammonProb[ 1 ][ j ];
      
      arOutput[ OUTPUT_LOSEGAMMON ] = r;
      
    }
    else {
      
      setGammonProb(anBoard, an[ 0 ], an[ 1 ],
                    &arOutput[ OUTPUT_LOSEGAMMON ],
                    &arOutput[ OUTPUT_WINGAMMON ] );
      
    }
  }
  else {
    /* no gammons possible */
    arOutput[ OUTPUT_WINGAMMON ] = 0.0f;
    arOutput[ OUTPUT_LOSEGAMMON ] = 0.0f;
  }
      
    
  /* no backgammons possible */
    
  arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0f;
  arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0f;

  return ar [ 0 ][ 0 ] * 65535.0;
}

static unsigned long
HashTwoSided ( const unsigned int nPosID ) {
  return nPosID;
}

/*
 * BEAROFF_GNUBG: read two sided bearoff database
 *
 */

static int
ReadTwoSidedBearoff ( bearoffcontext *pbc,
                      const unsigned int iPos,
                      float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  int k = ( pbc->fCubeful ) ? 4 : 1;
  int i;
  unsigned char ac[ 8 ];
  unsigned char *pc = NULL;
  unsigned short int us;

  /* look up in cache */

  if ( ! pbc->fInMemory && pbc->ph ) {

    hashentrytwosided *phe;
    hashentrytwosided he;
    
    he.nPosID = iPos;
    if ( ( phe = HashLookup ( pbc->ph, HashTwoSided ( iPos ), &he ) ) ) 
      pc = phe->auch;
      
  }

  if ( ! pc ) {

    if ( pbc->fInMemory )
      pc = (unsigned char*)(((char *) pbc->p)+ 40 + 2 * iPos * k);
    else {
      lseek ( pbc->h, 40 + 2 * iPos * k, SEEK_SET );
      int const o = read ( pbc->h, ac, k * 2 );
      pc = ac;
    }

    /* add to cache */

    if ( ! pbc->fInMemory && pbc->ph ) {
      /* add to cache */
      hashentrytwosided *phe = 
        (hashentrytwosided *) malloc ( sizeof ( hashentrytwosided ) );
      if ( phe ) {
        phe->nPosID = iPos;
        memcpy ( phe->auch, pc, sizeof ( phe->auch ) );
        HashAdd ( pbc->ph, HashTwoSided ( iPos ), phe );
      }
      
    }
    
  }

  for ( i = 0; i < k; ++i ) {
    us = pc[ 2 * i ] | ( pc[ 2 * i + 1 ] ) << 8;
    if ( aus )
      aus[ i ] = us;
    if ( ar )
      ar[ i ] = us / 32767.5f - 1.0f;
  }      

  ++pbc->nReads;

  return 0;

}

static int
BearoffEvalTwoSided(bearoffcontext* pbc, CONST int anBoard[2][25], float arOutput[])
{
  assert( pbc->nPoints == 6);
    
  int nUs = PositionBearoff( anBoard[ 1 ], pbc->nPoints /*, pbc->nChequers*/ );
  int nThem = PositionBearoff( anBoard[ 0 ], pbc->nPoints /*, pbc->nChequers */ );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  
  if ( ReadTwoSidedBearoff ( pbc, iPos, ar, NULL ) )
    return -1;

  memset ( arOutput, 0, 5 * sizeof ( float ) );
  arOutput[ OUTPUT_WIN ] = ar[ 0 ] / 2.0f + 0.5;
  
  return 0;

}

extern int
BearoffEval(bearoffcontext *pbc, CONST int anBoard[2][25], float arOutput[] )
{
  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:
    if ( pbc->fTwoSided ) 
      BearoffEvalTwoSided ( pbc, anBoard, arOutput );
    else 
      BearoffEvalOneSided ( pbc, anBoard, arOutput );

    break;


  case BEAROFF_EXACT_BEAROFF:

    /*assert ( pbc->fTwoSided );
      BearoffEvalTwoSided ( pbc, anBoard, arOutput ); */
    assert(0);
    break;

  default:

    assert ( FALSE );
    break;

  }

  return 0;
}

extern void
BearoffClose ( bearoffcontext *pbc ) {

  if ( ! pbc )
    return;

  if ( ! pbc->fInMemory )
    close ( pbc->h );
  else if ( pbc->p && pbc->fMalloc )
    free ( pbc->p );
  
}

#endif
