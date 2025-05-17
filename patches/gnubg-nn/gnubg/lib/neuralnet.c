/*
 * neuralnet.c
 *
 * by Gary Wong, 1998
 * by Joseph Heled <joseph@gnubg.org>, 2001
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

#include <errno.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memcpy */

#include "neuralnet.h"
#include "sse.h"

extern unsigned long genrand( void );
#define random genrand

// #define SIGMOIDNEW 0

#if ! SIGMOIDNEW
#include "sigmoid.h"
#endif

#if 0
inline float oldsigmoid( float xin ) {

  float const x = xin < 0 ? -xin : xin;
  float const v = 1.0/(2 + x + x*x * (1.0/2 + 1.0/3 + 1.0/4 + 1.0/5));

  return xin < 0 ? 1.0 - v : v;
}
#define sigmoid oldsigmoid
#endif

#if SIGMOIDNEW

#define SIG_Q 10.0f    /* reciprocal of precision step */
#define SIG_MAX 200    /* half number of entries in lookup table */
/* note: the lookup table covers the interval
   [ -SIG_MAX/SIG_Q  to  +SIG_MAX/SIG_Q ] */

static float Sig[2*SIG_MAX+1] =
{
0.999999997939 , /* -200 */
0.999999997722 , /* -199 */
0.999999997483 , /* -198 */
0.999999997218 , /* -197 */
0.999999996925 , /* -196 */
0.999999996602 , /* -195 */
0.999999996244 , /* -194 */
0.999999995849 , /* -193 */
0.999999995413 , /* -192 */
0.99999999493 , /* -191 */
0.999999994397 , /* -190 */
0.999999993808 , /* -189 */
0.999999993157 , /* -188 */
0.999999992437 , /* -187 */
0.999999991642 , /* -186 */
0.999999990763 , /* -185 */
0.999999989791 , /* -184 */
0.999999988717 , /* -183 */
0.999999987531 , /* -182 */
0.999999986219 , /* -181 */
0.99999998477 , /* -180 */
0.999999983168 , /* -179 */
0.999999981398 , /* -178 */
0.999999979442 , /* -177 */
0.99999997728 , /* -176 */
0.99999997489 , /* -175 */
0.999999972249 , /* -174 */
0.999999969331 , /* -173 */
0.999999966105 , /* -172 */
0.99999996254 , /* -171 */
0.999999958601 , /* -170 */
0.999999954247 , /* -169 */
0.999999949435 , /* -168 */
0.999999944117 , /* -167 */
0.999999938239 , /* -166 */
0.999999931744 , /* -165 */
0.999999924565 , /* -164 */
0.999999916632 , /* -163 */
0.999999907864 , /* -162 */
0.999999898174 , /* -161 */
0.999999887465 , /* -160 */
0.999999875629 , /* -159 */
0.999999862549 , /* -158 */
0.999999848093 , /* -157 */
0.999999832117 , /* -156 */
0.999999814461 , /* -155 */
0.999999794948 , /* -154 */
0.999999773382 , /* -153 */
0.999999749548 , /* -152 */
0.999999723208 , /* -151 */
0.999999694098 , /* -150 */
0.999999661926 , /* -149 */
0.99999962637 , /* -148 */
0.999999587075 , /* -147 */
0.999999543648 , /* -146 */
0.999999495653 , /* -145 */
0.99999944261 , /* -144 */
0.999999383989 , /* -143 */
0.999999319202 , /* -142 */
0.999999247602 , /* -141 */
0.999999168472 , /* -140 */
0.999999081019 , /* -139 */
0.99999898437 , /* -138 */
0.999998877555 , /* -137 */
0.999998759506 , /* -136 */
0.999998629043 , /* -135 */
0.999998484858 , /* -134 */
0.99999832551 , /* -133 */
0.999998149402 , /* -132 */
0.999997954774 , /* -131 */
0.999997739676 , /* -130 */
0.999997501956 , /* -129 */
0.999997239235 , /* -128 */
0.999996948884 , /* -127 */
0.999996627996 , /* -126 */
0.999996273361 , /* -125 */
0.999995881428 , /* -124 */
0.999995448276 , /* -123 */
0.99999496957 , /* -122 */
0.999994440518 , /* -121 */
0.999993855825 , /* -120 */
0.999993209641 , /* -119 */
0.999992495498 , /* -118 */
0.99999170625 , /* -117 */
0.999990833996 , /* -116 */
0.999989870009 , /* -115 */
0.99998880464 , /* -114 */
0.999987627229 , /* -113 */
0.999986325991 , /* -112 */
0.999984887905 , /* -111 */
0.999983298578 , /* -110 */
0.999981542107 , /* -109 */
0.999979600913 , /* -108 */
0.99997745557 , /* -107 */
0.999975084611 , /* -106 */
0.999972464309 , /* -105 */
0.999969568443 , /* -104 */
0.999966368036 , /* -103 */
0.999962831063 , /* -102 */
0.999958922132 , /* -101 */
0.999954602131 , /* -100 */
0.999949827835 , /* -99 */
0.999944551475 , /* -98 */
0.99993872026 , /* -97 */
0.99993227585 , /* -96 */
0.999925153772 , /* -95 */
0.999917282777 , /* -94 */
0.999908584126 , /* -93 */
0.999898970806 , /* -92 */
0.999888346659 , /* -91 */
0.999876605424 , /* -90 */
0.999863629673 , /* -89 */
0.999849289642 , /* -88 */
0.999833441935 , /* -87 */
0.999815928095 , /* -86 */
0.999796573022 , /* -85 */
0.99977518323 , /* -84 */
0.999751544918 , /* -83 */
0.999725421844 , /* -82 */
0.99969655297 , /* -81 */
0.99966464987 , /* -80 */
0.999629393859 , /* -79 */
0.999590432835 , /* -78 */
0.999547377777 , /* -77 */
0.999499798893 , /* -76 */
0.999447221363 , /* -75 */
0.999389120641 , /* -74 */
0.999324917269 , /* -73 */
0.999253971166 , /* -72 */
0.999175575314 , /* -71 */
0.999088948806 , /* -70 */
0.99899322918 , /* -69 */
0.998887463967 , /* -68 */
0.998770601379 , /* -67 */
0.99864148005 , /* -66 */
0.998498817743 , /* -65 */
0.99834119892 , /* -64 */
0.998167061058 , /* -63 */
0.997974679611 , /* -62 */
0.997762151479 , /* -61 */
0.997527376843 , /* -60 */
0.997268039237 , /* -59 */
0.996981583675 , /* -58 */
0.996665192693 , /* -57 */
0.996315760101 , /* -56 */
0.995929862284 , /* -55 */
0.995503726839 , /* -54 */
0.99503319835 , /* -53 */
0.994513701101 , /* -52 */
0.993940198508 , /* -51 */
0.993307149076 , /* -50 */
0.992608458656 , /* -49 */
0.991837428847 , /* -48 */
0.990986701347 , /* -47 */
0.990048198133 , /* -46 */
0.989013057369 , /* -45 */
0.987871565016 , /* -44 */
0.986613082172 , /* -43 */
0.985225968307 , /* -42 */
0.983697500629 , /* -41 */
0.982013790038 , /* -40 */
0.980159694266 , /* -39 */
0.978118729064 , /* -38 */
0.975872978582 , /* -37 */
0.973403006423 , /* -36 */
0.970687769249 , /* -35 */
0.967704535302 , /* -34 */
0.964428810727 , /* -33 */
0.960834277203 , /* -32 */
0.956892745059 , /* -31 */
0.952574126822 , /* -30 */
0.947846436922 , /* -29 */
0.942675824101 , /* -28 */
0.937026643943 , /* -27 */
0.930861579657 , /* -26 */
0.924141819979 , /* -25 */
0.916827303506 , /* -24 */
0.908877038985 , /* -23 */
0.90024951088 , /* -22 */
0.890903178804 , /* -21 */
0.880797077978 , /* -20 */
0.869891525637 , /* -19 */
0.8581489351 , /* -18 */
0.845534734916 , /* -17 */
0.832018385134 , /* -16 */
0.817574476194 , /* -15 */
0.802183888559 , /* -14 */
0.785834983043 , /* -13 */
0.768524783499 , /* -12 */
0.750260105595 , /* -11 */
0.73105857863 , /* -10 */
0.710949502625 , /* -9 */
0.689974481128 , /* -8 */
0.668187772168 , /* -7 */
0.645656306226 , /* -6 */
0.622459331202 , /* -5 */
0.598687660112 , /* -4 */
0.574442516812 , /* -3 */
0.549833997312 , /* -2 */
0.524979187479 , /* -1 */
0.5 , /* 0 */
0.475020812521 , /* 1 */
0.450166002688 , /* 2 */
0.425557483188 , /* 3 */
0.401312339888 , /* 4 */
0.377540668798 , /* 5 */
0.354343693774 , /* 6 */
0.331812227832 , /* 7 */
0.310025518872 , /* 8 */
0.289050497375 , /* 9 */
0.26894142137 , /* 10 */
0.249739894405 , /* 11 */
0.231475216501 , /* 12 */
0.214165016957 , /* 13 */
0.197816111441 , /* 14 */
0.182425523806 , /* 15 */
0.167981614866 , /* 16 */
0.154465265084 , /* 17 */
0.1418510649 , /* 18 */
0.130108474363 , /* 19 */
0.119202922022 , /* 20 */
0.109096821196 , /* 21 */
0.0997504891197 , /* 22 */
0.0911229610149 , /* 23 */
0.0831726964939 , /* 24 */
0.0758581800212 , /* 25 */
0.0691384203433 , /* 26 */
0.062973356057 , /* 27 */
0.0573241758989 , /* 28 */
0.0521535630784 , /* 29 */
0.0474258731776 , /* 30 */
0.0431072549411 , /* 31 */
0.0391657227968 , /* 32 */
0.0355711892726 , /* 33 */
0.0322954646985 , /* 34 */
0.0293122307514 , /* 35 */
0.0265969935769 , /* 36 */
0.0241270214177 , /* 37 */
0.0218812709361 , /* 38 */
0.0198403057341 , /* 39 */
0.0179862099621 , /* 40 */
0.0163024993714 , /* 41 */
0.0147740316933 , /* 42 */
0.0133869178277 , /* 43 */
0.0121284349843 , /* 44 */
0.0109869426306 , /* 45 */
0.0099518018669 , /* 46 */
0.00901329865285 , /* 47 */
0.00816257115316 , /* 48 */
0.00739154134428 , /* 49 */
0.00669285092428 , /* 50 */
0.00605980149158 , /* 51 */
0.00548629889945 , /* 52 */
0.00496680165006 , /* 53 */
0.00449627316094 , /* 54 */
0.0040701377159 , /* 55 */
0.00368423989944 , /* 56 */
0.00333480730741 , /* 57 */
0.00301841632471 , /* 58 */
0.00273196076301 , /* 59 */
0.00247262315663 , /* 60 */
0.00223784852128 , /* 61 */
0.00202532038905 , /* 62 */
0.00183293894249 , /* 63 */
0.00165880108017 , /* 64 */
0.00150118225674 , /* 65 */
0.00135851995043 , /* 66 */
0.00122939862128 , /* 67 */
0.00111253603286 , /* 68 */
0.00100677082009 , /* 69 */
0.000911051194401 , /* 70 */
0.000824424686398 , /* 71 */
0.000746028833837 , /* 72 */
0.000675082730633 , /* 73 */
0.000610879359434 , /* 74 */
0.000552778636924 , /* 75 */
0.00050020110708 , /* 76 */
0.000452622223241 , /* 77 */
0.000409567164986 , /* 78 */
0.000370606140626 , /* 79 */
0.000335350130466 , /* 80 */
0.000303447030029 , /* 81 */
0.000274578156101 , /* 82 */
0.000248455081839 , /* 83 */
0.000224816770233 , /* 84 */
0.000203426978055 , /* 85 */
0.000184071904963 , /* 86 */
0.000166558064777 , /* 87 */
0.00015071035806 , /* 88 */
0.000136370327079 , /* 89 */
0.000123394575986 , /* 90 */
0.00011165334063 , /* 91 */
0.000101029193908 , /* 92 */
9.14158738522e-05 , /* 93 */
8.27172228517e-05 , /* 94 */
7.48462275106e-05 , /* 95 */
6.77241496198e-05 , /* 96 */
6.12797396166e-05 , /* 97 */
5.54485247228e-05 , /* 98 */
5.01721646838e-05 , /* 99 */
4.53978687024e-05 , /* 100 */
4.10778677648e-05 , /* 101 */
3.71689371029e-05 , /* 102 */
3.36319640387e-05 , /* 103 */
3.04315569006e-05 , /* 104 */
2.75356911146e-05 , /* 105 */
2.49153889394e-05 , /* 106 */
2.25444296504e-05 , /* 107 */
2.03990872799e-05 , /* 108 */
1.84578932957e-05 , /* 109 */
1.67014218481e-05 , /* 110 */
1.5112095441e-05 , /* 111 */
1.36740090846e-05 , /* 112 */
1.23727711744e-05 , /* 113 */
1.11953595051e-05 , /* 114 */
1.01299909809e-05 , /* 115 */
9.16600371985e-06 , /* 116 */
8.29375037389e-06 , /* 117 */
7.50450159711e-06 , /* 118 */
6.7903586981e-06 , /* 119 */
6.14417460221e-06 , /* 120 */
5.55948233363e-06 , /* 121 */
5.03043030176e-06 , /* 122 */
4.5517237448e-06 , /* 123 */
4.11857174483e-06 , /* 124 */
3.72663928419e-06 , /* 125 */
3.37200386369e-06 , /* 126 */
3.0511162487e-06 , /* 127 */
2.76076495019e-06 , /* 128 */
2.49804408563e-06 , /* 129 */
2.2603242979e-06 , /* 130 */
2.04522644156e-06 , /* 131 */
1.85059777286e-06 , /* 132 */
1.67449040551e-06 , /* 133 */
1.51514181649e-06 , /* 134 */
1.37095720686e-06 , /* 135 */
1.24049354113e-06 , /* 136 */
1.12244510535e-06 , /* 137 */
1.0156304395e-06 , /* 138 */
9.18980513372e-07 , /* 139 */
8.31528027664e-07 , /* 140 */
7.52397733114e-07 , /* 141 */
6.80797670912e-07 , /* 142 */
6.16011246662e-07 , /* 143 */
5.57390058586e-07 , /* 144 */
5.04347408201e-07 , /* 145 */
4.56352428533e-07 , /* 146 */
4.1292477108e-07 , /* 147 */
3.73629798389e-07 , /* 148 */
3.38074234096e-07 , /* 149 */
3.05902226926e-07 , /* 150 */
2.7679178924e-07 , /* 151 */
2.50451574507e-07 , /* 152 */
2.26617961421e-07 , /* 153 */
2.05052415515e-07 , /* 154 */
1.85539101837e-07 , /* 155 */
1.67882724815e-07 , /* 156 */
1.51906573681e-07 , /* 157 */
1.37450753899e-07 , /* 158 */
1.24370586892e-07 , /* 159 */
1.12535162055e-07 , /* 160 */
1.01826026563e-07 , /* 161 */
9.21359998566e-08 , /* 162 */
8.33681009494e-08 , /* 163 */
7.54345778081e-08 , /* 164 */
6.82560291045e-08 , /* 165 */
6.17606095414e-08 , /* 166 */
5.58833108022e-08 , /* 167 */
5.05653109265e-08 , /* 168 */
4.57533856011e-08 , /* 169 */
4.13993754739e-08 , /* 170 */
3.74597041597e-08 , /* 171 */
3.38949421131e-08 , /* 172 */
3.0669412005e-08 , /* 173 */
2.77508316523e-08 , /* 174 */
2.51099909269e-08 , /* 175 */
2.27204594115e-08 , /* 176 */
2.0558321875e-08 , /* 177 */
1.86019389209e-08 , /* 178 */
1.68317304134e-08 , /* 179 */
1.52299795128e-08 , /* 180 */
1.3780655359e-08 , /* 181 */
1.24692526303e-08 , /* 182 */
1.12826463682e-08 , /* 183 */
1.02089606194e-08 , /* 184 */
9.23744957664e-09 , /* 185 */
8.35839003151e-09 , /* 186 */
7.56298406107e-09 , /* 187 */
6.84327097539e-09 , /* 188 */
6.19204764432e-09 , /* 189 */
5.60279640615e-09 , /* 190 */
5.06961983662e-09 , /* 191 */
4.58718172561e-09 , /* 192 */
4.15065367047e-09 , /* 193 */
3.75566675183e-09 , /* 194 */
3.39826780795e-09 , /* 195 */
3.07487987013e-09 , /* 196 */
2.78226636327e-09 , /* 197 */
2.5174987131e-09 , /* 198 */
2.27792703602e-09 , /* 199 */
2.06115361819e-09 , /* 200 */
/*1.86500891842e-09 ,  201 */
};


