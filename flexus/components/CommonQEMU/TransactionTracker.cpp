// DO-NOT-REMOVE begin-copyright-block 
//QFlex consists of several software components that are governed by various
//licensing terms, in addition to software that was developed internally.
//Anyone interested in using QFlex needs to fully understand and abide by the
//licenses governing all the software components.
//
//### Software developed externally (not by the QFlex group)
//
//    * [NS-3](https://www.gnu.org/copyleft/gpl.html)
//    * [QEMU](http://wiki.qemu.org/License) 
//    * [SimFlex] (http://parsa.epfl.ch/simflex/)
//
//Software developed internally (by the QFlex group)
//**QFlex License**
//
//QFlex
//Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification,
//are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//    * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//      nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written
//      permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
//EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// DO-NOT-REMOVE end-copyright-block   
#include <algorithm>

#include <memory>
#include <core/stats.hpp>

#include <components/CommonQEMU/Slices/TransactionTracker.hpp>

#define DBG_DefineCategories TransactionTrace, TransactionDetailsTrace
#include DBG_Control()

static const int32_t kMinLatency = 20;

namespace Flexus {
namespace SharedTypes {

namespace Stat = Flexus::Stat;
// namespace ll = boost::lambda;

uint64_t theGloballyUniqueId = 0;
uint64_t getTTGUID() {
  return theGloballyUniqueId++;
}
std::shared_ptr<TransactionTracer> TransactionTracker::theTracer;
std::shared_ptr<TransactionStatManager> TransactionTracker::theTSM;

class TransactionTracerImpl : public TransactionTracer {
  bool includeTransaction(TransactionTracker const & aTransaction) {
    //Fields required for trace
    bool meets_requirements
    = aTransaction.initiator()
      && (*aTransaction.initiator() == 0)
      && aTransaction.address()
      && aTransaction.completionCycle()
      ;
    if (meets_requirements) {
      if (aTransaction.fillLevel() && *aTransaction.fillLevel() == ePrefetchBuffer) {
        //Always include prefetch buffer fills
        meets_requirements = true;
      } else {
        //Include fills with a latency of > 20 cycles
        int64_t latency = *aTransaction.completionCycle() - aTransaction.startCycle();
        meets_requirements = (latency > kMinLatency);
      }
    }
    return meets_requirements ;
  }

  static void processCycles( std::tuple<std::string, std::string, int> const & aCause) {
    DBG_(VVerb,
         Cat(TransactionDetailsTrace)
         Set( (Component) << std::get<0>(aCause) )
         Set( (Cause) << std::get<1>(aCause) )
         SetNumeric( (Cycles) std::get<2>(aCause) )
        );
  }

  void processTransaction(TransactionTracker const & aTransaction) {
    int32_t initiator = *aTransaction.initiator();
    (void) initiator; //suppress warning
    int64_t latency = *aTransaction.completionCycle() - aTransaction.startCycle();
    (void) latency; //suppress warning

    int32_t responder = -1;
    if (aTransaction.responder()) {
      responder = *aTransaction.responder();
    }

    std::string OS = "Non-OS";
    if (aTransaction.OS() && *aTransaction.OS()) {
      OS = "OS";
    }

    std::string critical = "Non-Critical";
    if (aTransaction.criticalPath() && *aTransaction.criticalPath()) {
      critical = "Critical";
    }

    std::string source = "Unknown";
    if (aTransaction.source()) {
      source = *aTransaction.source();
    }

    std::string fill_level = "Unknown";
    if (aTransaction.fillLevel()) {
      switch (*aTransaction.fillLevel()) {
        case eL1:
          fill_level = "L1";
          break;
        case eL2:
          fill_level = "L2";
          break;
        case eL3:
          fill_level = "L3";
          break;
        case eLocalMem:
        case eRemoteMem:
          fill_level = "Memory";
          break;
        case ePrefetchBuffer:
          fill_level = "PrefetchBuffer";
          break;
        default:
          break;
      }
    }

    std::string complete = "";
    if (! aTransaction.completionCycle()) {
      complete = "Incomplete";
    }

    DBG_(VVerb,
         Cat(TransactionTrace)
         SetNumeric( (InitiatingNode) initiator)
         Set ( (Source) << source)
         SetNumeric( (RespondingNode) responder)
         Set( (FillLevel) << fill_level )
         Set( (Address) << "0x" << std::hex << std::setw(8) << std::setfill('0') << *aTransaction.address() )
         SetNumeric( (Latency) latency)
         Set( (OS) << OS )
         Set( (Critical) << critical )
         Set( (Complete) << complete )
        );

    // std::for_each( aTransaction.cycleAccounting().begin(), aTransaction.cycleAccounting().end(), &processCycles );

  }

  void trace(TransactionTracker const & aTransaction) {
    if ( includeTransaction(aTransaction) ) {
      processTransaction(aTransaction);
    }
  }
};

std::shared_ptr<TransactionTracer> TransactionTracer::createTracer() {
  return std::shared_ptr<TransactionTracer>(new TransactionTracerImpl());
}

struct XactCatStats {
  std::string theCategoryName;

  Stat::StatCounter theCount;
  Stat::StatAverage theAvgLatency;
  std::map<std::string, boost::intrusive_ptr<Stat::StatCounter> > theTotalDelayCounters;

  XactCatStats (std::string aCategoryName)
    : theCategoryName(aCategoryName)
    , theCount(std::string("Xacts<") + aCategoryName + ">-Count")
    , theAvgLatency(std::string("Xacts<") + aCategoryName + ">-AvgLatency")
  {}

