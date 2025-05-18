#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
/*
    mec - match equity calculator for backgammon. calculate equity table
    given match length, gammon rate and winning probabilities.

    Copyright (C) 1996  Claes Thornberg (claest@it.kth.se)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "mec.h"

#include <vector>

struct dp
{
  double e;
  double w;
};


/* If last bit is zero, then x is even. */

static inline bool
even(int const x)
{
  return (x & 0x1) == 0;
}

/* sq returns x if x is greater than zero, else it returns zero. */

static inline int
sq(int const x)
{
  return (x > 0 ? x : 0);
}



/* Compute point when p doubles o to c, assuming gammon rate gr,
   p wins wpp % of games, and equities E for favorite.  At this
   point o does equally well passing the double as taking it. */

static dp
dpt(int p, int o, int c, double gr, double wpp, double **E)
{
  dp dpo, dpp;
  double e0, w0, edp, wdp;

  if (p <= c / 2)
    {
      /* No reason for p to double o, since a single win
	 is enough to win the match. */

      dpp.e = 1; dpp.w = 1;
      return dpp;
    }

  /* p might double o to c since he needs more than a single
     game to win the match. */

  /* Find out when o (re)doubles p to 2*c */

  dpo = dpt (o, p, 2*c, gr, 1 - wpp, E);

  /* Find out equity for o if p does well and wins
     all games here (assuming no recube from o), i.e.
     o loses all games sitting on a c-cube. */

  if( wpp > 0.5 ) {
    /* o isn't game favorite, equity for o at o-away x-away is 1 - E[x][o]. */

    e0 = (1 - E[sq (p - c)][o]) * (1 - gr)
      + (1 - E[sq (p - 2 * c)][o]) * gr;
  }
  else {
    /* o is game favorite, equity for o at o-away x-away is E[o][x]. */

    e0 =  E[o][sq (p - c)] * (1 - gr)
      + E[o][sq (p - 2 * c)] * gr;
  }

  w0 = 0;

  /* Find out o:s equity if o passes the double to c, i.e. loses c / 2 points. */

  if (wpp > 0.5) {
    /* o isn't game favorite, equity for o at o-away x-away is 1 - E[x][o]. */

    edp = 1 - E[sq (p - c / 2)][o];
  }
  else {
    /* o is game favorite, equity for o at o-away x-away is E[o][x]. */

    edp = E[o][sq (p - c / 2)];
  }

  /* Find the winning percentage, which on the line from
     (w0,e0) to (dpo.w,dpo.e) gives o an equity equal to O's
     equity  passing the double to c (i.e. losing c / 2 pts) */

  wdp = (edp - e0) * dpo.w / (dpo.e - e0);

  /* Now we know when p should double o, expressed as
     winning percentage and equity for o.  Return this
     expressed in figures for p */

  dpp.e = 1 - edp; dpp.w = 1 - wdp;
  
  return dpp;
}

static void
post_crawford(double gr, double wpf, int ml, double** E)
{
  E[1][1] = wpf;
  
  for(int i = 2; i <= ml; i++) {
    if( even (i) ) {
      /* Free drop condition exists */
      E[1][i] = E[1][i - 1];
      E[i][1] = E[i - 1][1];
    } else {

      E[1][i] =		/* Equity for favorite when 1-away, i-away */
	E[0][i] * wpf	/* Favorite wins */
	+ E[1][sq (i - 2)] * (1 - wpf) * (1 - gr) /* Favorite loses single */
	+ E[1][sq (i - 4)] * (1 - wpf) * gr; /* Favorite loses gammon */

      E[i][1] =		/* Equity for favorite when i-away, 1-away */
	E[i][0] * (1 - wpf)	/* Favorite loses */
	+ E[sq (i - 2)][1] * wpf * (1 - gr) /* Favorite wins single */
	+ E[sq (i - 4)][1] * wpf * gr; /* Favorite wins gammon */
    }
  }
}