static inline float
sigmoid (float const x)
{
  float x2 = x * SIG_Q;
  int i = x2;

  if (i > - SIG_MAX) {
    if (i < SIG_MAX) {
      float a = Sig[i+SIG_MAX];
      float b = Sig[i+SIG_MAX+1];
      return a + (b - a) * (x2 - i);
    } else {
      return Sig[2*SIG_MAX];
    }
  } else {
    return Sig[0];
  }
}

#endif

extern int
NeuralNetCreate(neuralnet* pnn, int cInput, int cHidden,
		int cOutput, float rBetaHidden, float rBetaOutput )
{
  int i;
  float *pf;
    
  pnn->cInput = cInput;
  pnn->cHidden = cHidden;
  pnn->cOutput = cOutput;
  pnn->rBetaHidden = rBetaHidden;
  pnn->rBetaOutput = rBetaOutput;
  pnn->nTrained = 0;

  if( !( pnn->arHiddenWeight = sse_malloc( cHidden * cInput * sizeof( float ) ) ) )
    return -1;

  if( !( pnn->arOutputWeight = sse_malloc( cOutput * cHidden *
				       sizeof( float ) ) ) ) {
    sse_free( pnn->arHiddenWeight );
    return -1;
  }
    
  if( !( pnn->arHiddenThreshold = sse_malloc( cHidden * sizeof( float ) ) ) ) {
    sse_free( pnn->arOutputWeight );
    sse_free( pnn->arHiddenWeight );
    return -1;
  }

  if( !( pnn->arOutputThreshold = sse_malloc( cOutput * sizeof( float ) ) ) ) {
    sse_free( pnn->arHiddenThreshold );
    sse_free( pnn->arOutputWeight );
    sse_free( pnn->arHiddenWeight );
    return -1;
  }

  pnn->savedBase = malloc(cHidden * sizeof( Intermediate ) );
  pnn->savedIBase = malloc(cInput * sizeof( float ) );
  
  for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
    *pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
  for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
    *pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
  for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
    *pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
  for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
    *pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;

  return 0;
}

