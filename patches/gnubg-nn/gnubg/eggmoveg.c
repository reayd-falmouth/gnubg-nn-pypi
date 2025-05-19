/*
 * eggmove.cc
 *
 * Adapted from Eric Groleau move generator eggrules.c
 * Fixed some bugs as well 
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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*

  1. egg did not find the duplication here.
  
  0 -4 -3 -2 -1 0 -2 0 -2 0 0 0 0 0 0 0 0 0 0 0 1 1 3 6 2 -1 
  dice 5 6
  Gary:
  0)  4--2 3--2 {0 -4 -3 -2 -1 0 -2 0 -2 0 0 0 0 0 0 0 0 0 0 0 0 0 3 6 2 -1}
  Egg:
  0)  5-0 4-0 {0 -4 -3 -2 -1 0 -2 0 -2 0 0 0 0 0 0 0 0 0 0 0 0 0 3 6 2 -1}
  1)  5-0 4-0 {0 -4 -3 -2 -1 0 -2 0 -2 0 0 0 0 0 0 0 0 0 0 0 0 0 3 6 2 -1}


  2. egg missed a move.

  0 0 1 -2 -2 -3 -1 0 0 0 0 0 0 -2 0 0 0 0 -1 0 0 -3 2 5 -1 0 
  dice 1 3
  Gary:
  0)  2-1 {0 0 1 -2 -2 -3 -1 0 0 0 0 0 0 -2 0 0 0 0 -1 0 0 -3 1 6 -1 0}
  1)  1-0 {0 0 1 -2 -2 -3 -1 0 0 0 0 0 0 -2 0 0 0 0 -1 0 0 -3 2 4 1 -1}
  Egg:
  0)  2-1 {0 0 1 -2 -2 -3 -1 0 0 0 0 0 0 -2 0 0 0 0 -1 0 0 -3 2 4 1 -1}

  3. egg missed 2 moves
  0 0 -1 -2 -2 -2 -2 0 -1 0 -2 0 0 0 0 0 0 2 0 3 -1 3 5 -2 2 0
  Dice 6 3

  Gary:
  0)  7-4 {0 0 -1 -2 -2 -2 -2 0 -1 0 -2 0 0 0 0 0 0 1 0 3 1 3 5 -2 2 -1}
  1)  5-2 {0 0 -1 -2 -2 -2 -2 0 -1 0 -2 0 0 0 0 0 0 2 0 2 -1 3 6 -2 2 0}
  2)  3-0 {0 0 -1 -2 -2 -2 -2 0 -1 0 -2 0 0 0 0 0 0 2 0 3 -1 2 5 -2 3 0}
  Egg:
  0)  8-5 {0 0 -1 -2 -2 -2 -2 0 -1 0 -2 0 0 0 0 0 0 1 0 3 1 3 5 -2 2 -1}

  3. BG rules says you have to use both dice is possible.

  0 -3 -3 -3 -2 0 -3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 -1 0 
  dice 1 2
  Gary:
  0)  1-0 0--2 {0 -3 -3 -3 -2 0 -3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1}
  Egg:
  0)  2-1 1-0 {0 -3 -3 -3 -2 0 -3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1}
  1)  2-0 {0 -3 -3 -3 -2 0 -3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0}

  4. used only one dice in move description
   0 -2 -2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0
   Dice 5 2
   
*/

#define max(x,y)   (((x) < (y)) ? (y) : (x))
#define min(x,y)   (((x) > (y)) ? (y) : (x))

/* Note than egg uses 26 points, GNU only 25 */

#define NB_POINTS 26     /* Number of points in board representation  */
#define BAR       25     /* Bar's point number                        */

typedef struct emovelist {
  /* number of unplayed dies */
  int  unplayedDice;

  /* inital point */
  
  int  from[4];

  /* final point */
  /* If the final point is negative, this move hit */
  /* an opposing checker and sent it to the bar. */
  
  int  to[4];
} emovelist;

/* in GNU, we only move player 1 */
#define turn 1


/*
 *  move_checker
 *
 *  arguments:    board  I/O  current checker positions
 *                turn   I    active player (0 or 1)
 *                dice   I    dice value
 *                point  I    departure point
 *            max_point  I    highest occupied point
 *                move   O    move description
 *                d      I    current move number
 *
 *  return value: True if it is legal to move a checker from point 'point'
 *
 *  The procedure tries to move an active player's checker from the departure
 *  point using dice value 'dice'. If the move is illegal, it returns false
 *  without moving the checker.
 *  If the move is legal, the procedure:
 *       . moves the checker in 'board'
 *       . records the move in 'move' as move number 'd'
 *       . returns true
 */
