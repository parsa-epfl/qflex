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
#ifndef ABSTRACT_FACTORY_HPP
#define ABSTRACT_FACTORY_HPP

#include <iostream>

#include <map>
#include <list>
#include <functional>

class Dummy {
public:
  Dummy(int32_t x) {
    DBG_( Dev, ( << "Dummy constructor " << x << "." ));
  }
};

template<class _AbstractType>
class AbstractFactory {
public:
  //typedef _AbstractType *(cons_func_t)(std::list< std:pair<std::string, std::string> >&);
  typedef std::function<_AbstractType* ( std::list< std::pair<std::string, std::string> >& )> cons_func_t;

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

  static _AbstractType * createInstance(std::string args) {
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
          value = "1";
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

      return iter->second(arg_list);
    }
    iter = factory_map().begin();
    for (; iter != factory_map().end(); iter++) {
      std::cout << "FactoryMap contains: " << iter->first << std::endl;
    }
    DBG_Assert(false, ( << "Failed to create Instance of '" << name << "'" ) );
    return nullptr;
  }

};

template<typename _AbstractType, typename _ConcreteType>
class ConcreteFactory {
public:
  ConcreteFactory() {
    AbstractFactory<_AbstractType>::registerConstructor(_ConcreteType::name, &_ConcreteType::createInstance);
  };
};

#endif // ABSTRACT_FACTORY_HPP