extern void
NeuralNetInit(neuralnet* pnn)
{
  pnn->arHiddenWeight = 0;
  pnn->arOutputWeight = 0;
  pnn->arHiddenThreshold = 0;
  pnn->arOutputThreshold = 0;
  pnn->savedBase = 0;   pnn->savedIBase = 0;

  pnn->nEvals = 0;
}

extern int
NeuralNetDestroy(neuralnet* pnn)
{
  pnn->cOutput = pnn->cInput = pnn->cHidden = pnn->nTrained = 0;
  
  sse_free( pnn->arHiddenWeight ); pnn->arHiddenWeight = 0;
  sse_free( pnn->arOutputWeight ); pnn->arOutputWeight = 0;
  sse_free( pnn->arHiddenThreshold ); pnn->arHiddenThreshold = 0;
  sse_free( pnn->arOutputThreshold ); pnn->arOutputThreshold = 0;

  free(pnn->savedBase); pnn->savedBase = 0;
  free(pnn->savedIBase); pnn->savedIBase = 0;
  
/*    free( pnn->arHiddenWeightt ); */
    
  return 0;
}



static int
Evaluate(neuralnet* pnn, float arInput[], Intermediate ar[], float arOutput[],
	 Intermediate* saveAr)
{
  int i, j;
  float *prWeight;

  /* Calculate activity at hidden nodes */
  
  for(i = 0; i < pnn->cHidden; ++i) {
    ar[i] = pnn->arHiddenThreshold[i];
  }

  prWeight = pnn->arHiddenWeight;
  
  for(i = 0; i < pnn->cInput; ++i) {
    float const ari = arInput[i];

    if( ari == 0 ) {
      prWeight += pnn->cHidden;
    } else {
      Intermediate* pr = ar;
      
      if( ari == 1.0 ) {
	for( j = pnn->cHidden; j; j-- ) {
	  *pr++ += *prWeight++;
	}
      }
      else {
	for( j = pnn->cHidden; j; j-- ) {
	  *pr++ += *prWeight++ * ari;
	}
      }
    }
  }

  if( saveAr ) {
    memcpy(saveAr, ar, pnn->cHidden * sizeof(*saveAr));
  }
  
  for( i = 0; i < pnn->cHidden; i++ ) {
    ar[i] = sigmoid( -pnn->rBetaHidden * ar[i] );
  }
  
  /* Calculate activity at output nodes */
  
  prWeight = pnn->arOutputWeight;

  for( i = 0; i < pnn->cOutput; i++ ) {
    float r = pnn->arOutputThreshold[ i ];
    
    for( j = 0; j < pnn->cHidden; j++ )
      r += ar[ j ] * *prWeight++;

    arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
  }

  return 0;
}

