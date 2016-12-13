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
#include <iostream>
#include <sstream>
#include <fstream>
#include <deque>
#include <cstdlib>

#include <functional>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <core/stats.hpp>

namespace Flexus {
namespace Core {
void Break() {}
} }

using namespace Flexus::Stat;

std::deque< std::function< void () > > theCommands;

void usage() {
  std::cout << "Usage: stat-sample <output stat file> <input stat files>* " << std::endl;
}

void help(std::string const & command) {
  usage();
}

void loadDatabaseFile( std::istream & anIstream, std::string const & aPrefix, bool aFirst) {
  if (aFirst) {
    getStatManager()->load(anIstream);
  } else {
    getStatManager()->loadMore(anIstream, aPrefix);
  }
}

void loadDatabase( std::string const & aName, std::string const & aPrefix, bool aFirst) {
  size_t loc = aName.rfind(".gz");

  try {
    std::ifstream in(aName.c_str(), std::ios::binary);
    if (in) {
      if (loc == std::string::npos) {
        loadDatabaseFile(in, aPrefix, aFirst);
        in.close();
      } else {
        boost::iostreams::filtering_istream inGzip;
        inGzip.push(boost::iostreams::gzip_decompressor());
        inGzip.push(in);
        loadDatabaseFile(inGzip, aPrefix, aFirst);
        inGzip.reset();
      }
    } else {
      std::cout << "Cannot open stats database " << aName << std::endl;
      std::exit(-1);
    }
  } catch (...) {
    std::cout << "Unable to load stats from database " << aName << std::endl;
    std::exit(-1);
  }
}

void reduceSum() {
  getStatManager()->reduce(eReduction::eSum, ".*selection", "sum", std::cout);
}

void reduceAvg() {
  getStatManager()->reduce(eReduction::eAverage, ".*selection", "avg", std::cout);
}

void reduceStdev(std::string const & aMsmt) {
  getStatManager()->reduce(eReduction::eStdDev, ".*selection", aMsmt, std::cout);
}

void reduceCount() {
  getStatManager()->reduce(eReduction::eCount, ".*selection", "count", std::cout);
}

void reduceNodes() {
  getStatManager()->reduceNodes(".*selection");
}

void save(std::string const & aFilename, std::string const & aMeasurementRestriction) {
  getStatManager()->saveMeasurements(aMeasurementRestriction, aFilename);
}

std::string region_string;

void processCmdLine(int32_t aCount, char ** anArgList) {

  if (aCount < 2) {
    usage();
    std::exit(-1);
  }

  std::string output_file = anArgList[1];
  std::string first_file = anArgList[2];

  theCommands.push_back( [&first_file](){ return loadDatabase(first_file, "", true); }); //ll::bind( &loadDatabase, first_file, std::string(""), true ) );
  for (int32_t i = 3; i < aCount; ++i) {
    std::stringstream prefix;
    prefix << std::setw(2) << std::setfill('0') << (i - 1) << '-';
    theCommands.push_back( [i, &anArgList, &prefix](){ return loadDatabase(std::string(anArgList[i]), prefix.str(), false); }); //ll::bind( &loadDatabase, std::string(anArgList[i]), prefix.str(), false) );
  }
  theCommands.push_back( [](){ return reduceSum(); }); //ll::bind( &reduceSum ) );
  theCommands.push_back( [](){ return reduceAvg(); }); //ll::bind( &reduceAvg ) );
  theCommands.push_back( [](){ return reduceStdev("pernode-stdev"); }); //ll::bind( &reduceStdev, "pernode-stdev"  ) );
  theCommands.push_back( [](){ return reduceCount(); }); // ll::bind( &reduceCount) );
  theCommands.push_back( [](){ return reduceNodes(); }); // ll::bind( &reduceNodes ) );
  theCommands.push_back( [](){ return reduceStdev("stdev"); }); // ll::bind( &reduceStdev, "stdev" ) );
  theCommands.push_back( [&output_file](){ return save(output_file, "(sum|count|avg|stdev|pernode-stdev)"); }); // ll::bind( &save, output_file, "(sum|count|avg|stdev|pernode-stdev)" ) );

}

int32_t main(int32_t argc, char ** argv) {

  getStatManager()->initialize();

  processCmdLine(argc, argv);

  while (! theCommands.empty() ) {
    theCommands.front()();
    theCommands.pop_front();
  }

}
