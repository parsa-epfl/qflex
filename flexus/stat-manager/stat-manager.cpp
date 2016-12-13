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
  std::cout << "Usage: stat-manager [general options] <command>" << std::endl;
  std::cout << "  General options:" << std::endl;
  std::cout << "    -d <stat file> - load stat database from <stat file> instead of stats_db.out" << std::endl;
  std::cout << "    -per-node - print per-nodes statistics (i.e. do no aggregate)" << std::endl;
  std::cout << "  Commands:" << std::endl;
  std::cout << "    help" << std::endl;
  std::cout << "    format" << std::endl;
  std::cout << "    format-string" << std::endl;
  std::cout << "    collapse-file" << std::endl;
  std::cout << "    collapse-string" << std::endl;
  std::cout << "    list-measurements" << std::endl;
  std::cout << "    list-stats" << std::endl;
  std::cout << "    print" << std::endl;
}

void help(std::string const & command) {
  if (command == "help") {
    std::cout << "Usage: stat-manager help <command>" << std::endl;
    std::cout << "  Displays details on how to use <command>." << std::endl;
  } else if (command == "format") {
    std::cout << "Usage: stat-manager format <template file> <measurements>" << std::endl;
    std::cout << "  <template file> - file name for the stats template file" << std::endl;
    std::cout << "  <measurements> - regular expression selecting which measurements should be used with the template" << std::endl;
    std::cout << std::endl;
    std::cout << "Template file format:" << std::endl;
    std::cout << "  Each line from the template file is written to the generated output, with field and operation" << std::endl;
    std::cout << "  specifiers replaced with corresponding values from the stats database.  Comment lines are" << std::endl;
    std::cout << "  ignored and do not appear in the generated output.  Directives control output generation," << std::endl;
    std::cout << "  causing the generator to iterate over measurements or fields, and controlling which" << std::endl;
    std::cout << "  measurement is used if none is explicitly specified in a field." << std::endl;
    std::cout << std::endl;
    std::cout << "  Comments" << std::endl;
    std::cout << "    any line beginning with //" << std::endl;
    std::cout << std::endl;
    std::cout << "  Directives (all start with #)" << std::endl;
    std::cout << "    #FOR-MEASUREMENTS <measurement spec>" << std::endl;
    std::cout << "      ..." << std::endl;
    std::cout << "    #END" << std::endl;
    std::cout << "      The lines between for and end are generated for each measurement which matches the" << std::endl;
    std::cout << "      <measurement spec> regexp (as constrained by the list of measurements on the command line)." << std::endl;
    std::cout << std::endl;
    std::cout << "  Fields " << std::endl;
    std::cout << "    General format:  {stat-name[:options]} " << std::endl;
    std::cout << "    Note the braces and that whitespace is significant, so don't include extra spaces." << std::endl;
    std::cout << "      <stat-name> - the name of the stat in the database" << std::endl;
    std::cout << "      <options> - options to use when printing the stat." << std::endl;
    std::cout << "         :name - print the name of the stat instead of its value" << std::endl;
    std::cout << std::endl;
    std::cout << "  Operations" << std::endl;
    std::cout << "    General format:  <operation[:<options>]} " << std::endl;
    std::cout << "    Note the angle brackets and that whitespace is significant, so don't include extra spaces." << std::endl;
    std::cout << "      SUM:field-spec - returns the sum of all fields matching the field-spec regular expression" << std::endl;
    std::cout << "      CSV:(field-spec):field_options" << std::endl;
    std::cout << "         - returns a comma separated list of field-spec regular expression" << std::endl;
    std::cout << "           field_options are passed to each field when printed." << std::endl;
    std::cout << "      MSMT - prints the measurement name" << std::endl;
    std::cout << std::endl;
    std::cout << "Take a look at some of the sample templates checked into CVS for ideas." << std::endl;
  } else if (command == "format-string") {
    std::cout << "Usage: stat-manager format-string <template string> <measurements>" << std::endl;
    std::cout << "  <template string> - template for formatting.  Use quotes if it contains spaces or special characters." << std::endl;
    std::cout << "  [measurements] - regular expression selecting which measurements should be used with the template" << std::endl;
    std::cout << "  See help for \"format\" for more info on the template format." << std::endl;
  } else if (command == "collapse-file") {
    std::cout << "Usage: stat-manager collapse-file <template file> <measurements>" << std::endl;
    std::cout << "  <template file> - file name for the stats template file" << std::endl;
    std::cout << "  <measurements> - regular expression selecting which measurements should be collapsed into one" << std::endl;
    std::cout << "                   (with the name \"Collapsed\") before the template is applied" << std::endl;
    std::cout << "  See help for \"format\" for more info on the template format." << std::endl;
  } else if (command == "collapse-string") {
    std::cout << "Usage: stat-manager collapse-string <template string> <measurements>" << std::endl;
    std::cout << "  <template string> - template for formatting.  Use quotes if it contains spaces or special characters." << std::endl;
    std::cout << "  <measurements> - regular expression selecting which measurements should be collapsed into one" << std::endl;
    std::cout << "                   (with the name \"Collapsed\") before the template string is applied" << std::endl;
    std::cout << "  See help for \"format\" for more info on the template format." << std::endl;
  } else if (command == "list-measurements") {
    std::cout << "Usage: stat-manager list-measurements" << std::endl;
    std::cout << "  Lists all the measurements in the database." << std::endl;
  } else if (command == "list-stats") {
    std::cout << "Usage: stat-manager list-stats" << std::endl;
    std::cout << "  Lists all the stats in the database." << std::endl;
  } else if (command == "print") {
    std::cout << "Usage: stat-manager print <measurement>" << std::endl;
    std::cout << "  Print every stat and value in the specified measurement (if multiple measurements" << std::endl;
    std::cout << "  are specified, first collapse them together)." << std::endl;
  } else {
    usage();
  }
}