#if 0
static int
EvaluateFromBase(neuralnet* pnn, float arInputDif[], Intermediate ar[],
		 float arOutput[])
{
  int i, j;
  float *prWeight;

  /* Calculate activity at hidden nodes */
  
  prWeight = pnn->arHiddenWeight;
  
  for(i = 0; i < pnn->cInput; ++i) {
    float const ari = arInputDif[i];

    if( ari == 0 ) {
      prWeight += pnn->cHidden;
    } else {
      Intermediate* pr = ar;
      
      if( ari == 1.0 ) {
	for( j = pnn->cHidden; j; j-- ) {
	  *pr++ += *prWeight++;
	}
      }
      else if( ari == -1.0 ) {
	for( j = pnn->cHidden; j; j-- ) {
	  *pr++ -= *prWeight++;
	}
      } else {
	for( j = pnn->cHidden; j; j-- ) {
	  *pr++ += *prWeight++ * ari;
	}
      }
    }
  }
  
  for( i = 0; i < pnn->cHidden; i++ ) {
    ar[i] = sigmoid( -pnn->rBetaHidden * ar[i] );
  }
  
  /* Calculate activity at output nodes */
  
  prWeight = pnn->arOutputWeight;

  for( i = 0; i < pnn->cOutput; i++ ) {
    float r = pnn->arOutputThreshold[i];
    
    for( j = 0; j < pnn->cHidden; j++ )
      r += ar[ j ] * *prWeight++;

    arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
  }

  return 0;
}

#endif

int fuseSSE = 0;

