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
#include <algorithm>
#include <iomanip>
#include <queue>
#include <list>
#include <fstream>

#include <boost/throw_exception.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <functional>
#include <boost/regex.hpp>
#include <boost/optional.hpp>

#include <core/stats.hpp>

namespace Flexus {
namespace Stat {
namespace aux_ {

template<class Archive>
void save(Archive & ar, ::boost::intrusive_ptr<Flexus::Stat::aux_::Measurement> const & ptr, uint32_t version) {
  Measurement * msmt = ptr.get();
  ar & msmt;
}

template<class Archive>
void load(Archive & ar, ::boost::intrusive_ptr<Flexus::Stat::aux_::Measurement> & ptr, uint32_t version) {
  Measurement * msmt;
  ar & msmt;
  ptr = boost::intrusive_ptr<Flexus::Stat::aux_::Measurement> (msmt);
}

} //aux_
} //Stat
} //Flexus

namespace boost {
namespace serialization {
template<class Archive>
inline void serialize( Archive & ar, ::boost::intrusive_ptr<Flexus::Stat::aux_::Measurement> & t, const uint32_t file_version ) {
  split_free(ar, t, file_version);
}
}
}

namespace Flexus {
namespace Stat {

namespace aux_ {

void fold( StatValueBase & anAccumulator, StatValueBase const & anRHS, void ( StatValueBase::* aReduction)( StatValueBase const & ) ) {
  (anAccumulator .* aReduction) (anRHS);
}

bool Measurement::includeStat( Stat * aStat ) {
  return boost::regex_match(aStat->name(), theStatExpression);
}

void SimpleMeasurement :: addToMeasurement( Stat * aStat ) {
  //Check if the stat should be included in this measurement
  if (includeStat(aStat)) {
    theStats[ aStat->name() ] = aStat->createValue();
  }
}

void SimpleMeasurement :: close() {
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  while (iter != end) {
    iter->second.releaseUpdater();
    ++iter;
  }
}

void SimpleMeasurement :: print(std::ostream & anOstream, std::string const & options) {
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  anOstream << *this << std::endl;
  while (iter != end) {
    anOstream << "   " << iter->second << std::endl;
    ++iter;
  }
}

void SimpleMeasurement :: format(std::ostream & anOstream, std::string const & aField, std::string const & options) {
  stat_handle_map::iterator iter = theStats.find(aField);
  if (iter != theStats.end()) {
    iter->second.print(anOstream, options);
  } else {
    anOstream << "{ERR:No Such Stat}";
  }
}

void SimpleMeasurement :: doOp(std::ostream & anOstream, std::string const & anOp, std::string const & options) {
  if (anOp == "SUM") {
    doSUM( anOstream, options );
  } else if (anOp == "CSV") {
    doCSV( anOstream, options );
  } else if (anOp == "HISTSUM") {
    doHISTSUM( anOstream, options );
  } else if (anOp == "INSTSUM") {
    doINSTSUM( anOstream, options );
  } else if (anOp == "INST2HIST") {
    doINST2HIST( anOstream, options );
  } else if (anOp == "EXPR") {
    DBG_Assert( false );
  } else {
    Measurement::doOp(anOstream, anOp, options);
  }
}

int64_t SimpleMeasurement :: asLongLong(std::string const & aFieldSpec) {
  stat_handle_map::iterator iter = theStats.find(aFieldSpec);
  if (iter != theStats.end()) {
    return iter->second.asLongLong();
  } else {
    // look for options
    size_t loc = aFieldSpec.find(';');
    if (loc != std::string::npos) {
      std::string field( aFieldSpec.substr(0, loc) );
      std::string options( aFieldSpec.substr(loc + 1) );
      iter = theStats.find(field);
      if (iter != theStats.end()) {
        return iter->second.asLongLong(options);
      }
    }
    throw CalcException(std::string("{ERR:No Such Stat: ") + aFieldSpec + " }");
  }
}

double SimpleMeasurement :: asDouble(std::string const & aFieldSpec) {
  stat_handle_map::iterator iter = theStats.find(aFieldSpec);
  if (iter != theStats.end()) {
    return iter->second.asDouble();
  } else {
    // look for options
    size_t loc = aFieldSpec.find(';');
    if (loc != std::string::npos) {
      std::string field( aFieldSpec.substr(0, loc) );
      std::string options( aFieldSpec.substr(loc + 1) );
      iter = theStats.find(field);
      if (iter != theStats.end()) {
        return iter->second.asDouble(options);
      }
    }
    throw CalcException(std::string("{ERR:No Such Stat: ") + aFieldSpec + " }");
  }
}

int64_t SimpleMeasurement :: sumAsLongLong(std::string const & aFieldSpec) {
  try {
    boost::regex field_filter(aFieldSpec);
    int64_t sum = 0;
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        sum += iter->second.asLongLong();
      }
      ++iter;
    }

