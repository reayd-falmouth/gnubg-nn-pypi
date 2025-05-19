/*
 * neuralnet.h
 *
 * by Gary Wong, 1998
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

#ifndef _NEURALNET_H_
#define _NEURALNET_H_

#if defined(__cplusplus) || defined(__GNUC__)
#define CONST const
#else
#define CONST
#endif

typedef float Intermediate;

typedef struct _neuralnet {
  int cInput, cHidden, cOutput;
  unsigned int nTrained;
  
  float rBetaHidden, rBetaOutput;
  float *arHiddenWeight, *arOutputWeight,
	*arHiddenThreshold, *arOutputThreshold;

  Intermediate* savedBase;
  float* 	savedIBase;

    
  unsigned long		nEvals;

/*    float* arHiddenWeightt; */
  
/*    long* arHiddenThresholdl; */
/*    long* arHiddenWeightl; */
/*    long* arOutputThresholdl; */
/*    long*	arOutputWeightl; */
  
} neuralnet;

extern void NeuralNetInit( neuralnet *pnn );

extern int NeuralNetCreate( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput, float rBetaHidden,
			    float rBetaOutput );
extern int NeuralNetDestroy( neuralnet *pnn );

typedef enum  {
  NNEVAL_NONE,
  NNEVAL_SAVE,
  NNEVAL_FROMBASE,
} NNEvalType;

extern int NeuralNetEvaluate( neuralnet *pnn, float arInput[],
			      float arOutput[] /*, NNEvalType t*/);
extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha );
extern int
NeuralNetTrainS(neuralnet* pnn, float arInput[], float arOutput[],
		float arDesired[], float rAlpha, CONST int* tList);

extern int NeuralNetResize( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput );

#include <stdio.h>

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf );
extern int NeuralNetSave( neuralnet *pnn, FILE *pf );

#endif