  void accountDelay(std::string aReason, int32_t aDelay) {
    if ( ! theTotalDelayCounters[aReason] ) {
      theTotalDelayCounters[aReason] =
        boost::intrusive_ptr<Stat::StatCounter>(
          new Stat::StatCounter( std::string("Xacts<") + theCategoryName + ">-" + aReason + "Delay")
        );
    }
    *theTotalDelayCounters[aReason] += aDelay;
  }
};

class TransactionStatManagerImpl : public TransactionStatManager {

  XactCatStats theCriticalXacts;
  XactCatStats theNonCriticalXacts;

  XactCatStats theOSXacts;
  XactCatStats theNonOSXacts;

  XactCatStats theL1FillXacts;
  XactCatStats theL2FillXacts;
  XactCatStats theLocalFillXacts;
  XactCatStats theRemoteFillXacts;
  XactCatStats thePrefetchFillXacts;

  Stat::StatCounter theTransactionsMissingRequiredFields;
public:
  TransactionStatManagerImpl()
    : theCriticalXacts("Critical")
    , theNonCriticalXacts("NonCritical")
    , theOSXacts("OS")
    , theNonOSXacts("NonOS")
    , theL1FillXacts("L1")
    , theL2FillXacts("L2")
    , theLocalFillXacts("Local")
    , theRemoteFillXacts("Remote")
    , thePrefetchFillXacts("Prefetch")
    , theTransactionsMissingRequiredFields("Xacts-MissingRequiredFields")
  {}

  virtual ~TransactionStatManagerImpl() {}

private:
  void categorize(TransactionTracker const & aTransaction, std::vector<XactCatStats *> & aCategoryList) {
    //Required fields for any category
    if ( ! ( aTransaction.initiator() && aTransaction.address() && aTransaction.completionCycle() ) ) {
      ++theTransactionsMissingRequiredFields;
      return; //No categories allowed for
    }

    if (aTransaction.criticalPath() && *aTransaction.criticalPath()) {
      aCategoryList.push_back( & theCriticalXacts );
    } else {
      aCategoryList.push_back( & theNonCriticalXacts );
    }

    if (aTransaction.OS() && *aTransaction.OS()) {
      aCategoryList.push_back( & theOSXacts );
    } else {
      aCategoryList.push_back( & theNonOSXacts );
    }

    if (aTransaction.fillLevel()) {
      switch (*aTransaction.fillLevel()) {
        case eL1:
          aCategoryList.push_back( & theL1FillXacts );
          break;
        case eL2:
          aCategoryList.push_back( & theL2FillXacts );
          break;
        case eLocalMem:
          aCategoryList.push_back( & theLocalFillXacts );
          break;
        case eRemoteMem:
          aCategoryList.push_back( & theRemoteFillXacts );
          break;
        case ePrefetchBuffer:
          aCategoryList.push_back( & thePrefetchFillXacts );
          break;
        default:
          break;
      }
    }

  }

  template <class StatT, class UpdateT >
  void accum( StatT XactCatStats::* aCounter, UpdateT anUpdate, std::vector<XactCatStats *> & aCategoryList) {
    for(auto aCategory: aCategoryList){
      aCounter(aCategory) += anUpdate; 
    }
    // using ll::_1;
    // using ll::bind;
    // std::for_each( aCategoryList.begin(), aCategoryList.end(), bind( aCounter, _1) += anUpdate);
  }

  template <class StatT, class UpdateT >
  void append( StatT XactCatStats::* aCounter, UpdateT anUpdate, std::vector<XactCatStats *> & aCategoryList) {
    for(auto aCategory: aCategoryList){
      aCounter(aCategory) << anUpdate; 
    }
    // using ll::_1;
    // using ll::bind;
    // std::for_each( aCategoryList.begin(), aCategoryList.end(), bind( aCounter, _1) << anUpdate);
  }

  static void accountCycles( std::vector<XactCatStats *> const & aCategoryList, std::tuple<std::string, std::string, int> const & aDetail ) {
    // using ll::_1;
    // using ll::bind;

    for(auto aCaterory: aCategoryList){
      aCaterory->accountDelay(std::get<0>(aDetail), std::get<2>(aDetail));
      aCaterory->accountDelay(std::get<1>(aDetail), std::get<2>(aDetail));
    }
    // std::for_each
    // ( aCategoryList.begin()
    //   , aCategoryList.end()
    //   , ( bind( & XactCatStats::accountDelay , _1, std::get<0>(aDetail), std::get<2>(aDetail) )
    //       , bind( & XactCatStats::accountDelay , _1, std::get<1>(aDetail), std::get<2>(aDetail) )
    //     )
    // );
  }

public:
  virtual void count(TransactionTracker const & aTransaction) {
    /*
    using ll::bind;
    using ll::_1;

    std::vector<XactCatStats *> categories;
    categorize(aTransaction, categories);

    if (categories.size() > 0) {

      accum( & XactCatStats::theCount, 1, categories);

      int64_t latency = *aTransaction.completionCycle() - aTransaction.startCycle();
      append( & XactCatStats::theAvgLatency, latency, categories);

      std::for_each
        ( aTransaction.cycleAccounting().begin()
        , aTransaction.cycleAccounting().end()
        , bind(& TransactionStatManagerImpl::accountCycles, categories, _1)
        );
    }
    */
  }

};

std::shared_ptr<TransactionStatManager> TransactionStatManager::createTSM() {
  return std::shared_ptr<TransactionStatManager>(new TransactionStatManagerImpl());
}

} //SharedTypes
} //Flexus