    return sum;
  } catch (boost::regex_error & anExcept) {
    throw CalcException(std::string("{ERR:Bad Field Filter: ") + aFieldSpec + " }");
  }
}

int64_t SimpleMeasurement :: minAsLongLong(std::string const & aFieldSpec) {
  try {
    boost::regex field_filter(aFieldSpec);
    boost::optional<int64_t> min;
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        if (! min ||  iter->second.asLongLong() < *min) {
          min.reset( iter->second.asLongLong() );
        }
      }
      ++iter;
    }
    if (!min) {
      min.reset(0);
    }

    return *min;
  } catch (boost::regex_error & anExcept) {
    throw CalcException(std::string("{ERR:Bad Field Filter: ") + aFieldSpec + " }");
  }
}

int64_t SimpleMeasurement :: maxAsLongLong(std::string const & aFieldSpec) {
  try {
    boost::regex field_filter(aFieldSpec);
    int64_t max = 0;
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        if (iter->second.asLongLong() > max) {
          max = iter->second.asLongLong();
        }
      }
      ++iter;
    }

    return max;
  } catch (boost::regex_error & anExcept) {
    throw CalcException(std::string("{ERR:Bad Field Filter: ") + aFieldSpec + " }");
  }
}

double SimpleMeasurement :: avgAsDouble(std::string const & aFieldSpec) {
  try {
    boost::regex field_filter(aFieldSpec);
    double sum = 0.0;
    int32_t count = 0;
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        sum += iter->second.asLongLong();
        ++count;
      }
      ++iter;
    }

    return sum / count;
  } catch (boost::regex_error & anExcept) {
    throw CalcException(std::string("{ERR:Bad Field Filter: ") + aFieldSpec + " }");
  }
}

void SimpleMeasurement :: doSUM(std::ostream & anOstream, std::string const & options) {

  try {
    anOstream << sumAsLongLong(options);
  } catch (CalcException & anException) {
    anOstream << anException.theReason;
  }

}

void accumBuckets( std::vector<int64_t> & buckets, std::vector<int64_t> const & hist) {
  if (buckets.size() < hist.size()) {
    buckets.resize(hist.size(), 0);
  }
  for (uint32_t i = 0; i < hist.size(); ++i) {
    buckets[i] += hist[i];
  }
}

void printBuckets( std::ostream & anOstream, std::vector<int64_t> const & buckets) {
  if (buckets.size() > 0 ) {
    int64_t sum = 0;
    for (uint32_t i = 0; i < buckets.size(); ++i) {
      sum += buckets[i];
    }

    int64_t running_sum = 0;
    float pct;
    anOstream << "\n\tBucket\tSize\tPct\tCum Pct.\n";
    anOstream << "\t" << 0 << ":" << "\t" << buckets[0];
    running_sum += buckets[0];
    pct = static_cast<float>(buckets[0]) / sum * 100;
    anOstream << "\t" << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
    anOstream << std::setw(6) << pct << "% \n";
    int32_t label = 1;
    for (uint32_t i = 1; i < buckets.size(); ++i) {
      running_sum += buckets[i];
      anOstream << std::left << "\t" << label << ":" << "\t" << buckets[i];
      pct = static_cast<float>(buckets[i]) / sum * 100;
      anOstream << "\t" << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
      pct = static_cast<float>(running_sum) / sum * 100;
      anOstream << std::setw(6) << pct << "% \n";
      label *= 2;
    }
    anOstream << "      --------- ------- ------- -------" << std::endl;
    anOstream << std::left << "\tTotal:" << "\t" << sum << "\t100.00%\t100.00%\n";
  }
}

