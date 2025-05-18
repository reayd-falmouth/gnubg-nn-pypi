/*
 * dice_gen.cc
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

#include <assert.h>
#include <algorithm>

#include "bgdefs.h"

#include "dice_gen.h"

using namespace std;

namespace {
uint const chunk = 40;

void
semiRoll(int dice[2], int& semiRandCounter)
{
  uint const x = semiRandCounter % 36;
  uint const d1 = 1 + (x / 6);
  uint const d2 =  1 + (x % 6);
  dice[0] = std::max(d1, d2);
  dice[1] = std::min(d1, d2);

  --semiRandCounter;
}
}

GetDice::OneSeq::OneSeq(void) :
  seed(genrand()),
  len(0),
  allocated(0),
  numbers(0),
  cur(0),
  synchronized(true),
  semi(false)
{}

GetDice::OneSeq::~OneSeq()
{
  delete [] numbers;
}

void
GetDice::OneSeq::roll(int dice[2], int& semiRandCounter)
{
  if( cur == 0 && semiRandCounter > 0 ) {
    semiRoll(dice, semiRandCounter);

    semi = true;
  } else {
    RollDice(dice);
  }
}
    

void
GetDice::OneSeq::add(int dice[2])
{
  if( cur + 2 > allocated ) {
    int* n = new int [allocated + chunk];
    if( allocated ) {
      memcpy(n, numbers, allocated * sizeof(*n));
    }
    delete [] numbers;
    numbers = n;
    allocated += chunk;
  }

  numbers[cur] = dice[0];
  numbers[cur+1] = dice[1];
  cur += 2;
  len = cur;
}

void
GetDice::OneSeq::get(int dice[2])
{
  if( cur + 2 <= len ) {
    dice[0] = numbers[cur];
    dice[1] = numbers[cur+1];
    cur += 2;
  } else {
    if( ! synchronized ) {
      sgenrand(seed);
      for(uint k = 0; k < len - (semi ? 2 : 0); ++k) {
	genrand();
      }
      synchronized = true;
    }
    
    RollDice(dice);
    add(dice);
  }
}
  
void
GetDice::OneSeq::start(void)
{
  cur = 0;

  if( len == 0 ) {
    sgenrand(seed);
    synchronized = true;
  } else {
    synchronized = false;
  }
}

bool
GetDice::OneSeq::empty(void) const
{
  assert( (len == 0) == (allocated == 0) && (len == 0) == (numbers == 0) );
  
  return len == 0;
}

GetDice::GetDice(bool const semiRand_) :
  semiRand(semiRand_),
  nSeq(0),
  seqs(0),
  mode(NONE)
{}

GetDice::~GetDice()
{
  delete [] seqs;
}

void
GetDice::startSave(uint const nSeq_)
{
  delete [] seqs;
  nSeq = nSeq_;
  seqs = new OneSeq [nSeq];

  cur = 0;

  mode = SAVING;

  semiRandCounter = semiRand ? (nSeq - (nSeq % 36)) : 0;
}

void
GetDice::startRetrive(void)
{
  mode = RETRIVING;
  cur = 0;
  
  // HACK
  // If generator was not called at all in first round, keep it in saving
  // mode in the next round, instead of handling the login in retrive.
  
  if( seqs[cur].empty() ) {
#if !defined(NDEBUG)
    for(uint k = 0; k < nSeq; ++k) {
      assert( seqs[k].empty() );
    }
#endif
    mode = SAVING;
    return;
  }
  
  seqs[cur].start();
}

void
GetDice::next(void)
{
  if( mode != NONE ) {
    ++cur;
    {                                                   assert( cur < nSeq ); }
    seqs[cur].start();
  } else {
    cur = 0;
  }
}

void
GetDice::endSave(uint const nSeq)
{
  semiRandCounter = (semiRand && nSeq > 0) ? (nSeq - (nSeq % 36)) : 0;
    
  mode = NONE;
}


void
GetDice::get(int dice[2])
{
  switch( mode ) {
    case SAVING:
    {
      seqs[cur].roll(dice, semiRandCounter);
      seqs[cur].add(dice);
      break;
    }
    case RETRIVING:
    {
      seqs[cur].get(dice);
      break;
    }
    case NONE:
    {
      if( cur == 0 && semiRandCounter > 0 ) {
	semiRoll(dice, semiRandCounter);
	cur = 1;
      } else {
	RollDice(dice);
      }
      break;
    }
  }

  if( dice[0] < dice[1] ) {
    int const d = dice[0];
    dice[0] = dice[1];
    dice[1] = d;
  }
}