void loadDatabase( std::string const & aName, bool specified) {
  std::string name;
  std::string nameGzip;
  size_t loc = aName.rfind(".gz");
  if (loc == std::string::npos) {
    name = aName;
    nameGzip = aName + ".gz";
  } else {
    name = aName.substr(0, loc);
    nameGzip = aName;
  }

  if (specified) {
    try {
      std::ifstream in(aName.c_str(), std::ios::binary);
      if (in) {
        if (loc == std::string::npos) {
          getStatManager()->load(in);
          in.close();
        } else {
          boost::iostreams::filtering_istream inGzip;
          inGzip.push(boost::iostreams::gzip_decompressor());
          inGzip.push(in);
          getStatManager()->load(inGzip);
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
    return;
  }

  try {
    std::ifstream in(name.c_str(), std::ios::binary);
    if (in) {
      getStatManager()->load(in);
      in.close();
    } else {
      in.clear();
      in.open(nameGzip.c_str(), std::ios::binary);
      if (!in) {
        std::cout << "Cannot open stats database " << name << " or " << nameGzip << std::endl;
        std::exit(-1);
      }
      boost::iostreams::filtering_istream inGzip;
      inGzip.push(boost::iostreams::gzip_decompressor());
      inGzip.push(in);
      getStatManager()->load(inGzip);
      inGzip.reset();
    }
  } catch (...) {
    std::cout << "Unable to load stats from database " << name << " or " << nameGzip << std::endl;
    std::exit(-1);
  }
}

void reduceNodes() {
  getStatManager()->reduceNodes(".*");
}

void listStats() {
  getStatManager()->listStats(std::cout);
}

void listMeasurements() {
  getStatManager()->listMeasurements(std::cout);
}

void printMeasurement(std::string const & aMeasurement) {
  getStatManager()->printMeasurement(aMeasurement, std::cout);
}

void format(std::string const & aFilename, std::string const & aMeasurementRestriction) {
  getStatManager()->formatFile(aMeasurementRestriction, aFilename, std::cout);
}

void collapse(std::string const & aFilename, std::string const & aMeasurementRestriction) {
  getStatManager()->collapseFile(aMeasurementRestriction, aFilename, std::cout);
}

void save(std::string const & aFilename, std::string const & aMeasurementRestriction) {
  getStatManager()->saveMeasurements(aMeasurementRestriction, aFilename);
}

void formatString(std::string const & aString, std::string const & aMeasurementRestriction) {
  std::string str = "#FOR-MEASUREMENTS\n";
  str += aString + "\n";
  str += "#END";
  getStatManager()->format(aMeasurementRestriction, aString, std::cout);
}

void formatStringSample(std::string const & aString) {
  getStatManager()->format(".*", aString, std::cout);
}

void collapseString(std::string const & aString, std::string const & aMeasurementRestriction) {
  std::string str = "#FOR-MEASUREMENTS\n";
  str += aString + "\n";
  str += "#END";
  getStatManager()->collapse(aMeasurementRestriction, str, std::cout);
}

void processCmdLine(int32_t aCount, char ** anArgList) {
  bool database_loaded = false;
  bool per_node = false;

  int32_t i = 1;
  for ( ; i < aCount; ++i ) {
    std::string arg(anArgList[i]);

    //Handle flags
    if (arg[0] == '-') {
      //-d <database>
      if (arg == "-d") {
        ++i;
        if (i < aCount) {
          std::string db_name = anArgList[i];
          theCommands.emplace_back( [db_name](){ return loadDatabase(db_name, true); }); //ll::bind( &loadDatabase, db_name, true) );
          database_loaded = true;
        } else {
          std::cout << "Must specify database with -d" << std::endl;
          std::exit(-1);
        }
      }

      //-per-node
      else if (arg == "-per-node") {
        per_node = true;
      }

    } else {
      break;
    }
  }

  if (i >= aCount) {
    theCommands.push_front( usage );
    return;
  }

  if (! per_node) {
    theCommands.emplace_back( [](){ return reduceNodes(); }); //ll::bind( &reduceNodes) );
  }

  //Handle command
  std::string command(anArgList[i]);

  if (command == "list-stats") {
    theCommands.emplace_back( listStats );
  } else if (command == "list-measurements") {
    theCommands.emplace_back( listMeasurements );
  } else if (command == "help") {
    if (i + 1 < aCount) {
      std::string command = anArgList[i+1];
      theCommands.push_front(  [command]{ help(command); });//ll::bind( &help, command ) );
    } else {
      theCommands.push_front( usage );
    }
    return;
  } else if (command == "print") {
    if (i + 1 < aCount) {
      std::string measurement = anArgList[i+1];
      theCommands.emplace_back( [measurement]{ printMeasurement(measurement); }); //ll::bind( &printMeasurement, measurement) );
    } else {
      std::cout << "Must specify a measurment to print." << std::endl;
      std::exit(-1);
    }
  } else if (command == "format-string") {
    std::string fmt;
    std::string measurements("all");
    if (i + 1 < aCount) {
      fmt = anArgList[i+1];
    } else {
      std::cout << "Must specify a template string to format." << std::endl;
      std::exit(-1);
    }
    if (i + 2 < aCount) {
      measurements = anArgList[i+2];
    }
    theCommands.emplace_back(  [fmt, measurements]{ formatString(fmt, measurements); });//ll::bind( &formatString, fmt, measurements ) );
  } else if (command == "sample-string") {
    std::string fmt;
    if (i + 1 < aCount) {
      fmt = anArgList[i+1];
    } else {
      std::cout << "Must specify a template string to format." << std::endl;
      std::exit(-1);
    }
    theCommands.emplace_back( [fmt](){ formatStringSample(fmt); }); //ll::bind( &formatStringSample, fmt) );
  } else if (command == "format") {
    std::string file;
    std::string measurements("all");
    if (i + 1 < aCount) {
      file = anArgList[i+1];
    } else {
      std::cout << "Must specify a tempate file to format." << std::endl;
      std::exit(-1);
    }
    if (i + 2 < aCount) {
      measurements = anArgList[i+2];
    }
    theCommands.emplace_back( [file, measurements]{ format(file, measurements); }); //ll::bind( &format, file, measurements ) );
  } else if (command == "collapse-string") {
    std::string fmt;
    std::string measurements("Region*");
    if (i + 1 < aCount) {
      fmt = anArgList[i+1];
    } else {
      std::cout << "Must specify a template string for collapse." << std::endl;
      std::exit(-1);
    }
    if (i + 2 < aCount) {
      measurements = anArgList[i+2];
    }
    theCommands.emplace_back( [fmt, measurements]{ collapseString(fmt, measurements); }); //ll::bind( &collapseString, fmt, measurements ) );
  } else if (command == "collapse-file") {
    std::string file;
    std::string measurements("Region*");
    if (i + 1 < aCount) {
      file = anArgList[i+1];
    } else {
      std::cout << "Must specify a tempate file for collapse." << std::endl;
      std::exit(-1);
    }
    if (i + 2 < aCount) {
      measurements = anArgList[i+2];
    }
    theCommands.emplace_back( [file, measurements]{ collapse(file, measurements); }); //ll::bind( &collapse, file, measurements ) );
  } else if (command == "save") {
    std::string file;
    std::string measurements("all");
    if (i + 1 < aCount) {
      file = anArgList[i+1];
    } else {
      std::cout << "Must specify a destination file for save." << std::endl;
      std::exit(-1);
    }
    if (i + 2 < aCount) {
      measurements = anArgList[i+2];
    }
    theCommands.emplace_back(  [file, measurements]{ save(file, measurements); });//ll::bind( &save, file, measurements ) );
  } else {
    std::cout << command << " is not a valid command." << std::endl;
    std::exit(-1);
  }

  if (! database_loaded ) {
    theCommands.push_front( []{ loadDatabase("stats_db.out", false); }); //ll::bind( &loadDatabase, "stats_db.out", false ) );
  }
}

int32_t main(int32_t argc, char ** argv) {

  getStatManager()->initialize();

  processCmdLine(argc, argv);

  while (! theCommands.empty() ) {
    theCommands.front()();
    theCommands.pop_front();
  }

}