void SimpleMeasurement :: doHISTSUM(std::ostream & anOstream, std::string const & options) {

  try {
    boost::regex field_filter(options);
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();
    std::vector<int64_t> buckets;

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        boost::intrusive_ptr< StatValueBase > val = iter->second.getValue();
        StatValueBase * val_ptr = val.get();
        StatValue_Log2Histogram * hist = dynamic_cast<StatValue_Log2Histogram *>(val_ptr);
        if (hist) {
          accumBuckets( buckets, hist->theBuckets);
        } else {
          StatValue_WeightedLog2Histogram * whist = dynamic_cast<StatValue_WeightedLog2Histogram *>(val_ptr);
          if (whist) {
            accumBuckets( buckets, whist->theBuckets);
          } else {
            anOstream << "{ERR: Don't know how to process histogram: " << iter->first << "}";
          }
        }
      }
      ++iter;
    }

    printBuckets( anOstream, buckets );

  } catch (boost::regex_error & anExcept) {
    anOstream <<  "{ERR:Bad Field Filter: " << options << " }";
  } catch (...) {
    anOstream << "{ERR:Unable to construct HISTSUM.  Stat is probably not a Log2Histogram}";
  }

}

void accumInst( std::map<int64_t, int64_t> & buckets, std::map<int64_t, int64_t> const & inst) {
  std::map<int64_t, int64_t>::const_iterator iter = inst.begin();
  std::map<int64_t, int64_t>::const_iterator end = inst.end();
  while (iter != end) {
    buckets[iter->first] += iter->second;
    ++iter;
  }
}

struct sort_helper {
  int64_t value;
  int64_t count;
  sort_helper() {}
  sort_helper(int64_t const & v, int64_t c)
    : value(v)
    , count(c)
  {}
  bool operator < (sort_helper const & other) const {
    return count > other.count;
  }
};

void printInst( std::ostream & anOstream, std::map<int64_t, int64_t> const  & buckets) {

  //Enroll all the counts and elements in a vector for sorting
  std::vector<sort_helper> elements;
  std::map<int64_t, int64_t>::const_iterator iter = buckets.begin();
  std::map<int64_t, int64_t>::const_iterator end = buckets.end();
  while (iter != end) {
    elements.push_back( sort_helper(iter->first, iter->second));
    ++iter;
  }

  std::sort(elements.begin(), elements.end());

  int64_t sum = 0;
  int64_t count = elements.size();

  for(auto& bucket: buckets){
    sum += bucket.second;
  }
  // std::for_each
  // ( buckets.begin()
  //   , buckets.end()
  //   , ll::var(sum) += ll::bind( &std::map<int64_t, int64_t>::value_type::second, ll::_1 )
  // );

  //Print implementation here
  anOstream << std::endl;
  anOstream << "      " << std::setw(10) << "Count" << std::setw(8) << "Pct" << "Value" << std::endl;

  float sum_f = sum;
  int64_t displayed_count = 0;
  int64_t displayed_sum = 0;
  int64_t overall_sum = 0;

  for (uint32_t i = 0; i < elements.size(); ++i) {
    float pct = static_cast<float>(elements[i].count) / sum_f * 100.0;
    if (displayed_count > 100) break;
    ++displayed_count;
    displayed_sum += elements[i].count;
    overall_sum += elements[i].count * elements[i].value;
    anOstream << "      ";
    anOstream << std::left << std::setw(10) << std::setfill(' ') << elements[i].count;
    anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
    anOstream << elements[i].value;
    anOstream << std::endl;
  }
  if ( displayed_count != count ) {
    float pct = static_cast<float>(sum - displayed_sum) / sum_f * 100.0;
    anOstream << "      ";
    anOstream << std::left << std::setw(10) << std::setfill(' ') << sum - displayed_sum;
    anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
    anOstream << "in " << count - displayed_count << " undisplayed elements";
    anOstream << std::endl;
  }
  anOstream << "      --------- ------- ----------" << std::endl;
  anOstream << "      ";
  anOstream << std::left << std::setw(10) << std::setfill(' ') << sum;
  anOstream << "100.00% ";
  anOstream << count << " elements";
  anOstream << std::endl;

  anOstream << "     Average value: " << std::left << std::setw(10) << std::setfill(' ')
            << (double)overall_sum / (double)displayed_sum << std::endl;
}

