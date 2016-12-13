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
#ifndef ABSTRACT_FACTORY_HPP
#define ABSTRACT_FACTORY_HPP

#include <iostream>

#include <map>
#include <list>
#include <functional>

template<class _AbstractType, typename _ParamType>
class AbstractFactory {
public:
  //typedef _AbstractType *(cons_func_t)(std::list< std:pair<std::string, std::string> >&);
  typedef std::function<_AbstractType* ( std::list< std::pair<std::string, std::string> >&, const _ParamType & )> cons_func_t;

private:
  typedef std::map<std::string, cons_func_t> factory_map_t;

  static factory_map_t & factory_map() {
    static factory_map_t my_map;
    return my_map;
  };

public:
  static void registerConstructor(const std::string & name, cons_func_t fn) {
    std::pair<typename factory_map_t::iterator, bool> ret = factory_map().insert( std::make_pair(name, fn) );
    DBG_Assert(ret.second, ( << "Failed to register Constructor with name '" << name << "'" ) );
  }

  static _AbstractType * createInstance(std::string args, const _ParamType & params) {
    std::string::size_type loc = args.find(':', 0);
    std::string name = args.substr(0, loc);
    if (loc != std::string::npos) {
      args = args.substr(loc + 1);
    } else {
      args = "";
    }
    typename factory_map_t::iterator iter = factory_map().find(name);
    if (iter != factory_map().end()) {
      std::list< std::pair<std::string, std::string> > arg_list;
      std::string key;
      std::string value;
      std::string::size_type pos = 0;
      do {
        pos = args.find(':', 0);
        std::string cur_arg = args.substr(0, pos);
        std::string::size_type equal_pos = cur_arg.find('=', 0);
        if (equal_pos == std::string::npos) {
          key = cur_arg;
          value = "";
        } else {
          key = cur_arg.substr(0, equal_pos);
          value = cur_arg.substr(equal_pos + 1);
        }
        if (key.length() > 0) {
          arg_list.push_back(std::make_pair(key, value));
        }

        if (pos != std::string::npos) {
          args = args.substr(pos + 1);
        }
      } while (pos != std::string::npos);

      return iter->second(arg_list, params);
    }
    iter = factory_map().begin();
    for (; iter != factory_map().end(); iter++) {
      std::cout << "FactoryMap contains: " << iter->first << std::endl;
    }
    DBG_Assert(false, ( << "Failed to create Instance of '" << name << "'" ) );
    return nullptr;
  }

};

template<typename _AbstractType, typename _ConcreteType, typename _ParamType>
class ConcreteFactory {
public:
  ConcreteFactory() {
    AbstractFactory<_AbstractType, _ParamType>::registerConstructor(_ConcreteType::name, &_ConcreteType::createInstance);
  };
};

#endif // ABSTRACT_FACTORY_HPP