static int
move_checker(int board[2][NB_POINTS] /*, int turn*/, int dice, int point,
	     int max_point, emovelist* move, int d)
{
  int arr_point;                 /* Arrival point */
  
  /* Verify if i have a checker on that point. */

  if( !board[turn][point] )
    return 0;

  arr_point = point - dice;

  /* Do we have a checker stuck on the BAR?                */

  if( (point != BAR) && board[turn][BAR] )
    return 0;

  /* If we're bearing off, arr_point = 0                   */

  if( arr_point <= 0 ) {
    /* can't move from HOME point. rare but the algorithm may try to do that
       say play 12 for
       {0 -3 -3 -3 -2 0 -3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 -1 0}
    */
    if( point == 0 )
      return 0;

    if( (max_point <= 6) && ((point == max_point) || (point == dice)) )
      arr_point = 0;
    else 
      return 0;
  }
  
  /* I can move if opponent has 0 or 1 checker on arr_point */
  /* or if arr_point = 0 (bearoff)                          */

  if( (board[1-turn][BAR-arr_point] < 2) || (arr_point == 0) ) {

    --d;
    
    /* Record the move */

    move->from[d] = point;
    move->to[d]   = arr_point;

    /* Change the board accordingly */

    board[turn][point]--;
    board[turn][arr_point]++;

    /* If we hit a blot, move it to the bar. */

    if( (arr_point != 0) && board[1-turn][BAR-arr_point]) {
      board[1-turn][BAR-arr_point]--;
      board[1-turn][BAR]++;
      move->to[d] = -move->to[d];
    }
    
    return 1;     /* The checker has been moved. */
  }

  return 0;     /* The checker could not move. */
}

/*
 *  unmove_checker
 *
 *  arguments:    board  O    current checker positions
 *                turn   I    active player (0 or 1)
 *                move   I    move description
 *                d      I    current move number
 *
 *  return value: none
 *
 *  The procedure replaces a previously moved checker to it's original
 *  position using move number 'd' in 'move' description.
 *
 *  Note: checkers must be unmoved in the exact reverse order in which they
 *        were moved to keep a coherent board.
 */
static void
unmove_checker(int board[2][NB_POINTS] /*, int turn*/, emovelist* move, int d)
{
  int dep_point,                 /* Departure point */
      arr_point;                 /* Arrival point   */

  --d;
  
  dep_point = move->from[d];
  arr_point = move->to[d];

  /* If a blot was hit, put it back from the bar */

  if( arr_point < 0 ) {
    arr_point = -arr_point;
    
    board[1-turn][BAR-arr_point]++;
    board[1-turn][BAR]--;
  }

  /* Put our own checker back */

  board[turn][dep_point]++;
  board[turn][arr_point]--;
}

/*
 *  list_moves_non_double
 *
 *  arguments:    board  I    current checker positions
 *                moves  O    array of moves
 *                turn   I    active player (0 or 1)
 *                d1,d2  I    dice value
 *
 *  return value: number of legal moves found
 *
 *  Find all legal moves for player 'turn' playing dice 'd1-d2' from position
 *  'board'. Legal moves are written to array 'moves'. See eggbg.h for the
 *  description of 'moves' format.
 *
 *  Note that 'board' is modified using move_checker and unmove_checker to
 *  find the valid moves. However, 'board' is returned to it's original
 *  value at the end of the function, that is all checkers that are moved
 *  are subsequently unmoved.
 */