struct sort_value_helper {
  int64_t value;
  int64_t count;
  sort_value_helper() {}
  sort_value_helper(int64_t const & v, int64_t c)
    : value(v)
    , count(c)
  {}
  bool operator < (sort_value_helper const & other) const {
    return value < other.value;
  }
  int64_t product() const {
    return value * count;
  }
  int64_t sqsum() const {
    return value * value * count;
  }
};

void printInst2Hist( std::ostream & anOstream, std::map<int64_t, int64_t> const  & buckets, std::string options) {

  //Enroll all the counts and elements in a vector for sorting
  std::vector<sort_value_helper> elements;
  std::map<int64_t, int64_t>::const_iterator iter = buckets.begin();
  std::map<int64_t, int64_t>::const_iterator end = buckets.end();
  while (iter != end) {
    elements.push_back( sort_value_helper(iter->first, iter->second));
    ++iter;
  }

  std::sort(elements.begin(), elements.end());

  int64_t total_count = 0;
  int64_t weighted_total = 0;
  int64_t square_sum = 0;

  if (options.substr(0, 11) == "balance_no0") {
    if (elements[0].value == 0) {
      //Remove the zero bucket
      elements.erase(elements.begin());
    }
  }

  for(auto& element: elements){
    total_count += element.count;
    weighted_total += element.product();
    square_sum += element.sqsum();
  }

  // std::for_each
  // ( elements.begin()
  //   , elements.end()
  //   , ll::var(total_count) += ll::bind( &sort_value_helper::count, ll::_1 )
  // );

  // std::for_each
  // ( elements.begin()
  //   , elements.end()
  //   , ll::var(weighted_total) += ll::bind( &sort_value_helper::product, ll::_1 )
  // );

  // std::for_each
  // ( elements.begin()
  //   , elements.end()
  //   , ll::var(square_sum) += ll::bind( &sort_value_helper::sqsum, ll::_1 )
  // );

  double stdev = std::sqrt( square_sum / total_count - ( weighted_total / total_count ) * ( weighted_total / total_count ) );

  //Print implementation here
  anOstream << std::endl;
  anOstream << "      " << std::setw(10) << std::left << "Count" << std::setw(8) << "Pct" << std::setw(8) << "CumPct" << std::setw(8) << "Mean" << "  Range" << std::endl;

  float total_count_f = total_count;
  float cum_pct = 0;

  if (options.substr(0, 7) == "balance") {
    uint32_t i = 0;
    while (i < elements.size() ) {
      float tgt_pct = std::floor( cum_pct + 1 );
      int64_t min = elements[i].value;
      int64_t step_count = 0;
      int64_t weighted_sum = 0;
      int64_t max = 0;
      float pct = 0;
      while (i < elements.size() && cum_pct + pct < tgt_pct ) {
        weighted_sum += elements[i].count * elements[i].value;
        step_count += elements[i].count;
        pct = static_cast<float>(step_count) / total_count_f * 100.0;
        max = elements[i].value;
        ++i;
      }
      cum_pct += pct;
      anOstream << "      ";
      anOstream << std::left << std::setw(10) << std::setfill(' ') << step_count;
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << cum_pct << "%   ";
      anOstream << std::right << std::setprecision(1) << std::fixed << std::setw(6) << ((float)weighted_sum / step_count) << "  ";
      anOstream << min << '-' << max;
      anOstream << std::endl;
    }

  } else {
    int64_t min = elements.front().value;
    int64_t max = elements.back().value;
    int64_t step = (max - min) / 100;
    if (step == 0) {
      step = 1;
    }

    int32_t next_step = min + step;
    uint32_t i = 0;
    while (i < elements.size() ) {
      int64_t step_count = 0;
      while (i < elements.size() && elements[i].value < next_step) {
        step_count += elements[i].count;
        ++i;
      }
      float pct = static_cast<float>(step_count) / total_count_f * 100.0;
      cum_pct += pct;
      anOstream << "      ";
      anOstream << std::left << std::setw(10) << std::setfill(' ') << step_count;
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << cum_pct << "%   ";
      anOstream << std::right << std::setprecision(1) << std::fixed << std::setw(6) << (next_step - ((float)step) / 2) << "  ";
      anOstream << next_step - step << '-' << next_step;
      anOstream << std::endl;
      next_step += step;
    }

  }

  anOstream << "      --------- ------- ----------" << std::endl;
  anOstream << "      ";
  anOstream << std::left << std::setw(10) << std::setfill(' ') << total_count;
  anOstream << "100.00% ";
  anOstream << "100.00% ";
  anOstream << weighted_total << " weighted_total";
  anOstream << std::endl;

  anOstream << "     Average value: " << std::left << std::setw(10) << std::setfill(' ')
            << (double)weighted_total / (double)total_count;
  anOstream << "     Stdev: " << std::left << std::setw(10) << std::setfill(' ')
            << stdev << std::endl;
}