extern int
NeuralNetEvaluate(neuralnet* pnn, float arInput[], float arOutput[]
		  /*,NNEvalType t*/)
{
  ++ pnn->nEvals;    
  if( fuseSSE ) {
    NeuralNetEvaluateSSE(pnn, arInput, arOutput);
  } else {
#ifdef _MSC_VER
      Intermediate* ar = (Intermediate*) malloc(pnn->cHidden * sizeof(Intermediate));
            if (!ar) return -1;
            Evaluate(pnn, arInput, ar, arOutput, 0);
            free(ar);
#else
      Intermediate ar[pnn->cHidden];
      Evaluate(pnn, arInput, ar, arOutput, 0);
#endif
#if 0
    switch( t ) {
      case NNEVAL_NONE:
      {
	Evaluate(pnn, arInput, ar, arOutput, 0);
	break;
      }
      case NNEVAL_SAVE:
      {
	memcpy(pnn->savedIBase, arInput, pnn->cInput * sizeof(*ar));
	Evaluate(pnn, arInput, ar, arOutput, pnn->savedBase);
	break;
      }
      case NNEVAL_FROMBASE:
      {
	int i;
      
	memcpy(ar, pnn->savedBase, pnn->cHidden * sizeof(*ar));

	{
	  float* r = arInput;
	  float* s = pnn->savedIBase;
	
	  for(i = 0; i < pnn->cInput; ++i, ++r, ++s) {
	    if( *r != *s ) {
	      *r -= *s;
	    } else {
	      *r = 0.0;
	    }
	  }
	}
	EvaluateFromBase(pnn, arInput, ar, arOutput);
	break;
      }
    }
#endif
  }
    
  return 0;
}

extern int
NeuralNetTrain(neuralnet* pnn, float arInput[], float arOutput[],
               float arDesired[], float rAlpha)
{
    int i, j;
    Intermediate* ar = malloc(pnn->cHidden * sizeof(Intermediate));
    float* arOutputError = malloc(pnn->cOutput * sizeof(float));
    float* arHiddenError = malloc(pnn->cHidden * sizeof(float));
    float* pr = NULL;
    float* prWeight = NULL;

    if (!ar || !arOutputError || !arHiddenError) {
        free(ar);
        free(arOutputError);
        free(arHiddenError);
        return -1;
    }

  Evaluate(pnn, arInput, ar, arOutput, 0);

  /* Calculate error at output nodes */
  for( i = 0; i < pnn->cOutput; i++ )
    arOutputError[ i ] = ( arDesired[ i ] - arOutput[ i ] ) *
      pnn->rBetaOutput * (arOutput[i] * ( 1 - arOutput[i] ) /*+ 0.1*/);

  /* Calculate error at hidden nodes */
  for( i = 0; i < pnn->cHidden; i++ )
    arHiddenError[ i ] = 0.0;

  prWeight = pnn->arOutputWeight;
    
  for( i = 0; i < pnn->cOutput; i++ )
    for( j = 0; j < pnn->cHidden; j++ )
      arHiddenError[j] += arOutputError[i] * *prWeight++;

  for( i = 0; i < pnn->cHidden; i++ )
    arHiddenError[i] *= pnn->rBetaHidden * ar[i] * (1 - ar[i]);

    /* Adjust weights at output nodes */
  prWeight = pnn->arOutputWeight;

  {
    float a = rAlpha; // rAlpha/pnn->cHidden;
      
    for( i = 0; i < pnn->cOutput; i++ ) {
      for( j = 0; j < pnn->cHidden; j++ )
	*prWeight++ += a * arOutputError[ i ] * ar[ j ];

      pnn->arOutputThreshold[ i ] += a * arOutputError[ i ];
    }
  }
    
  /* Adjust weights at hidden nodes */
  for( i = 0; i < pnn->cInput; i++ )
    if( arInput[ i ] == 1.0 )
      for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
	     j = pnn->cHidden, pr = arHiddenError; j; j-- )
	*prWeight++ += rAlpha * *pr++;
    else if( arInput[ i ] )
      for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
	     j = pnn->cHidden, pr = arHiddenError; j; j-- )
	*prWeight++ += rAlpha * *pr++ * arInput[ i ];

  for( i = 0; i < pnn->cHidden; i++ )
    pnn->arHiddenThreshold[ i ] += rAlpha * arHiddenError[ i ];

  pnn->nTrained++;

    free(ar);
    free(arOutputError);
    free(arHiddenError);
    return 0;
}

extern int
NeuralNetTrainS(neuralnet* pnn, float arInput[], float arOutput[],
		float arDesired[], float rAlpha, CONST int* tList)
{
  int i, j;    
  Intermediate ar[ pnn->cHidden ];
  
  float
    arOutputError[ pnn->cOutput ],
    arHiddenError[ pnn->cHidden ],
    *pr,
    *prWeight;

  Evaluate(pnn, arInput, ar, arOutput, 0);

  /* Calculate error at output nodes */
  for( i = 0; i < pnn->cOutput; i++ )
    arOutputError[ i ] = ( arDesired[ i ] - arOutput[ i ] ) *
      pnn->rBetaOutput * (arOutput[i] * ( 1 - arOutput[i] ) /*+ 0.1*/);

  /* Calculate error at hidden nodes */
  for( i = 0; i < pnn->cHidden; i++ )
    arHiddenError[ i ] = 0.0;

  prWeight = pnn->arOutputWeight;
    
  for( i = 0; i < pnn->cOutput; i++ )
    for( j = 0; j < pnn->cHidden; j++ )
      arHiddenError[j] += arOutputError[i] * *prWeight++;

  for( i = 0; i < pnn->cHidden; i++ )
    arHiddenError[i] *= pnn->rBetaHidden * ar[i] * (1 - ar[i]);

  /* Adjust weights at output nodes */
#if 0
  prWeight = pnn->arOutputWeight;

  {
    float a = rAlpha; // rAlpha/pnn->cHidden;
      
    for( i = 0; i < pnn->cOutput; i++ ) {
      for( j = 0; j < pnn->cHidden; j++ )
	*prWeight++ += a * arOutputError[ i ] * ar[ j ];

      pnn->arOutputThreshold[ i ] += a * arOutputError[ i ];
    }
  }
#endif
  
  /* Adjust weights at hidden nodes */
  //for( i = 0; i < pnn->cInput; i++ ) {

  

  {
    int k = 0;
    while( tList[k] >= 0 ) {
      i = tList[k];
      {                                       assert( 0 <= i && i < pnn->cInput ); }
      ++k;
    
      if( arInput[ i ] == 1.0 ) {
	prWeight = pnn->arHiddenWeight + i * pnn->cHidden;
	pr = arHiddenError;
	for(j = pnn->cHidden; j; j-- ) {
	  *prWeight++ += rAlpha * *pr++;
	}
      }
      else if( arInput[ i ] ) {
	prWeight = pnn->arHiddenWeight + i * pnn->cHidden;
	pr = arHiddenError;
	for( j = pnn->cHidden; j; j-- ) {
	  *prWeight++ += rAlpha * *pr++ * arInput[ i ];
	}
      }
    }
  }
  
  for( i = 0; i < pnn->cHidden; i++ ) {
    pnn->arHiddenThreshold[ i ] += rAlpha * arHiddenError[ i ];
  }

  pnn->nTrained++;
    
  return 0;
}