static void
crawford(double gr, double wpf, int ml, double** E)
{
  /* Compute crawford equities.  Do this backwards, since
     we overwrite post crawford equities with crawford equities.
     In this way we only overwrite equities no longer needed. */

  for(int i = ml; i >= 2; i--)
    {
      E[1][i] =			/* Equity for favorite when 1-away,i-away */
	E[0][i] * wpf		/* Favorite wins */
	+ E[1][i - 1] * (1 - wpf) * (1 - gr) /* Favorite loses single */
	+ E[1][i - 2] * (1 - wpf) * gr; /* Favorite loses gammon */

      E[i][1] =			/* Equity for favorite when i-away, 1-away */
	E[i][0] * (1 - wpf)	/* Favorite loses */
	+ E[i - 1][1] * wpf * (1 - gr) /* Favorite wins single */
	+ E[i - 2][1] * wpf * gr; /* Favorite wins gammon */
    }
}

void pre_crawford (double gr, double wpf, int ml, double **E)
{
  int i, j;
  dp dpf, dpu;
  double eq;

  for (i = 2; i <= ml; i++)
    for (j = i; j <= ml; j++)
      {
	dpf = dpt (i, j, 2, gr, wpf, E);
	dpu = dpt (j, i, 2, gr, 1 - wpf, E);

	dpu.e = 1 - dpu.e;
	dpu.w = 1 - dpu.w;

	eq = dpu.e
	  + (dpf.e - dpu.e) * (wpf - dpu.w) / (dpf.w - dpu.w);

	E[i][j] = eq;

	if (i != j)
	  {
	    dpf = dpt (j, i, 2, gr, wpf, E);
	    dpu = dpt (i, j, 2, gr, 1 - wpf, E);

	    dpu.e = 1 - dpu.e;
	    dpu.w = 1 - dpu.w;

	    eq = dpu.e
	      + (dpf.e - dpu.e) * (wpf - dpu.w) / (dpf.w - dpu.w);

	    E[j][i] = eq;
	  }
      }
}

/*
  Arguments are (in this order):
     match length
     gammon rate
     winning percentage (favorite)
 */


void
eqTable(int ml, float** T, double gr, double wpf)
{
  /* ml match length */

  /* gr gammon rate, i.e. how many of games won/lost will be gammons */

  /* Here one could argue for different approaches.  Does the underdog
     in match (current score) have different gammon rate.  Or does the
     underdog in match (equity at current score) have different gammon
     rate.  This could be solved, but for now, we assume same rates. */

  /* winning percentage for favorite in one game.  NB: when one player
     is favorite, there is no symmetry in the equity table! I.e.
     E[i][1] = 1 - E[1][i] doesn't necessarily hold.  This figure must
     be higher than or equal to .5, i.e. 50% */

  /* Equity chart, E[p][o] = equity for p when p is p away, and o is o away.
     If there is a game favorite, i.e. wpf > 0.5, p above is considered
     to be the favorite in each game.  For now we also assume no free drop
     vigourish.  This will only affect the computation of post crawford
     equities, and is quite easy to correct.  */

    std::vector<double*> E(ml + 1);
    std::vector<double> ec((ml + 1) * (ml + 1));

  {
    int i;

    /* Initialize E to point to correct positions */

    for (i = 0; i <= ml; i++)
      {
	    E[i] = & ec[i * (ml + 1)];
      }
    
    /* Initialize E for 0-away scores */

    for (i = 1;  i <= ml; i++)
      {
	E[0][i] = 1;
	E[i][0] = 0;
      }
  }

  bool rev = false;
  if( wpf < 0.5 ) {
    wpf = 1 - wpf;
    rev = true;
  }
  
  /* Compute post crawford equities, given gammon rate, winning percentage
     for favorite (game), match length.  Fill in the equity table. */

  post_crawford (gr, wpf, ml, E.data());

  /* Compute crawford equities, given gammon rate, winning percentage
     for favorite (game), match length, and post crawford equities.
     Fill in the table. */

  crawford (gr, wpf, ml, E.data());

  /* Compute pre-crawford equities,  given gammon rate, winning percentage
     for favorite (game), match length, and crawford equities.
     Fill in the table. */

  pre_crawford (gr, wpf, ml, E.data());

  for(int i = 1; i <= ml; i++) {
    for(int j = 1; j <= ml; j++) {
      T[i-1][j-1] = rev ? 1 - E[j][i] : E[i][j];
    }
  }
}