void SimpleMeasurement :: doINSTSUM(std::ostream & anOstream, std::string const & options) {

  try {
    boost::regex field_filter(options);
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();
    std::map<int64_t, int64_t> instances;

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        boost::intrusive_ptr< StatValueBase > val = iter->second.getValue();
        StatValueBase * val_ptr = val.get();
        StatValue_InstanceCounter<int64_t> * inst = dynamic_cast<StatValue_InstanceCounter<int64_t> *>(val_ptr);
        if (inst) {
          accumInst( instances, inst->theMap);
        } else {
          anOstream << "{ERR: Don't know how to process instance counter: " << iter->first << "}";
        }
      }
      ++iter;
    }

    printInst( anOstream, instances);

  } catch (boost::regex_error & anExcept) {
    anOstream <<  "{ERR:Bad Field Filter: " << options << " }";
  } catch (...) {
    anOstream << "{ERR:Unable to construct INSTSUM.  Stat is probably not an InstanceCounter}";
  }

}

void SimpleMeasurement :: doINST2HIST(std::ostream & anOstream, std::string const & options) {

  try {
    std::string field = options;
    std::string parameters;
    size_t loc = options.find(';');
    if (loc != std::string::npos) {
      field = options.substr(0, loc);
      parameters = options.substr(loc + 1);
    }

    boost::regex field_filter(field);
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();
    std::map<int64_t, int64_t> instances;

    while (iter != end) {
      if (boost::regex_match(iter->first, field_filter)) {
        boost::intrusive_ptr< StatValueBase > val = iter->second.getValue();
        StatValueBase * val_ptr = val.get();
        StatValue_InstanceCounter<int64_t> * inst = dynamic_cast<StatValue_InstanceCounter<int64_t> *>(val_ptr);
        if (inst) {
          accumInst( instances, inst->theMap);
        } else {
          anOstream << "{ERR: Don't know how to process instance counter: " << iter->first << "}";
        }
      }
      ++iter;
    }

    printInst2Hist( anOstream, instances, parameters);

  } catch (boost::regex_error & anExcept) {
    anOstream <<  "{ERR:Bad Field Filter: " << options << " }";
  } catch (...) {
    anOstream << "{ERR:Unable to construct INSTSUM.  Stat is probably not an InstanceCounter}";
  }

}