extern int
NeuralNetResize(neuralnet* pnn, int cInput, int cHidden, int cOutput)
{
  int i, j;
  float *pr, *prNew;

  if( cHidden != pnn->cHidden ) {
    if( !( pnn->arHiddenThreshold = realloc( pnn->arHiddenThreshold,
					     cHidden * sizeof( float ) ) ) )
      return -1;

    for( i = pnn->cHidden; i < cHidden; i++ )
      pnn->arHiddenThreshold[ i ] = ( ( random() & 0xFFFF ) - 0x8000 ) /
	131072.0;
  }
    
  if( cHidden != pnn->cHidden || cInput != pnn->cInput ) {
    if( !( pr = malloc( cHidden * cInput * sizeof( float ) ) ) )
      return -1;

    prNew = pr;
	
    for( i = 0; i < cInput; i++ )
      for( j = 0; j < cHidden; j++ )
	if( j >= pnn->cHidden )
	  *prNew++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
	else if( i >= pnn->cInput )
	  *prNew++ = ( ( random() & 0x0FFF ) - 0x0800 ) / 131072.0;
	else
	  *prNew++ = pnn->arHiddenWeight[ i * pnn->cHidden + j ];
		    
    free( pnn->arHiddenWeight );

    pnn->arHiddenWeight = pr;
  }
	
  if( cOutput != pnn->cOutput ) {
    if( !( pnn->arOutputThreshold = realloc( pnn->arOutputThreshold,
					     cOutput * sizeof( float ) ) ) )
      return -1;

    for( i = pnn->cOutput; i < cOutput; i++ )
      pnn->arOutputThreshold[ i ] = ( ( random() & 0xFFFF ) - 0x8000 ) /
	131072.0;
  }
    
  if( cOutput != pnn->cOutput || cHidden != pnn->cHidden ) {
    if( !( pr = malloc( cOutput * cHidden * sizeof( float ) ) ) )
      return -1;

    prNew = pr;
	
    for( i = 0; i < cHidden; i++ )
      for( j = 0; j < cOutput; j++ )
	if( j >= pnn->cOutput )
	  *prNew++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
	else if( i >= pnn->cHidden )
	  *prNew++ = ( ( random() & 0x0FFF ) - 0x0800 ) / 131072.0;
	else
	  *prNew++ = pnn->arOutputWeight[ i * pnn->cOutput + j ];
		    
    free( pnn->arOutputWeight );

    pnn->arOutputWeight = pr;
  }

  pnn->cInput = cInput;
  pnn->cHidden = cHidden;
  pnn->cOutput = cOutput;
    
  return 0;
}

extern int
NeuralNetLoad(neuralnet* pnn, FILE *pf)
{
  int i;
  unsigned int nTrained;
  float *pr;
    
  if( fscanf( pf, "%d %d %d %d %f %f\n", &pnn->cInput, &pnn->cHidden,
	      &pnn->cOutput, &nTrained, &pnn->rBetaHidden,
	      &pnn->rBetaOutput ) < 5 || pnn->cInput < 1 ||
      pnn->cHidden < 1 || pnn->cOutput < 1 ||
      pnn->rBetaHidden <= 0.0 || pnn->rBetaOutput <= 0.0 ) {
    errno = EINVAL;

    return -1;
  }

  if( NeuralNetCreate( pnn, pnn->cInput, pnn->cHidden, pnn->cOutput,
		       pnn->rBetaHidden, pnn->rBetaOutput ) ) {
    return -1;
  }

  pnn->nTrained = nTrained;

  {
/*      long* prl = pnn->arHiddenWeightl; */

    pr = pnn->arHiddenWeight;
    for(i = 0; i < pnn->cInput; ++i) {
      int j;
      for(j = 0; j < pnn->cHidden; ++j) {
    
	if( fscanf( pf, "%f\n", pr++ ) < 1 ) {
	  return -1;
	}

/*  	pnn->arHiddenWeightt[ j* pnn->cInput + i] = pr[-1]; */
      }
    }
      
/*        *prl++ = (long)(factor * pr[-1] + 0.5); */
  }

  {
/*      long* prl = pnn->arOutputWeightl; */
    
    for( i = pnn->cHidden * pnn->cOutput, pr = pnn->arOutputWeight; i; i-- ) {
      if( fscanf( pf, "%f\n", pr++ ) < 1 ) {
	return -1;
      }
      
/*        *prl++ = (long)(factor * pr[-1] + 0.5); */
    }
  }
  
  {
/*      long* prl = pnn->arHiddenThresholdl; */
      
    for( i = pnn->cHidden, pr = pnn->arHiddenThreshold; i; i-- ) {
      if( fscanf( pf, "%f\n", pr++ ) < 1 ) {
	return -1;
      }
/*        *prl++ = (long)(factor * pr[-1] + 0.5); */
    }
  }

  {
/*      long* prl = pnn->arOutputThresholdl; */

    for( i = pnn->cOutput, pr = pnn->arOutputThreshold; i; i-- ) {
      if( fscanf( pf, "%f\n", pr++ ) < 1 ) {
	return -1;
      }
/*        *prl++ = (long)(factor * pr[-1] + 0.5); */
    }
  }

  for(i = 0; i < pnn->cHidden; ++i) {
    pnn->savedBase[i] = 0.0;
  }
  for(i = 0; i < pnn->cInput; ++i) {
    pnn->savedIBase[i] = 0.0;
  }
  
  return 0;
}

