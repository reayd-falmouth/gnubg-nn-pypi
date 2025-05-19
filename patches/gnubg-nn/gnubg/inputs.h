// -*- C++ -*-

/*
 * inputs.h
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
 */
#if !defined( INPUTS_H )
#define INPUTS_H

/* Ensure CONST is defined to the C ‘const’ keyword if config.h didn’t */
#ifndef CONST
# define CONST const
#endif

typedef void
(*InputsFunc)(CONST int anBoard[2][25], float arInput[]);

typedef CONST char*
(*InputNameFunc)(unsigned int n);

typedef struct NetInputFuncs_ {
  CONST char*   	name;
  InputsFunc		func;
  unsigned int		nInputs;
  InputNameFunc		ifunc;
  void*			lib;
} NetInputFuncs;


extern NetInputFuncs inputFuncs[];

CONST NetInputFuncs*
ifByName(CONST char* name);

#define MAX_NUM_INPUTS 400

void
ComputeTable(void);

void 
CalculateBearoffInputs(CONST int anBoard[2][25], float inputs[]);

void 
baseInputs(CONST int anBoard[2][25], float inputs[]);

CONST char*
inputNameFromFunc(CONST NetInputFuncs* inp, unsigned int k);

CONST NetInputFuncs*
ifFromFile(CONST char* name);

unsigned int
maxIinputs(void);

void
getInputs(CONST int anBoard[2][25], CONST int* which, float* values);

int
closeInputs(CONST NetInputFuncs* f);

CONST char*
genericInputName(unsigned int n);

#endif