void SimpleMeasurement :: doCSV(std::ostream & anOstream, std::string const & options) {

  try {
    boost::regex options_parser("\\(([^\\)]*)\\)(:)?(.*)?");
    boost::smatch results;
    if ( boost::regex_match(options, results, options_parser )) {

      boost::regex field_filter(results.str(1));
      std::string field_options;
      if (results[3].matched) {
        field_options = results.str(3);
      }

      bool first = true;
      stat_handle_map::iterator iter = theStats.begin();
      stat_handle_map::iterator end = theStats.end();

      while (iter != end) {
        if (boost::regex_match(iter->first, field_filter)) {
          if (!first) {
            anOstream << " ";
          }
          first = false;
          iter->second.print(anOstream, field_options);
        }
        ++iter;
      }
    } else {

    }

  } catch (boost::regex_error & anExcept) {
    anOstream << "{ERR:Bad Field Filter}";
  }

}

void SimpleMeasurement :: reduce(eReduction aReduction, Measurement * aMeasurement) {
  SimpleMeasurement * simple = dynamic_cast<SimpleMeasurement *>(aMeasurement);
  if (!simple) {
    std::cerr << "{ERR: Cannot collapse measurement " << aMeasurement->name() << " (not a SimpleMeasurement)}" << std::endl;
    return;
  }
  //std::cerr << "Collapsing measurement: " << aMeasurement->name() << std::endl;

  for(auto& aStat: simple->theStats){
    if (theStats.find(aStat.first) == theStats.end()) {
      try {
        switch ( aReduction) {
          case eReduction::eSum: {
            auto accumulator = aStat.second.sumAccumulator();
            theStats[aStat.first] = StatValueHandle(aStat.first, accumulator);
            break;
          }
          case eReduction::eAverage: {
            auto accumulator = aStat.second.avgAccumulator();
            theStats[aStat.first] = StatValueHandle(aStat.first, accumulator);
            break;
          }
          case eReduction::eStdDev: {
            auto accumulator = aStat.second.stdevAccumulator();
            theStats[aStat.first] = StatValueHandle(aStat.first, accumulator);
            break;
          }
          case eReduction::eCount: {
            auto accumulator = aStat.second.countAccumulator();
            theStats[aStat.first] = StatValueHandle(aStat.first, accumulator);
            break;
          }
          default:
            DBG_Assert(false);
        }
      } catch ( ... ) {
        //std::cerr  << "Can't accumulate stat " << iter->first << std::endl;
        theStats.erase(aStat.first);
      }
    } else {
      try {
        switch ( aReduction) {
          case eReduction::eSum: {
            auto accumulator = theStats[aStat.first].getValue();
            fold( *accumulator, * (aStat.second.getValue()), & StatValueBase::reduceSum );
            theStats[aStat.first].setValue(accumulator);
            break;
          }
          case eReduction::eAverage: {
            auto accumulator = theStats[aStat.first].getValue();
            fold( *accumulator, * (aStat.second.getValue()), & StatValueBase::reduceAvg );
            theStats[aStat.first].setValue(accumulator);
            break;
          }
          case eReduction::eStdDev: {
            auto accumulator = theStats[aStat.first].getValue();
            fold( *accumulator, * (aStat.second.getValue()), & StatValueBase::reduceStdDev );
            theStats[aStat.first].setValue(accumulator);
            break;
          }
          case eReduction::eCount: {
            auto accumulator = theStats[aStat.first].getValue();
            fold( *accumulator, * (aStat.second.getValue()), & StatValueBase::reduceCount );
            theStats[aStat.first].setValue(accumulator);
            break;
          }
          default:
            DBG_Assert(false);
        }
      } catch ( ... ) {
        //std::cerr  << "Can't accumulate stat " << iter->first << std::endl;
        theStats.erase(aStat.first);
      }
    }
  }
  // stat_handle_map::iterator iter = simple->theStats.begin();
  // stat_handle_map::iterator end = simple->theStats.end();
  // while (iter != end) {
  //   if (theStats.find(iter->first) == theStats.end()) {
  //     try {
  //       switch ( aReduction) {
  //         case eReduction::eSum: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = iter->second.sumAccumulator();
  //           theStats[iter->first] = StatValueHandle(iter->first, accumulator);
  //           break;
  //         }
  //         case eReduction::eAverage: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = iter->second.avgAccumulator();
  //           theStats[iter->first] = StatValueHandle(iter->first, accumulator);
  //           break;
  //         }
  //         case eReduction::eStdDev: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = iter->second.stdevAccumulator();
  //           theStats[iter->first] = StatValueHandle(iter->first, accumulator);
  //           break;
  //         }
  //         case eReduction::eCount: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = iter->second.countAccumulator();
  //           theStats[iter->first] = StatValueHandle(iter->first, accumulator);
  //           break;
  //         }
  //         default:
  //           DBG_Assert(false);
  //       }
  //     } catch ( ... ) {
  //       //std::cerr  << "Can't accumulate stat " << iter->first << std::endl;
  //       theStats.erase(iter->first);
  //     }
  //   } else {
  //     try {
  //       switch ( aReduction) {
  //         case eReduction::eSum: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = theStats[iter->first].getValue();
  //           fold( *accumulator, * (iter->second.getValue()), & StatValueBase::reduceSum );
  //           theStats[iter->first].setValue(accumulator);
  //           break;
  //         }
  //         case eReduction::eAverage: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = theStats[iter->first].getValue();
  //           fold( *accumulator, * (iter->second.getValue()), & StatValueBase::reduceAvg );
  //           theStats[iter->first].setValue(accumulator);
  //           break;
  //         }
  //         case eReduction::eStdDev: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = theStats[iter->first].getValue();
  //           fold( *accumulator, * (iter->second.getValue()), & StatValueBase::reduceStdDev );
  //           theStats[iter->first].setValue(accumulator);
  //           break;
  //         }
  //         case eReduction::eCount: {
  //           boost::intrusive_ptr<StatValueBase> accumulator = theStats[iter->first].getValue();
  //           fold( *accumulator, * (iter->second.getValue()), & StatValueBase::reduceCount );
  //           theStats[iter->first].setValue(accumulator);
  //           break;
  //         }
  //         default:
  //           DBG_Assert(false);
  //       }
  //     } catch ( ... ) {
  //       //std::cerr  << "Can't accumulate stat " << iter->first << std::endl;
  //       theStats.erase(iter->first);
  //     }
  //   }
  //   ++iter;
  // }
}