static int
list_moves_non_double(int board[2][NB_POINTS], emovelist* moves,
		      /*int turn,*/ int d1, int d2)
{
  int nb_moves,        /* number of possible moves            */
      p1, p2,          /* current departure point             */
      minp1, minp2,    /* lowest legal departure point        */
      maxp1, maxp2,    /* highest occupied point              */
      seq,             /* search sequence                     */
      replicate,       /* TRUE if this move replicates a previous one */
      i;

  /* Initialize searching variables */

  nb_moves = 0;

  for(i = 0; i < 4; i++) {
    moves[0].from[i] = 0;
    moves[0].to[i] = 0;
  }

  /* Find the highest occupied point */

  for(maxp1 = BAR; !board[turn][maxp1]; maxp1--)
    ;

  /* Search once, invert the dice order and search again */

  for(seq = 0; seq < 2; seq++) {

    /* If we have a checker past the 6 point, we can't bearoff yet */

    if( maxp1 > 6 ) minp1 = d1;
    else            minp1 = min(d1, maxp1) - 1;

    /* Search from maxpnt to the lowest legal point */

    for( p1 = maxp1; p1 > minp1; p1--) {

      /* Skip empty points */

      while( !board[turn][p1])	p1--;

      /* Verify if we can move this checker with d1 */

      if( move_checker(board, /*turn,*/ d1, p1, maxp1, &moves[nb_moves], 1)) {

        moves[nb_moves].unplayedDice = 1;         /* 1 checker left to move  */

        /* Find the highest occupied point. Could have changed since d1 */

        for(maxp2=maxp1; !board[turn][maxp2]; maxp2--);

        /* If we have a checker past the 6 point, we can't bearoff yet */

        if (maxp2 > 6) minp2 = d2;
        else           minp2 = min(d2, maxp2) - 1;

        for(p2 = p1 - seq; p2 > minp2; p2--) {

        /* Skip empty points */

        while (!board[turn][p2]) p2--;

        /* Verify if we can move the second checker with d2 */

          if( move_checker(board,/*turn,*/d2, p2, maxp2, &moves[nb_moves],2)) {

            /* The move is complete. Verify that it is not a replicate */
            /* With our searching algorith, the only possible          */
            /* replication is if the same checker is moved with both   */
            /* dice and no blot was taken with the first dice.         */

            replicate = 0;
            if( moves[nb_moves].to[0] == moves[nb_moves].from[1] ) {
              for( i = 0; i < nb_moves; ++i) {
		/* don't compare incomplete move with this complete move. We */
		/* want the complete move to 'override' an incomplete one.   */
		
                if( moves[i].unplayedDice == 0 &&
		    (moves[i].from[0] == moves[nb_moves].from[0]) &&
		    (moves[i].to[0] == moves[i].from[1])) {
                  replicate = 1;
		  break;
		}
	      }
	    }

	    /* Another source for replication is when both dice where used  */
	    /* to bear off, say 5 6 in                                      */
	    /* {0 -4 -3 -2 -1 0 -2 0 -2 0 0 0 0 0 0 0 0 0 0 0 1 1 3 6 2 -1} */
	    
            if( !replicate &&
		(moves[nb_moves].to[0] == 0 && moves[nb_moves].to[1] == 0) ) {
              for(i=0; i < nb_moves; i++)
                if( memcmp(moves[i].from, moves[nb_moves].from,
			   2 * sizeof(moves[i].from[0])) == 0 &&
		    memcmp(moves[i].to, moves[nb_moves].to,
			   2 * sizeof(moves[i].to[0])) == 0 ) {
                  replicate = 1;
		  break;
		}
	    }

            unmove_checker(board, /* turn, */ &moves[nb_moves], 2);

            if( !replicate ) {

              /* It's not a replicate. Keep it.                        */

              moves[nb_moves].unplayedDice = 0;    /* move complete    */
              moves[nb_moves].from[2] = moves[nb_moves].from[3] = 0;
              moves[nb_moves].to[2] = moves[nb_moves].to[3] = 0;

              /* Erase previously recorded less complete moves.        */

              if( moves[0].unplayedDice > moves[nb_moves].unplayedDice ) {
		moves[0] = moves[nb_moves];
                nb_moves = 0;
              }

              ++ nb_moves;
              moves[nb_moves].unplayedDice =
		moves[nb_moves-1].unplayedDice + 1;
	      
	      moves[nb_moves].from[0] = moves[nb_moves-1].from[0];
	      moves[nb_moves].to[0]   = moves[nb_moves-1].to[0];
            }

          } /* checker 2 moved */
        }

        unmove_checker(board, /* turn, */ &moves[nb_moves], 1);

        /* Record the move if no more complete move has been recorded  */

        if( moves[nb_moves].unplayedDice <= moves[0].unplayedDice ) {

          if( (moves[nb_moves].from[0] - abs(moves[nb_moves].to[0])) >
              (moves[0].from[0] - abs(moves[0].to[0]))) {
	    
	    moves[0].from[0] = moves[nb_moves].from[0];
	    moves[0].to[0] = moves[nb_moves].to[0];

	    for(i=1; i<4; i++) {
              moves[0].from[i] = 0;
              moves[0].to[i] = 0;
	    }

            nb_moves = 1;
          }

          if( (nb_moves == 0) ||
              (((moves[nb_moves].from[0] - abs(moves[nb_moves].to[0])) ==
                (moves[0].from[0] - abs(moves[0].to[0]))) &&
               (moves[nb_moves].from[0] != moves[0].from[0]))) {
            for (i=1; i<4; i++) {
              moves[nb_moves].from[i] = 0;
              moves[nb_moves].to[i] = 0;
	    }
            nb_moves++;
          }
        }
      
      } /* checker 1 moved */
    }

    /* Invert the dice to look for new moves. */

    i = d1; d1 = d2; d2 = i;
  }

  return nb_moves;
}

/*
 *  list_moves_double
 *
 *  arguments:    board  I    current checker positions
 *                moves  O    array of moves
 *                turn   I    active player (0 or 1)
 *                dice   I    dice value
 *
 *  return value: number of legal moves found
 *
 *  Find all legal moves for player 'turn' playing dice 'dice' from position
 *  'board'. Legal moves are written to array 'moves'. See eggbg.h for the
 *  description of 'moves' format.
 *
 *  Note that 'board' is modified using move_checker and unmove_checker to
 *  find the valid moves. However, 'board' is returned to it's original
 *  value at the end of the function, that is all checkers that are moved
 *  are subsequently unmoved.
 */
