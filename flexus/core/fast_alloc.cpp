// DO-NOT-REMOVE begin-copyright-block 
//
// Redistributions of any form whatsoever must retain and/or include the
// following acknowledgment, notices and disclaimer:
//
// This product includes software developed by Carnegie Mellon University.
//
// Copyright 2012 by Mohammad Alisafaee, Eric Chung, Michael Ferdman, Brian 
// Gold, Jangwoo Kim, Pejman Lotfi-Kamran, Onur Kocberber, Djordje Jevdjic, 
// Jared Smolens, Stephen Somogyi, Evangelos Vlachos, Stavros Volos, Jason 
// Zebchuk, Babak Falsafi, Nikos Hardavellas and Tom Wenisch for the SimFlex 
// Project, Computer Architecture Lab at Carnegie Mellon, Carnegie Mellon University.
//
// For more information, see the SimFlex project website at:
//   http://www.ece.cmu.edu/~simflex
//
// You may not use the name "Carnegie Mellon University" or derivations
// thereof to endorse or promote products derived from this software.
//
// If you modify the software you must place a notice on or within any
// modified version provided or made available to any third party stating
// that you have modified the software.  The notice shall include at least
// your name, address, phone number, email address and the date and purpose
// of the modification.
//
// THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER
// EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY
// THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
// TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
// BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT,
// SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN
// ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY,
// CONTRACT, TORT OR OTHERWISE).
//
// DO-NOT-REMOVE end-copyright-block   
/*
 * Copyright (c) 1998 by Mark Hill, James Larus, and David Wood for the
 * Wisconsin Wind Tunnel Project.
 *
 * This source file is part of the WWT2 simulator, originally written by
 * Steven K. Reinhardt and subsequently maintained and enhanced by Babak
 * Falsafi, Michael J. Litzkow, Shubhendu S. Mukherjee, and
 * Steven K. Reinhardt.
 *
 * This software is furnished under a license and may be used and
 * copied only in accordance with the terms of such license and the
 * inclusion of the above copyright notice.  This software or any other
 * copies thereof or any derivative works may not be provided or
 * otherwise made available to any other persons.  Title to and ownership
 * of the software is retained by Mark Hill, James Larus, and David Wood.
 *
 * This software is provided "as is".  The licensor makes no
 * warrenties about its correctness or performance.
 *
 * Do not distribute.  Direct all inquiries to:
 *
 * Mark Hill
 * Computer Sciences Department
 * University of Wisconsin
 * 1210 West Dayton Street
 * Madison, WI 53706
 * markhill@cs.wisc.edu
 * 608-262-2196
 *
 */
#ifdef __GNUC__
#pragma implementation
#endif

#include <core/fast_alloc.hpp>

void * FastAlloc::freeLists[Num_Buckets];

#ifdef FAST_ALLOC_STATS
unsigned FastAlloc::newCount[Num_Buckets];
unsigned FastAlloc::deleteCount[Num_Buckets];
unsigned FastAlloc::allocCount[Num_Buckets];
#endif

void * FastAlloc::moreStructs(int32_t bucket) {
  assert(bucket > 0 && bucket < Num_Buckets);

  int32_t sz = bucket * Alloc_Quantum;
  const int32_t nstructs = Num_Structs_Per_New; // how many to allocate?
  char * p = (char *)::new char[nstructs * sz];

#ifdef FAST_ALLOC_STATS
  ++allocCount[bucket];
#endif

  freeLists[bucket] = p;
  for (int32_t i = 0; i < (nstructs - 2); ++i, p += sz)
    * (void **)p = p + sz;
  *(void **)p = 0;

  return (p + sz);
}

#ifdef FAST_ALLOC_DEBUG

FastAlloc * FastAlloc::inUseList;

FastAlloc::FastAlloc() {
  inUsePrev = nullptr;
  inUseNext = inUseList;
  if (inUseList) {
    inUseList->inUsePrev = this;
  }
  inUseList = this;
}

FastAlloc::~FastAlloc() {
  if (inUsePrev) {
    inUsePrev->inUseNext = inUseNext;
  } else {
    assert(inUseList == this);
    inUseList = inUseNext;
  }

  inUseNext->inUsePrev = inUsePrev;
}

#endif