void SimpleMeasurement :: reduceNodes() {
  boost::regex perNodeFilter("\\d+-(.*)");
  std::string extractStatExp("\\1");
  std::deque<std::string> toDelete;
  std::deque<std::string>::iterator delIter;

  stat_handle_map::iterator iter = theStats.begin();
  //stat_handle_map::iterator end = theStats.end();
  while (iter != theStats.end()) {
    boost::smatch pieces;
    if (boost::regex_match(iter->first, pieces, perNodeFilter)) {
      std::string nodeStatName = std::string("Nodes-") + pieces[1].str();
      if (theStats.find(nodeStatName) == theStats.end()) {
        theStats[nodeStatName] = (iter->second);
        theStats[nodeStatName].rename(nodeStatName);
      } else {
        try {
          theStats[nodeStatName] += (iter->second);
        } catch (...) { }
      }
      toDelete.push_back(iter->first);
    }
    ++iter;
  }

  for (delIter = toDelete.begin(); delIter != toDelete.end(); ++delIter) {
    theStats.erase(*delIter);
  }

}

PeriodicMeasurement::PeriodicMeasurement( std::string const & aName, std::string const & aStatExpression, int64_t aPeriod, accumulation_type anAccumulationType)
  : Measurement(aName, aStatExpression)
  , thePeriod(aPeriod)
  , theCurrentPeriod(0)
  , theCancelled(false)
  , theAccumulationType(anAccumulationType) {
  //Period of zero is not allowed.
  if (aPeriod > 0) {
    getStatManager()->addEvent(getStatManager()->ticks() + aPeriod, [this](){ return this->fire(); }); //ll::bind( &PeriodicMeasurement::fire, this) );
  }
}

void PeriodicMeasurement :: addToMeasurement( Stat * aStat ) {
  //Check if the stat should be included in this measurement
  if (includeStat(aStat)) {
    theStats[ aStat->name() ] = aStat->createValueArray();
  }
}