static int
list_moves_double(int board[2][NB_POINTS], emovelist* moves,
		  /* int turn,*/ int dice)
{
  int nb_moves,        /* number of possible moves            */
      p1, p2, p3, p4,  /* current points for all 4 dice       */
    /*minp1,*/ minp2, minp3, minp4,
      maxp1, maxp2, maxp3, maxp4,
      i;

  /* Initialize searching variables */

  nb_moves = 0;

  for( i=0; i<4; i++ ) {
    moves[0].from[i] = 0;
    moves[0].to[i] = 0;
  }

  /* Same search structure as non_double. See comments there. */

  for( maxp1 = BAR; !board[turn][maxp1]; maxp1--);
  
/*    if( maxp1 > 6 ) minp1 = dice; */
/*    else            minp1 = min(dice, maxp1) - 1; */

  /* Start searching */

  for( p1 = maxp1; p1 > 0; p1-- ) {
    while (!board[turn][p1] && p1) p1--;
    
    if( move_checker(board, /*turn,*/ dice, p1, maxp1, &moves[nb_moves], 1)) {

      moves[nb_moves].unplayedDice = 3;           /* 3 checkers left to move */

      for(maxp2=maxp1; !board[turn][maxp2]; maxp2--);
      
      if (maxp2 > 6) minp2 = dice;
      else           minp2 = min(dice, maxp2) - 1;

      for( p2 = p1; p2 > minp2; p2-- ) {
        while( !board[turn][p2] && p2 ) p2--;
	
        if( move_checker(board,/*turn,*/dice, p2, maxp2, &moves[nb_moves],2)) {

          moves[nb_moves].unplayedDice = 2;    /* 2 checkers left to move */

          for (maxp3=maxp2; !board[turn][maxp3]; maxp3--);
          if (maxp3 > 6) minp3 = dice;
          else           minp3 = min(dice, maxp3) - 1;

          for (p3=p2; p3>minp3; p3--) {
            while (!board[turn][p3] && p3) p3--;
            if (move_checker(board, /* turn, */ dice, p3, maxp3, 
                             &moves[nb_moves], 3)) {

              moves[nb_moves].unplayedDice = 1;   /* 1 checker left to move  */

              for (maxp4=maxp3; !board[turn][maxp4]; maxp4--);
              if (maxp4 > 6) minp4 = dice;
              else           minp4 = min(dice, maxp4) - 1;

              for (p4=p3; p4>minp4; p4--) {
                while (!board[turn][p4] && p4) p4--;
		
                if( move_checker(board, /* turn, */ dice, p4, maxp4,
                                 &moves[nb_moves], 4)) {

                  moves[nb_moves].unplayedDice = 0; /* move complete */

                  unmove_checker(board, /* turn, */ &moves[nb_moves], 4);

                  /* Erase previously recorded less complete moves.        */

                  if( moves[0].unplayedDice > moves[nb_moves].unplayedDice ) {
		    moves[0] = moves[nb_moves];
                    nb_moves = 0;
                  }

                  nb_moves++;
                  moves[nb_moves].unplayedDice =
		    moves[nb_moves-1].unplayedDice + 1;
		  
                  for(i=0; i<3; i++) {
                    moves[nb_moves].from[i] = moves[nb_moves-1].from[i];
                    moves[nb_moves].to[i]   = moves[nb_moves-1].to[i];
                  }

                } /* checker 4 moved */
              }

              unmove_checker(board, /* turn, */ &moves[nb_moves], 3);

              if( moves[nb_moves].unplayedDice <= moves[0].unplayedDice ) {
                nb_moves++;
                moves[nb_moves].unplayedDice =
		  moves[nb_moves-1].unplayedDice + 1;
		
                for (i=0; i<2; i++) {
                  moves[nb_moves].from[i] = moves[nb_moves-1].from[i];
                  moves[nb_moves].to[i]   = moves[nb_moves-1].to[i];
                }
              }

            } /* checker 3 moved */
          }

          unmove_checker(board, /* turn, */ &moves[nb_moves], 2);

          if (moves[nb_moves].unplayedDice <= moves[0].unplayedDice) {
            nb_moves++;
            moves[nb_moves].unplayedDice = moves[nb_moves-1].unplayedDice + 1;

	    for(i=0; i<1; i++) {
              moves[nb_moves].from[i] = moves[nb_moves-1].from[i];
              moves[nb_moves].to[i]   = moves[nb_moves-1].to[i];
            }
          }

        } /* checker 2 moved */
      }

      unmove_checker(board, /* turn, */ &moves[nb_moves], 1);

      if( moves[nb_moves].unplayedDice <= moves[0].unplayedDice )
        nb_moves++;
      
    } /* checker 1 moved */
  }

  return nb_moves;
}