extern int
NeuralNetSave(neuralnet* pnn, FILE *pf)
{
  int i;
  float *pr;
    
  if( fprintf( pf, "%d %d %d %u %11.7f %11.7f\n", pnn->cInput, pnn->cHidden,
	       pnn->cOutput, pnn->nTrained, pnn->rBetaHidden,
	       pnn->rBetaOutput ) < 0 ) {
    return -1;
  }

  for( i = pnn->cInput * pnn->cHidden, pr = pnn->arHiddenWeight; i; i-- )
    if( fprintf( pf, "%.7f\n", *pr++ ) < 0 )
      return -1;

  for( i = pnn->cHidden * pnn->cOutput, pr = pnn->arOutputWeight; i; i-- )
    if( fprintf( pf, "%.7f\n", *pr++ ) < 0 )
      return -1;

  for( i = pnn->cHidden, pr = pnn->arHiddenThreshold; i; i-- )
    if( fprintf( pf, "%.7f\n", *pr++ ) < 0 )
      return -1;

  for( i = pnn->cOutput, pr = pnn->arOutputThreshold; i; i-- )
    if( fprintf( pf, "%.7f\n", *pr++ ) < 0 )
      return -1;

  return 0;
}







#if defined( nono )
extern int NeuralNetCreate( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput, float rBetaHidden,
			    float rBetaOutput ) {
    int i;
    float *pf;
    
    pnn->cInput = cInput;
    pnn->cHidden = cHidden;
    pnn->cOutput = cOutput;
    pnn->rBetaHidden = rBetaHidden;
    pnn->rBetaOutput = rBetaOutput;
    pnn->nTrained = 0;

    if( !( pnn->arHiddenWeight = malloc( cHidden * cInput *
					 sizeof( float ) ) ) )
	return -1;

/*      if( !( pnn->arHiddenWeightt = malloc( cHidden * cInput * */
/*  					 sizeof( float ) ) ) ) */
/*  	return -1; */
    
/*      if( !( pnn->arHiddenWeightl = */
/*  	   malloc( cHidden * cInput * sizeof(*pnn->arHiddenWeightl ) ) ) ) */
/*  	return -1; */
    
    if( !( pnn->arOutputWeight = malloc( cOutput * cHidden *
					 sizeof( float ) ) ) ) {
	free( pnn->arHiddenWeight );
	return -1;
    }

    
/*      if( !( pnn->arOutputWeightl = */
/*  	   malloc( cOutput * cHidden * sizeof( *pnn->arOutputWeightl ) ) ) ) { */
/*        return -1; */
/*      } */
    
    if( !( pnn->arHiddenThreshold = malloc( cHidden * sizeof( float ) ) ) ) {
	free( pnn->arOutputWeight );
	free( pnn->arHiddenWeight );
	return -1;
    }

/*      if( !( pnn->arHiddenThresholdl = */
/*  	   malloc( cHidden * sizeof(*pnn->arHiddenThresholdl  ) ) ) ) { */
/*        return -1; */
/*      } */
    
    if( !( pnn->arOutputThreshold = malloc( cOutput * sizeof( float ) ) ) ) {
	free( pnn->arHiddenThreshold );
	free( pnn->arOutputWeight );
	free( pnn->arHiddenWeight );
	return -1;
    }

/*      if( !( pnn->arOutputThresholdl = */
/*  	   malloc( cOutput * sizeof( *pnn->arOutputThresholdl ) ) ) ) { */
/*        return -1; */
/*      } */
    
    for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;

    return 0;
}
#endif

#if defined( nono )
float maxm = -1.0;

inline float
sigmoid(float const xin)
{

/*      return ( x < 0 ) ? 1-sigmoid(-x): */
/*  	1/(2+x*(1+x*1/2*(1+x*1/3*(1+x*1/4*(1+x*1/5))))); */

  float const x = xin < 0 ? -xin : xin;
  
  //float const v = 1.0/(2 + x + x*x * (1.0/2));
  float const v = 1.0/(1.0 + exp(x));

  if( v > maxm ) {
    maxm = v;
  }
  
  return xin < 0 ? 1.0 - v : v;

/*      return ( x < 0 ) ? 1-sigmoid(-x): */
/*  	1.0/(2 + x + x*x * (1.0/2 + 1.0/3 + 1.0/4 + 1.0/5)); */
}
/*extern float sigmoid( float r ); */
#endif

#if defined( nono )
static int Evaluateo( neuralnet *pnn, float arInput[], float ar[],
		     float arOutput[] )
{
  int i, j;
  float r, *pr, *prWeight;

  /* Calculate activity at hidden nodes */
  for( i = 0; i < pnn->cHidden; i++ )
    ar[ i ] = pnn->arHiddenThreshold[ i ];

  for( i = 0; i < pnn->cInput; i++ ) {
    float const ari = arInput[ i ];

    if( ari != 0 ) {
      prWeight = pnn->arHiddenWeight + i * pnn->cHidden;
      
      if( ari == 1.0 ) {
	for(j = pnn->cHidden, pr = ar; j; j-- ) {
	  *pr++ += *prWeight++;
	}
      }
      else {
	for(j = pnn->cHidden, pr = ar; j; j-- ) {
	  *pr++ += *prWeight++ * ari;
	}
      }
    }
  }
  
  for( i = 0; i < pnn->cHidden; i++ )
    ar[ i ] = sigmoid( -pnn->rBetaHidden * ar[ i ] );

  /* Calculate activity at output nodes */
  prWeight = pnn->arOutputWeight;

  for( i = 0; i < pnn->cOutput; i++ ) {
    r = pnn->arOutputThreshold[ i ];
    
    for( j = 0; j < pnn->cHidden; j++ )
      r += ar[ j ] * *prWeight++;

    arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
  }

  return 0;
}
#endif


