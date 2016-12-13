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
#include "profile.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#include <boost/lexical_cast.hpp>

// #ifndef __STDC_CONSTANT_MACROS
// #define __STDC_CONSTANT_MACROS
// #endif
// #include <boost/date_time/posix_time/posix_time.hpp>

namespace nProfile {

using namespace std::chrono;

ProfileManager * theProfileManager = 0;
Profiler * theProfileTOS = 0;

ProfileManager * ProfileManager::profileManager() {
  if (theProfileManager == 0) {
    theProfileManager = new ProfileManager();
  }
  return theProfileManager;
}

bool sortTotalTime( Profiler * left, Profiler * right) {
  return left->totalTime() > right->totalTime();
}

bool sortSelfTime( Profiler * left, Profiler * right) {
  return left->selfTime() > right->selfTime();
}

std::string rightmost( std::string const & aString, uint32_t N) {
  std::string retval(aString);
  if (retval.length() > N) {
    retval.erase(0, retval.length() - N);
  } else if (retval.length() < N) {
    retval.append(N - retval.length(), ' ');
  }
  return retval;
}

std::string leftmost( std::string const & aString, uint32_t N) {
  std::string retval(aString.substr(0, N));
  if (retval.length() < N) {
    retval.append(N - retval.length(), ' ');
  }
  return retval;
}

system_clock::time_point last_reset(system_clock::now());

void ProfileManager::reset() {
  std::vector< Profiler *>::iterator iter, end;
  for (iter = theProfilers.begin(), end = theProfilers.end(); iter != end; ++iter) {
    (*iter)->reset();
  }

  last_reset = system_clock::now();
  theStartTime = rdtsc();
}

void ProfileManager::report(std::ostream & out) {
  std::vector< Profiler *>::iterator iter, end;
  int32_t i = 0;

  float program_time = programTime() / 100 ;

  out << "Ticks Since Reset: " << program_time << std::endl << std::endl;
  system_clock::time_point now(system_clock::now());
  out << "Wall Clock Since Reset: " << duration_cast<microseconds>(now - last_reset).count() << "us" << std::endl << std::endl;

  out << "Worst sources, by Self Time \n";
  std::sort( theProfilers.begin(), theProfilers.end(), &sortSelfTime);

  out << std::setiosflags( std::ios::fixed) << std::setprecision(2);
  out << "File                          " << " ";
  out << "Function/Name                 " << " ";
  out << "Self Time  " << "   ";
  out << "% ";
  out << "\n";
  for (iter = theProfilers.begin(), end = theProfilers.end(), i = 0; iter != end && i < 200; ++iter, ++i) {
    std::string file_line = (*iter)->file() + ":" + std::to_string((*iter)->line());
    out << rightmost(file_line, 30) << " ";
    out << leftmost((*iter)->name(), 30) << " ";
    out << std::setiosflags(std::ios::right) << std::setw(11) << (*iter)->selfTime() << "   ";
    out << std::setw(5) << static_cast<float>((*iter)->selfTime()) / program_time << "% ";
    out << std::endl;
  }
  out << "\n";

  out << "Worst sources, sorted by Total Time \n";
  std::sort( theProfilers.begin(), theProfilers.end(), &sortTotalTime);

  out << std::setiosflags( std::ios::fixed) << std::setprecision(2);
  out << "File                          " << " ";
  out << "Function/Name                 " << " ";
  out << "Total Time " << "   ";
  out << "% ";
  out << "\n";
  for (iter = theProfilers.begin(), end = theProfilers.end(), i = 0; iter != end && i < 200; ++iter, ++i) {
    std::string file_line = (*iter)->file() + ":" + std::to_string((*iter)->line());
    out << rightmost(file_line, 30) << " ";
    out << leftmost((*iter)->name(), 30) << " ";
    out << std::setiosflags(std::ios::right) << std::setw(11) << (*iter)->totalTime() << "   ";
    out << std::setw(5) << static_cast<float>((*iter)->totalTime()) / program_time << "% ";
    out << std::endl;
  }
  out << "\n";

}

} //nProfile
