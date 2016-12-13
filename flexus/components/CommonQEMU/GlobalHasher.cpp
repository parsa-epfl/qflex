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
#include <iostream>
#include <sstream>

#include <core/target.hpp>
#include <core/debug/debug.hpp>
#include <core/types.hpp>
#include <core/stats.hpp>

#include "GlobalHasher.hpp"
#include <components/CommonQEMU/Util.hpp>

#define DBG_DefineCategories GlobalHasherCat
#define DBG_SetDefaultOps AddCat(GlobalHasherCat)
#include DBG_Control()

using namespace Flexus;
using namespace Core;
using namespace Flexus::Core;
using namespace Flexus::SharedTypes;

using nCommonUtil::log_base2;

namespace nGlobalHasher {

GlobalHasher::GlobalHasher() {
}

std::set<int> GlobalHasher::hashAddr(const Address & addr) {
  std::set<int> ret;
  hash_iterator_t hash = theHashList.begin();
  for (; hash != theHashList.end(); hash++) {
    ret.insert( (*hash)(addr) );
  }
  return ret;
}

int32_t GlobalHasher::simple_hash(int32_t offset, const Address & addr) const {
  return ((addr >> theHashShift) & theHashMask) + offset;
}

int32_t GlobalHasher::xor_hash(int32_t offset, int32_t xor_shift, const Address & addr) const {
  int32_t ret = (((addr >> theHashShift) ^ (addr >> xor_shift)) & theHashMask) + offset;
  return (((addr >> theHashShift) ^ (addr >> xor_shift)) & theHashMask) + offset;
}

int32_t GlobalHasher::shift_hash(int32_t offset, int32_t shift, const Address & addr) const {
  return ((addr >> (theHashShift + shift)) & theHashMask) + offset;
}

int32_t GlobalHasher::full_prime_hash(int32_t offset, int32_t prime, const Address & addr) const {
  return (addr % prime) + offset;
}

struct MatrixHash {
public:
  MatrixHash(const std::list<int> &matrix, int32_t shift, int32_t mask, int32_t offset)
    : theMatrix(matrix), theHashShift(shift), theHashMask(mask), theOffset(offset) {}
  int32_t operator()(uint64_t addr) {
    addr >>= theHashShift;
    int32_t ret = 0;
    std::list<int>::iterator it = theMatrix.begin();
    for (; it != theMatrix.end(); it++, addr >>= 1) {
      if ((addr & 1) != 0) {
        ret ^= *it;
      }
    }
    return (ret & theHashMask) + theOffset;
  }
private:
  std::list<int>  theMatrix;
  int32_t             theHashShift;
  int32_t             theHashMask;
  int    theOffset;
};

std::function<int(int)> GlobalHasher::createMatrixHash(std::string args, int32_t num_buckets, int32_t shift, int32_t mask, int32_t offset) const {
  std::list<int> matrix;
  if (strncasecmp(args.c_str(), "random", 6) == 0) {
    uint32_t seed = boost::lexical_cast<uint32_t>(args.substr(7));
    std::srand(seed);
    int32_t n = 32 - shift;
    double max = num_buckets;
    // Generate a list of n numbers with theHashBits bits
    for (int32_t i = 0; i < n; i++) {
      int32_t random = (int)(max * std::rand() / (RAND_MAX + 1.0));
      matrix.push_back(random);
    }
  } else if (strncasecmp(args.c_str(), "list", 4) == 0) {
    std::string matrix_list = args.substr(5);
    // The matrix is a list of comma-separated numbers
    std::string::size_type pos = 0;
    int32_t num = 0;
    do {
      pos = matrix_list.find(',', 0);
      if (pos == std::string::npos) {
        num = boost::lexical_cast<int>(matrix_list);
      } else {
        num = boost::lexical_cast<int>(matrix_list.substr(0, pos));
        matrix_list = matrix_list.substr(pos + 1);
      }
      matrix.push_back(num);
    } while (pos != std::string::npos);
  } else {
    DBG_Assert(false, ( << "Unknown Matrix Hash Type '" << args << "'" ));
  }

  return MatrixHash(matrix, shift, mask, offset);
}

void GlobalHasher::initialize(std::list<std::string> &hash_configs, int32_t initial_shift, int32_t buckets_per_hash, bool partitioned) {
  if (!has_been_initialized) {
    int32_t first_bucket = 0;
    int32_t offset = ( partitioned ? buckets_per_hash : 0);

    theHashShift = initial_shift;
    theHashMask = (buckets_per_hash - 1);

    std::list<std::string>::iterator cfg = hash_configs.begin();
    for (; cfg != hash_configs.end(); cfg++, first_bucket += offset) {
      if (strcasecmp(cfg->c_str(), "simple") == 0) {
        theHashList.push_back([this, first_bucket](const Address& x){ return this->simple_hash(first_bucket, x); });//std::bind(&GlobalHasher::simple_hash, this, first_bucket, _1));
        DBG_(Dev, ( << "Added simple hash function to global hasher." ) );
      } else if (strncasecmp(cfg->c_str(), "xor", 3) == 0) {
        int32_t xor_shift = boost::lexical_cast<int>(cfg->substr(4));
        theHashList.push_back([this, first_bucket, xor_shift](const Address& x){ return this->xor_hash(first_bucket, xor_shift, x); });//std::bind(&GlobalHasher::xor_hash, this, first_bucket, xor_shift, _1));
        DBG_(Dev, ( << "Added XOR hash function to global hasher. XOR Shift = " << xor_shift << ", first = " << first_bucket ));
      } else if (strncasecmp(cfg->c_str(), "shift", 5) == 0) {
        int32_t shift = boost::lexical_cast<int>(cfg->substr(6));
        theHashList.push_back([this, first_bucket, shift](const Address& x){ return this->shift_hash(first_bucket, shift, x); }); //std::bind(&GlobalHasher::shift_hash, this, first_bucket, shift, _1));
        DBG_(Dev, ( << "Added Shift hash function to global hasher. Shift = " << shift ));
      } else if (strncasecmp(cfg->c_str(), "matrix", 6) == 0) {
        theHashList.push_back(createMatrixHash(cfg->substr(7), buckets_per_hash, theHashShift, theHashMask, first_bucket));
        DBG_(Dev, ( << "Added Matrix hash function to global hasher. Args = " << cfg->substr(7) ));
      } else if (strcasecmp(cfg->c_str(), "full_prime") == 0) {
        int32_t closest_prime = nCommonUtil::get_closest_prime(buckets_per_hash);
        theHashList.push_back([this, closest_prime, first_bucket](const Address& x){ return this->shift_hash(closest_prime, first_bucket, x); });//std::bind(&GlobalHasher::shift_hash, this, closest_prime, first_bucket, _1));
        DBG_(Dev, ( << "Added Full Prime hash function to global hasher. Closest prime = " << closest_prime ));
      }
    }
    has_been_initialized = true;
  }
}

GlobalHasher & GlobalHasher::theHasher() {
  static GlobalHasher my_hasher;
  return my_hasher;
}

};