/*
 *  play_move
 *
 *  arguments:    board  O    current checker positions
 *                turn   I    active player (0 or 1)
 *                move   I    move description
 *
 *  return value: none
 *
 *  The procedure applies 'move' to 'board' for the active player. The legality
 *  of the move is not verified. Use move_checker to verify for the legality
 *  of a move.
 */
static void
play_move(int board[2][NB_POINTS] /*, int turn*/, emovelist* move)
{
  int dep_point,
      arr_point,
      i;

  for(i=1; (i<5) && move->from[i-1]; i++) {
    dep_point = move->from[i-1];
    arr_point = move->to[i-1];

    if( arr_point < 0 ) {                /* A blot was hit          */
      arr_point = -arr_point;
      board[1-turn][BAR-arr_point]--;   /* Remove the blot         */
      board[1-turn][BAR]++;             /* and place it on the bar */
    }

    board[turn][dep_point]--;           /* Move our own checker    */
    board[turn][arr_point]++;           /* to it's landing spot.   */
  }
}

/*
 *  unplay_move
 *
 *  arguments:    board  O    current checker positions
 *                turn   I    active player (0 or 1)
 *                move   I    move description
 *
 *  return value: none
 *
 *  The procedure restores 'board' to the state it was in before 'move' was
 *  applied.
 *
 *  Note: moves must be unplayed in the exact reverse order in which they
 *        were played to keep a coherent board.
 */
static void
unplay_move(int board[2][NB_POINTS] /*, int turn*/, emovelist* move)
{
  int dep_point,
      arr_point,
      i;

  for (i=1; (i<5) && move->from[i-1]; i++) {
    dep_point = move->from[i-1];
    arr_point = move->to[i-1];

    if (arr_point < 0) {                /* We hit a blot :)        */
      arr_point = -arr_point;
      board[1-turn][BAR-arr_point]++;   /* Replace the blot        */
      board[1-turn][BAR]--;             /* remove it from the bar  */
    }

    board[turn][dep_point]++;           /* Replace our own checker */
    board[turn][arr_point]--;           /* to it's original spot.  */
  }
}


static
#if defined( __GNUC__ )
inline
#endif
void
addBits(unsigned char auchKey[10], int const  bitPos, int const nBits)
{
  int const k = bitPos / 8;
  int const r = (bitPos & 0x7);
  
  unsigned int b = (((unsigned int)0x1 << nBits) - 1) << r;
  
  auchKey[k] |= b;
  
  if( k < 8 ) {
    auchKey[k+1] |= b >> 8;
    auchKey[k+2] |= b >> 16;
  } else if( k == 8 ) {
    auchKey[k+1] |= b >> 8;
  }
}

static void
eggPositionKey(int anBoard[2][26], unsigned char auchKey[10])
{
  int i, iBit = 0;
  const int* j;

  memset(auchKey, 0, 10 * sizeof(*auchKey));

  for(i = 0; i < 2; ++i) {
    const int* const b = anBoard[i] + 1;
    for(j = b; j < b + 25; ++j) {
      int const nc = *j;

      if( nc ) {
	addBits(auchKey, iBit, nc);
	iBit += nc + 1;
      } else {
	++iBit;
      }
    }
  }
}

#include "eggmove.h"
#include "eval.h"

#define MAX_MOVES 3060

static emovelist moves[MAX_MOVES];

move amMoves[MAX_MOVES];

int
eGenerateMoves(movelist* pml, CONST int anBoard[2][25], int n0, int n1)
{
  int eb[2][NB_POINTS];
  int i, k;

  eb[0][0] = eb[1][0] = 0;
  memcpy(&eb[0][1], anBoard[0], sizeof(anBoard[0]));
  memcpy(&eb[1][1], anBoard[1], sizeof(anBoard[1]));
  
  if( n0 == n1 ) {
    pml->cMoves = list_moves_double(eb, moves /*, 1*/, n0);
  } else {
    pml->cMoves = list_moves_non_double(eb, moves /*, 1*/, n0, n1);
  }
  assert( pml->cMoves <= MAX_MOVES );
  
  pml->amMoves = amMoves;
  pml->cMaxMoves = 4;
  
  for(i = 0; i < pml->cMoves; ++i) {
    emovelist* const m = &moves[i];
    
    play_move(eb /*, 1*/, m);

    eggPositionKey(eb, amMoves[i].auch);

    unplay_move(eb /*, 1*/, m);

    for(k = 1; k < 4 && m->from[k] != 0; ++k)
      ;
    amMoves[i].cMoves = k;
    
    for(k = 0; k < amMoves[i].cMoves; ++k) {
      int const j = 2 * k;
      
      amMoves[i].anMove[j] = m->from[k] - 1;
      amMoves[i].anMove[j+1] = abs(m->to[k]) - 1;
    }

    if( k < 4 ) {
      amMoves[i].anMove[2*k] = -1;
    }

    amMoves[i].pEval = 0;
  }

  return pml->cMoves;
}