void PeriodicMeasurement :: close() {
  theCancelled = true;
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  while (iter != end) {
    iter->second.releaseUpdater();
    ++iter;
  }
}

void PeriodicMeasurement :: print(std::ostream & anOstream, std::string const & options) {
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  anOstream << *this << std::endl;
  while (iter != end) {
    anOstream << "   " << iter->second << std::endl;
    ++iter;
  }
}

void PeriodicMeasurement :: format(std::ostream & anOstream, std::string const & aField, std::string const & options) {
  stat_handle_map::iterator iter = theStats.find(aField);
  if (iter != theStats.end()) {
    iter->second.print(anOstream, options);
  } else {
    anOstream << "{ERR:No Such Stat}";
  }
}

void PeriodicMeasurement :: fire () {
  if (! theCancelled) {
    stat_handle_map::iterator iter = theStats.begin();
    stat_handle_map::iterator end = theStats.end();
    while (iter != end) {
      iter->second.newValue(theAccumulationType);
      ++iter;
    }

    getStatManager()->addEvent(getStatManager()->ticks() + thePeriod, [this](){ return this->fire(); }); //ll::bind( &PeriodicMeasurement::fire, this) );
  }
}

LoggedPeriodicMeasurement::LoggedPeriodicMeasurement( std::string const & aName, std::string const & aStatExpression, int64_t aPeriod, accumulation_type anAccumulationType, std::ostream & anOstream)
  : Measurement(aName, aStatExpression)
  , thePeriod(aPeriod)
  , theCurrentPeriod(0)
  , theCancelled(false)
  , theFirst(true)
  , theAccumulationType(anAccumulationType)
  , theOstream(anOstream) {
  //Period of zero is not allowed.
  if (aPeriod > 0) {
    getStatManager()->addEvent(getStatManager()->ticks() + aPeriod, [this](){ return this->fire(); }); //ll::bind( &LoggedPeriodicMeasurement::fire, this) );
  }
}

void LoggedPeriodicMeasurement :: addToMeasurement( Stat * aStat ) {
  //Check if the stat should be included in this measurement
  if (includeStat(aStat)) {
    theStats[ aStat->name() ] = aStat->createValue();
  }
}

void LoggedPeriodicMeasurement :: close() {
  theCancelled = true;
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  while (iter != end) {
    iter->second.releaseUpdater();
    ++iter;
  }
}

void LoggedPeriodicMeasurement :: print(std::ostream & anOstream, std::string const & options) {
  stat_handle_map::iterator iter = theStats.begin();
  stat_handle_map::iterator end = theStats.end();
  anOstream << getStatManager()->ticks();
  while (iter != end) {
    anOstream << ", ";
    iter->second.print(anOstream);
    ++iter;
  }
  anOstream << std::endl;
}

void LoggedPeriodicMeasurement :: format(std::ostream & anOstream, std::string const & aField, std::string const & options) {
  stat_handle_map::iterator iter = theStats.find(aField);
  if (iter != theStats.end()) {
    iter->second.print(anOstream, options);
  } else {
    anOstream << "{ERR:No Such Stat}";
  }
}

void LoggedPeriodicMeasurement  :: fire () {
  if (! theCancelled) {
    if (theFirst) {
      stat_handle_map::iterator iter = theStats.begin();
      stat_handle_map::iterator end = theStats.end();
      theOstream << "ticks";
      while (iter != end) {
        theOstream << ", " << iter->second.name();
        ++iter;
      }
      theOstream << std::endl;
      theFirst = false;
    }

    print(theOstream);
    theOstream.flush();

    if (theAccumulationType == accumulation_type::Reset) {
      for(auto& aStat:theStats)
        aStat.second.reset();
      // stat_handle_map::iterator iter = theStats.begin();
      // stat_handle_map::iterator end = theStats.end();
      // while (iter != end) {
      //   iter->second.reset();
      //   ++iter;
      // }
    }

    getStatManager()->addEvent(getStatManager()->ticks() + thePeriod, [this](){ return this->fire(); }); //ll::bind( &LoggedPeriodicMeasurement::fire, this) );
  }
}

}

} // end Stat
} // end Flexus