#if defined( nono )
static int Evaluaten( neuralnet *pnn, float arInput[], float ar[],
		     float arOutput[] )
{
  int i,j;
  const float *weight;
  const float *iweight;
  unsigned int n1 = 0, n2 = 0;
  int skip1[pnn->cInput], skip2[pnn->cInput];
  int s1 = 0, s2 = 0;
  
  for( i = 0; i < pnn->cInput; i++ ) {
    float const ari = arInput[i];
    
    if( ari == 0.0 ) {
      ++s1;
      ++s2;
    } else if( ari == 1.0 ) {
      ++s1;
      skip2[n2++] = s2;
      s2 = 1;
    } else {
       ++s2;
      skip1[n1++] = s1;
      s1 = 1;
    }
  }

  iweight = pnn->arHiddenWeightt;
  
  for(j = 0; j < pnn->cHidden; ++j) {
    float z = pnn->arHiddenThreshold[j];

    const float* arp = arInput;
    //    weight = pnn->arHiddenWeight + j;

    weight = iweight;

    {
      int* s1 = skip1;
      for(i = 0; i < n1; ++i) {
	arp += *s1;
	weight += *s1++;
      
	z += *arp * *weight;
      }
    }

    //weight = pnn->arHiddenWeight + j;
    weight = iweight;

    {
      int* s2 = skip1;

      for(i = 0; i < n2; ++i) {
	weight += *(s2)++;
      
	z += *weight;
      }
    }

    iweight += pnn->cInput;
    
    ar[j] = sigmoid(-pnn->rBetaHidden * z);
  }

  /* Calculate activity at output nodes */
  weight = pnn->arOutputWeight;

  for( i = 0; i < pnn->cOutput; i++ ) {
    float r = pnn->arOutputThreshold[ i ];
    
    for( j = 0; j < pnn->cHidden; j++ )
      r += ar[ j ] * *weight++;

    arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
  }

  return 0;
}
#endif


#if defined( nono )
#define  factor ((long)(0x1L << 24))

static int
Evaluate1(neuralnet* pnn, float arInput[], float arOutput[])
{
  int i, j;
  long*  prWeight;
  long long* pr;
  
  long long arl[pnn->cHidden];
  
  //float ar[ pnn->cHidden ];
  
  /* Calculate activity at hidden nodes */
  for( i = 0; i < pnn->cHidden; i++ ) {
    arl[ i ] = (long long)(pnn->arHiddenThresholdl[i]) << 24;
  }

#if defined( print )
  for( i = 0; i < pnn->cHidden; i++ ) {
    printf("%d %lf\n", i, (double)arl[i] / ((double)factor * factor));
  }
#endif
  
  for( i = 0; i < pnn->cInput; i++ ) {
    long ari = (long)(factor * arInput[i]  + 0.5);

#if defined( print )
    printf("%d %ld %lf", i, ari, (double)ari / factor);
#endif
    
    if( ari != 0 ) {
      prWeight = pnn->arHiddenWeightl + i * pnn->cHidden;
      pr = arl;

      if( ari == factor ) {
	for(j = pnn->cHidden; j; j--) {
	  long prw = *(prWeight++);
      
#if defined( print )
	  if( j == pnn->cHidden - 1) {
	    printf("\n   %d %lf %lf\n", j,
		   (double)pr[0] / ((double)factor * factor),
		   (double)((long long)prw << 24) / ((double)factor * factor));
	  }
#endif
	  
	  *pr++ += (long long)prw << 24;
	  
#if defined( print )
	  if( j == pnn->cHidden - 1) {
	    printf("\n   %d %lf %lf\n", j, (double)pr[-1],
		   (double)pr[-1] / ((double)factor * factor));
	  }
#endif 
	}
      } else {
	for(j = pnn->cHidden; j; j-- ) {
	  long prw = *(prWeight++);
	  
	  *pr++ += (long long)ari * prw;
	  
#if defined( print )
	  if( j == pnn->cHidden - 1 ) {
	    printf("\n   %d %lf %lf\n", j,
		   (double)pr[-1], (double)pr[-1] / ((double)factor * factor));
	  }
#endif 
	}
      }
    } else {
#if defined( print )
      printf("\n");
#endif
    }
  }
  
#if defined( print )
  for( i = 0; i < pnn->cHidden; i++ ) {
    printf("%d %lf\n", i, (double)arl[i] / ((double)factor * factor) );
  }
#endif
  
  for( i = 0; i < pnn->cHidden; i++ ) {
    double x = (double)arl[i] / ((double)factor * factor);
    float y = sigmoid( -pnn->rBetaHidden * x );
    
#if defined( print )
    printf("%d %lf %lf\n", i, x, y);
#endif
    
    arl[i] = (long)(factor * y  + 0.5);

#if defined( print )
    printf("%d %lf\n", i, (double)arl[i] / (double)factor );
#endif
  }

  /* Calculate activity at output nodes */
    
  prWeight = pnn->arOutputWeightl;

  for( i = 0; i < pnn->cOutput; i++ ) {
    long long r = (long long)pnn->arOutputThresholdl[i] << 24;    
	
    for( j = 0; j < pnn->cHidden; j++ ) {
      long prw = *prWeight++;
      
      r += arl[j] * prw;
    }

    {
      double x = (double)r / ((double)factor * factor);
      arOutput[ i ] = sigmoid( -pnn->rBetaOutput * x );
    }
  }

  return 0;
}

#endif
