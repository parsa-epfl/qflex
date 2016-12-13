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
#ifndef _GLOBAL_HASHER_HPP_
#define _GLOBAL_HASHER_HPP_

#include <set>
#include <list>

#include <functional>

namespace nGlobalHasher {

typedef Flexus::SharedTypes::PhysicalMemoryAddress Address;

class GlobalHasher {
private:
  typedef std::function<int(const Address &)> hash_fn_t;
  typedef std::list<hash_fn_t>::iterator hash_iterator_t;

  GlobalHasher();

  int32_t theHashShift;
  int32_t theHashMask;

  std::list<hash_fn_t> theHashList;

  bool has_been_initialized;

  int32_t simple_hash(int32_t offset, const Address & addr) const;
  int32_t xor_hash(int32_t offset, int32_t xor_shift, const Address & addr) const;
  int32_t shift_hash(int32_t offset, int32_t shift, const Address & addr) const;
  int32_t full_prime_hash(int32_t offset, int32_t prime, const Address & addr) const;

  std::function<int(int)> createMatrixHash(std::string args, int32_t num_buckets, int32_t shift, int32_t mask, int32_t offset) const;

public:
  std::set<int> hashAddr(const Address & addr);

  void initialize(std::list<std::string> &hash_configs, int32_t initial_shift, int32_t buckets_per_hash, bool partitioned);

  int32_t numHashes() {
    return theHashList.size();
  }

  static GlobalHasher & theHasher();
};
};

#endif // ! _GLOBAL_HASHER_HPP_