/****/


static float wc[122] = {
    .25696f,   -.66937f, -1.66135f, -2.02487f, -2.53398f / 2.0f,
    -.16092f, -1.11725f, -1.06654f,  -.92830f, -1.99558f / 2.0f,
    -1.10388f, -.80802f, .09856f, -.62086f, -1.27999f / 2.0f,
    -.59220f, -.73667f, .89032f, -.38933f, -1.59847f / 2.0f,
    -1.50197f, -.60966f, 1.56166f, -.47389f, -1.80390f / 2.0f,
    -.83425f, -.97741f, -1.41371f, .24500f, .10970f / 2.0f,
    -1.36476f, -1.05572f, 1.15420f, .11069f, -.38319f / 2.0f,
    -.74816f, -.59244f, .81116f, -.39511f, .11424f / 2.0f,
    -.73169f, -.56074f, 1.09792f, .15977f, .13786f / 2.0f,
    -1.18435f, -.43363f, 1.06169f, -.21329f, .04798f / 2.0f,
    -.94373f, -.22982f, 1.22737f, -.13099f, -.06295f / 2.0f,
    -.75882f, -.13658f, 1.78389f, .30416f, .36797f / 2.0f,
    -.69851f, .13003f, 1.23070f, .40868f, -.21081f / 2.0f,
    -.64073f, .31061f, 1.59554f, .65718f, .25429f / 2.0f,
    -.80789f, .08240f, 1.78964f, .54304f, .41174f / 2.0f,
    -1.06161f, .07851f, 2.01451f, .49786f, .91936f / 2.0f,
    -.90750f, .05941f, 1.83120f, .58722f, 1.28777f / 2.0f,
    -.83711f, -.33248f, 2.64983f, .52698f, .82132f / 2.0f,
    -.58897f, -1.18223f, 3.35809f, .62017f, .57353f / 2.0f,
    -.07276f, -.36214f, 4.37655f, .45481f, .21746f / 2.0f,
    .10504f, -.61977f, 3.54001f, .04612f, -.18108f / 2.0f,
    .63211f, -.87046f, 2.47673f, -.48016f, -1.27157f / 2.0f,
    .86505f, -1.11342f, 1.24612f, -.82385f, -2.77082f / 2.0f,
    1.23606f, -1.59529f, .10438f, -1.30206f, -4.11520f / 2.0f,

    5.62596f/2.0f, -2.75800f/15.0f
};

static float wr[122] = {
    .00000f, -.17160f, .27010f, .29906f, -.08471f / 2.0f,
    .00000f, -1.40375f, -1.05121f, .07217f, -.01351f / 2.0f,
    .00000f, -1.29506f, -2.16183f, .13246f, -1.03508f / 2.0f,
    .00000f, -2.29847f, -2.34631f, .17253f, .08302f / 2.0f,
    .00000f, -1.27266f, -2.87401f, -.07456f, -.34240f / 2.0f,
    .00000f, -1.34640f, -2.46556f, -.13022f, -.01591f / 2.0f,
    .00000f, .27448f, .60015f, .48302f, .25236f / 2.0f,
    .00000f, .39521f, .68178f, .05281f, .09266f / 2.0f,
    .00000f, .24855f, -.06844f, -.37646f, .05685f / 2.0f,
    .00000f, .17405f, .00430f, .74427f, .00576f / 2.0f,
    .00000f, .12392f, .31202f, -.91035f, -.16270f / 2.0f,
    .00000f, .01418f, -.10839f, -.02781f, -.88035f / 2.0f,
    .00000f, 1.07274f, 2.00366f, 1.16242f, .22520f / 2.0f,
    .00000f, .85631f, 1.06349f, 1.49549f, .18966f / 2.0f,
    .00000f, .37183f, -.50352f, -.14818f, .12039f / 2.0f,
    .00000f, .13681f, .13978f, 1.11245f, -.12707f / 2.0f,
    .00000f, -.22082f, .20178f, -.06285f, -.52728f / 2.0f,
    .00000f, -.13597f, -.19412f, -.09308f, -1.26062f / 2.0f,
    .00000f, 3.05454f, 5.16874f, 1.50680f, 5.35000f / 2.0f,
    .00000f, 2.19605f, 3.85390f, .88296f, 2.30052f / 2.0f,
    .00000f, .92321f, 1.08744f, -.11696f, -.78560f / 2.0f,
    .00000f, -.09795f, -.83050f, -1.09167f, -4.94251f / 2.0f,
    .00000f, -1.00316f, -3.66465f, -2.56906f, -9.67677f / 2.0f,
    .00000f, -2.77982f, -7.26713f, -3.40177f, -12.32252f / 2.0f,
    .00000f / 2.0f, 3.42040f / 15.0f
};

