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
#ifndef FLEXUS_PROFILE_HPP_INCLUDED
#define FLEXUS_PROFILE_HPP_INCLUDED

#include <string>
#include <vector>

#include <boost/preprocessor/cat.hpp>

namespace nProfile {

#ifndef X86_64
inline int64_t rdtsc() {
  int64_t tsc;
  __asm__ __volatile__ ( "rdtsc" : "=A" (tsc));
  return tsc;
}
#else
inline int64_t rdtsc() {
  int64_t tsca, tscd;
  __asm__ __volatile__ ( "rdtsc" : "=A" (tsca), "=D" (tscd));
  return (tscd << 32) | (tsca & 0xffffffff);
}
#endif

/*
inline int32_t prid() {
  int64_t id = 0;
  __asm__ __volatile__ ( "cpuid" : "=b" (id) : "a" (1) ) ;
  return id >> 31;
}
*/

class Timer;
class ManualTimer;
class Profiler;

class ProfileManager {
  std::vector< Profiler *> theProfilers;
  int64_t theStartTime;
public:
  ProfileManager() {
    theStartTime = rdtsc();
  }
  void addProfiler( Profiler * aProfiler) {
    theProfilers.push_back(aProfiler);
  }
  inline int64_t programTime() {
    return ( rdtsc() - theStartTime) / 1000;
  }
  void report(std::ostream &);
  void reset();
  static ProfileManager * profileManager();
};

extern Profiler * theProfileTOS;

class Profiler {
  std::string theFn;
  std::string theFile;
  int64_t theLine;
  int64_t theTimeIn;
  int64_t theTimeAccum;
  int64_t theTimeAccumChildren;
  friend class Timer;
  friend class ManualTimer;
public:
  std::string const & name() const {
    return theFn;
  }
  std::string const & file() const {
    return theFile;
  }
  int64_t line() const {
    return theLine;
  }
  int64_t totalTime() const {
    return theTimeAccum / 1000;
  }
  int64_t selfTime() const {
    return (theTimeAccum - theTimeAccumChildren) / 1000;
  }

  Profiler( std::string const & aFn, std::string const & aFile, int64_t aLine )
    : theFn(aFn)
    , theFile(aFile)
    , theLine(aLine)
    , theTimeIn(0)
    , theTimeAccum(0)
    , theTimeAccumChildren(0) {
    ProfileManager::profileManager()->addProfiler(this);
  }

  void reset() {
    theTimeIn = 0;
    theTimeAccum = 0;
    theTimeAccumChildren = 0;
  }
};

class Timer {
  Profiler & theProfiler;
  Profiler * theParent;
public:
  inline Timer(Profiler & aProfiler)
    : theProfiler(aProfiler) {
    theParent = theProfileTOS;
    theProfileTOS = &theProfiler;
    theProfiler.theTimeIn = rdtsc();
  }
  inline ~Timer() {
    if (theProfiler.theTimeIn != 0) {
      int64_t delta = rdtsc() - theProfiler.theTimeIn;
      if (delta > 0) {
        theProfiler.theTimeAccum += delta;
        if (theParent) {
          theParent->theTimeAccumChildren += delta;
        }
      }
    }
    theProfileTOS = theParent;
    theProfiler.theTimeIn = 0;
  }
};

class ManualTimer {
  Profiler & theProfiler;
public:
  inline ManualTimer(Profiler & aProfiler)
    : theProfiler(aProfiler)
  { }

  inline void start() {
    theProfiler.theTimeIn = rdtsc();
  }

  inline void stop() {
    if (theProfiler.theTimeIn != 0) {
      int64_t delta = rdtsc() - theProfiler.theTimeIn;
      if (delta > 0) {
        theProfiler.theTimeAccum += delta;
      }
    }
    theProfiler.theTimeIn = 0;
  }
};

#undef PROFILING_ENABLED
#ifdef PROFILING_ENABLED

#define FLEXUS_PROFILE()                                                                 \
  static nProfile::Profiler BOOST_PP_CAT( profiler,__LINE__) ( __FUNCTION__, __FILE__, __LINE__ ) ;  \
  nProfile::Timer BOOST_PP_CAT( timer,__LINE__)( BOOST_PP_CAT( profiler,__LINE__) )                           /**/

#define FLEXUS_PROFILE_N(name)                                                           \
  static nProfile::Profiler BOOST_PP_CAT( profiler,__LINE__) ( name, __FILE__, __LINE__ ) ;          \
  nProfile::Timer BOOST_PP_CAT( timer,__LINE__)( BOOST_PP_CAT( profiler,__LINE__) )                           /**/

#else

#define FLEXUS_PROFILE() while (false)
#define FLEXUS_PROFILE_N(name) while (false)

#endif

} //End nProfile

#endif //FLEXUS_PROFILE_HPP_INCLUDED
