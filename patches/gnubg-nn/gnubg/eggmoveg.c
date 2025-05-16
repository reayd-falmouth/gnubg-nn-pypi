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
    .25696,   -.66937, -1.66135, -2.02487, -2.53398 / 2.0,
    -.16092, -1.11725, -1.06654,  -.92830, -1.99558 / 2.0,
    -1.10388, -.80802, .09856, -.62086, -1.27999 / 2.0,
    -.59220, -.73667, .89032, -.38933, -1.59847 / 2.0,
    -1.50197, -.60966, 1.56166, -.47389, -1.80390 / 2.0,
    -.83425, -.97741, -1.41371, .24500, .10970 / 2.0,
    -1.36476, -1.05572, 1.15420, .11069, -.38319 / 2.0,
    -.74816, -.59244, .81116, -.39511, .11424 / 2.0,
    -.73169, -.56074, 1.09792, .15977, .13786 / 2.0,
    -1.18435, -.43363, 1.06169, -.21329, .04798 / 2.0,
    -.94373, -.22982, 1.22737, -.13099, -.06295 / 2.0,
    -.75882, -.13658, 1.78389, .30416, .36797 / 2.0,
    -.69851, .13003, 1.23070, .40868, -.21081 / 2.0,
    -.64073, .31061, 1.59554, .65718, .25429 / 2.0,
    -.80789, .08240, 1.78964, .54304, .41174 / 2.0,
    -1.06161, .07851, 2.01451, .49786, .91936 / 2.0,
    -.90750, .05941, 1.83120, .58722, 1.28777 / 2.0,
    -.83711, -.33248, 2.64983, .52698, .82132 / 2.0,
    -.58897, -1.18223, 3.35809, .62017, .57353 / 2.0,
    -.07276, -.36214, 4.37655, .45481, .21746 / 2.0,
    .10504, -.61977, 3.54001, .04612, -.18108 / 2.0,
    .63211, -.87046, 2.47673, -.48016, -1.27157 / 2.0,
    .86505, -1.11342, 1.24612, -.82385, -2.77082 / 2.0,
    1.23606, -1.59529, .10438, -1.30206, -4.11520 / 2.0,

    5.62596/2.0, -2.75800/15.0
};

static float wr[122] = {
    .00000, -.17160, .27010, .29906, -.08471 / 2.0,
    .00000, -1.40375, -1.05121, .07217, -.01351 / 2.0,
    .00000, -1.29506, -2.16183, .13246, -1.03508 / 2.0,
    .00000, -2.29847, -2.34631, .17253, .08302 / 2.0,
    .00000, -1.27266, -2.87401, -.07456, -.34240 / 2.0,
    .00000, -1.34640, -2.46556, -.13022, -.01591 / 2.0,
    .00000, .27448, .60015, .48302, .25236 / 2.0,
    .00000, .39521, .68178, .05281, .09266 / 2.0,
    .00000, .24855, -.06844, -.37646, .05685 / 2.0,
    .00000, .17405, .00430, .74427, .00576 / 2.0,
    .00000, .12392, .31202, -.91035, -.16270 / 2.0,
    .00000, .01418, -.10839, -.02781, -.88035 / 2.0,
    .00000, 1.07274, 2.00366, 1.16242, .22520 / 2.0,
    .00000, .85631, 1.06349, 1.49549, .18966 / 2.0,
    .00000, .37183, -.50352, -.14818, .12039 / 2.0,
    .00000, .13681, .13978, 1.11245, -.12707 / 2.0,
    .00000, -.22082, .20178, -.06285, -.52728 / 2.0,
    .00000, -.13597, -.19412, -.09308, -1.26062 / 2.0,
    .00000, 3.05454, 5.16874, 1.50680, 5.35000 / 2.0,
    .00000, 2.19605, 3.85390, .88296, 2.30052 / 2.0,
    .00000, .92321, 1.08744, -.11696, -.78560 / 2.0,
    .00000, -.09795, -.83050, -1.09167, -4.94251 / 2.0,
    .00000, -1.00316, -3.66465, -2.56906, -9.67677 / 2.0,
    .00000, -2.77982, -7.26713, -3.40177, -12.32252 / 2.0,
    .00000 / 2.0, 3.42040 / 15.0
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