static float
pubeval1(int race, int pos[])
{
  int i, j;
  float score;
  float* w = race ? wr : wc;
  
  if(pos[26] == 15) return(99999999.);
  /* all men off, best possible move */
    
  score = (w[120] * -(float)(pos[0]) +
	   w[121] * (float)(pos[26]));
    
  for(j = 24, i = 0;  j > 0; --j, i += 5) {
    int n = pos[j];

    switch( n ) {
      case -1: score += w[i]; break;
      case  1: score += w[i+1]; break;
      case  2: score += w[i+2]; break;
      case  3: score += w[i+2] + w[i+3]; break;
      /* replace the GCC range with explicit cases: */
      // case 4 ... 15: score += w[i+2] + (w[i+4] * (float)(n-3)); break;
      case  4:
      case  5:
      case  6:
      case  7:
      case  8:
      case  9:
      case 10:
      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
        score += w[i+2] + (w[i+4] * (float)(n-3));
        break;
      case 0: break;
      default: break;
    }
  }
  
  return(score);
}

#include <math.h>

static float
pubEvalVale(int race, int b[2][26])
{
  int anPubeval[28], j;

  int* b0 = &b[0][1];
  int* b1 = &b[1][1];

  anPubeval[ 26 ] = 15; 
  anPubeval[ 27 ] = -15;

/*    anPubeval[ 26 ] = b[1][0];  */
/*    anPubeval[ 27 ] = -b[0][0]; */
  
  for( j = 0; j < 24; j++ ) {
    int nc = b1[ j ];
    
    if( nc ) {
      anPubeval[ j + 1 ] = nc;
      anPubeval[ 26 ] -= nc; 
    }
    else {
      int nc = b0[ 23 - j ];
      anPubeval[ j + 1 ] = -nc;
      anPubeval[ 27 ] += nc;
    }
  }
    
  anPubeval[ 0 ] = -b0[ 24 ];
  anPubeval[ 25 ] = b1[ 24 ];

  {
    //float v1 = pubeval(0, anPubeval);
    float v2 = pubeval1(race, anPubeval);
    //assert( fabs(v1 - v2) <= 0.0001 );
    return v2;
  }
}

// probably bug, may need to reverse sides
void
getPBMove(CONST int anBoard[2][25], int race, int bestMove[2][25], int n0, int n1)
{
  int eb[2][NB_POINTS];
  int n;

  memcpy(&eb[0][1], anBoard[0], sizeof(anBoard[0]));
  memcpy(&eb[1][1], anBoard[1], sizeof(anBoard[1]));
  eb[0][0] = eb[1][0] = 0;
  
  if( n0 == n1 ) {
    n = list_moves_double(eb, moves /*, 1*/, n0);
  } else {
    n = list_moves_non_double(eb, moves /*, 1*/, n0, n1);
  }

  if( n == 0 ) {
    memcpy(bestMove[0], anBoard[0], sizeof(anBoard[0]));
    memcpy(bestMove[1], anBoard[1], sizeof(anBoard[1]));
  } else {
    
    int i, iBest = -1;
    float best = - 1e20;
    
    for(i = 0; i < n; ++i) {
      emovelist* const m = &moves[i];
    
      play_move(eb /*, 1*/, m);

      {
	float v = pubEvalVale(race, eb);
	if( v > best ) {
	  best = v;
	  iBest = i;
	}
      }

      unplay_move(eb /*, 1*/, m);
    }

    play_move(eb /*, 1*/, &moves[iBest]);
    memcpy(bestMove[0], &eb[0][1], sizeof(anBoard[0]));
    memcpy(bestMove[1], &eb[1][1], sizeof(anBoard[1]));
  }
}


#if ! defined( GenerateMoves )

move amMoves[ 3060 ];

static void ApplyMove( int anBoard[2][25], int iSrc, int nRoll ) {

    int iDest = iSrc - nRoll;

    anBoard[ 1 ][ iSrc ]--;

    if( iDest < 0 )
	return;
    
    if( anBoard[ 0 ][ 23 - iDest ] ) {
	anBoard[ 1 ][ iDest ] = 1;
	anBoard[ 0 ][ 23 - iDest ] = 0;
	anBoard[ 0 ][ 24 ]++;
    } else
	anBoard[ 1 ][ iDest ]++;
}

static void
SaveMoves(movelist *pml, int cMoves, int cPip, int anMoves[],
	  int anBoard[2][25])
{
  int i, j;
  move *pm;
  unsigned char auch[ 10 ];

  if( cMoves < pml->cMaxMoves || cPip < pml->cMaxPips )
    return;

  if( cMoves > pml->cMaxMoves || cPip > pml->cMaxPips )
    pml->cMoves = 0;

  pm = pml->amMoves + pml->cMoves;
    
  pml->cMaxMoves = cMoves;
  pml->cMaxPips = cPip;
    
  PositionKey( anBoard, auch );
    
  for( i = 0; i < pml->cMoves; i++ )
    if( EqualKeys( auch, pml->amMoves[ i ].auch ) ) {
      /* update moves, just in case cMoves or cPip has increased */
      for( j = 0; j < cMoves * 2; j++ )
	pml->amMoves[ i ].anMove[ j ] = anMoves[ j ];
    
      if( cMoves < 4 )
	pml->amMoves[ i ].anMove[ cMoves * 2 ] = -1;
    
      return;
    }
    
  for( i = 0; i < cMoves * 2; i++ )
    pm->anMove[ i ] = anMoves[ i ];
    
  if( cMoves < 4 )
    pm->anMove[ cMoves * 2 ] = -1;
    
  for( i = 0; i < 10; i++ )
    pm->auch[ i ] = auch[ i ];

  pm->cMoves = cMoves;
  pm->cPips = cPip;

  pm->pEval = NULL;
    
  pml->cMoves++;

  assert( pml->cMoves < 3060 );
}

static int
LegalMove( int anBoard[2][25], int iSrc, int nPips )
{

  int i, nBack = 0, iDest = iSrc - nPips;

  if( iDest >= 0 )
    return ( anBoard[ 0 ][ 23 - iDest ] < 2 );

    /* otherwise, attempting to bear off */

  for( i = 1; i < 25; i++ )
    if( anBoard[ 1 ][ i ] > 0 )
      nBack = i;

  return ( nBack <= 5 && ( iSrc == nBack || iDest == -1 ) );
}

static int
GenerateMovesSub( movelist *pml, int anRoll[], int nMoveDepth,
		  int iPip, int cPip, int anBoard[2][25],
		  int anMoves[] )
{
  int i, iCopy, fUsed = 0;
  int anBoardNew[2][25];

  if( nMoveDepth > 3 || !anRoll[ nMoveDepth ] )
    return -1;

  if( anBoard[ 1 ][ 24 ] ) { /* on bar */
    if( anBoard[ 0 ][ anRoll[ nMoveDepth ] - 1 ] >= 2 )
      return -1;

    anMoves[ nMoveDepth * 2 ] = 24;
    anMoves[ nMoveDepth * 2 + 1 ] = 24 - anRoll[ nMoveDepth ];

    for( i = 0; i < 25; i++ ) {
      anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
      anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
    }
	
    ApplyMove( anBoardNew, 24, anRoll[ nMoveDepth ] );
	
    if( GenerateMovesSub( pml, anRoll, nMoveDepth + 1, 23, cPip +
			  anRoll[ nMoveDepth ], anBoardNew, anMoves ) < 0 )
      SaveMoves( pml, nMoveDepth + 1, cPip + anRoll[ nMoveDepth ],
		 anMoves, anBoardNew );

    return 0;
  } else {
    for( i = iPip; i >= 0; i-- )
      if( anBoard[ 1 ][ i ] && LegalMove( anBoard, i,
					  anRoll[ nMoveDepth ] ) ) {
	anMoves[ nMoveDepth * 2 ] = i;
	anMoves[ nMoveDepth * 2 + 1 ] = i -
	  anRoll[ nMoveDepth ];
		
	for( iCopy = 0; iCopy < 25; iCopy++ ) {
	  anBoardNew[ 0 ][ iCopy ] = anBoard[ 0 ][ iCopy ];
	  anBoardNew[ 1 ][ iCopy ] = anBoard[ 1 ][ iCopy ];
	}
    
	ApplyMove( anBoardNew, i, anRoll[ nMoveDepth ] );
		
	if( GenerateMovesSub( pml, anRoll, nMoveDepth + 1,
			      anRoll[ 0 ] == anRoll[ 1 ] ? i : 23,
			      cPip + anRoll[ nMoveDepth ],
			      anBoardNew, anMoves ) < 0 ) {
	  SaveMoves( pml, nMoveDepth + 1, cPip +
		     anRoll[ nMoveDepth ], anMoves, anBoardNew );
	}
		
	fUsed = 1;
      }
  }

  return fUsed ? 0 : -1;
}

extern int
GenerateMoves(movelist* pml, int anBoard[2][25], int n0, int n1)
{
  int anRoll[ 4 ], anMoves[ 8 ];

  anRoll[ 0 ] = n0;
  anRoll[ 1 ] = n1;

  anRoll[2] = anRoll[ 3 ] = ( ( n0 == n1 ) ? n0 : 0 );

  pml->cMoves = pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
  pml->amMoves = amMoves; /* use static array for top-level search, since
			     it doesn't need to be re-entrant */
    
  GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves );

  if( anRoll[ 0 ] != anRoll[ 1 ] ) {
    swap( anRoll, anRoll + 1 );

    GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves );
  }

  return pml->cMoves;
}
#endif

