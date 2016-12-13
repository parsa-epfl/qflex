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
#ifndef FLEXUS_CONFIGURATION_HPP_INCLUDED
#define FLEXUS_CONFIGURATION_HPP_INCLUDED

#include <sstream>
#include <type_traits>

#include <boost/utility.hpp>
#include <core/boost_extensions/lexical_cast.hpp>

#include <core/debug/debug.hpp>

#include <core/metaprogram.hpp>
#include <core/exception.hpp>

namespace Flexus {
namespace Core {

using std::string;
using std::is_integral;
using std::is_same;

using boost::lexical_cast;
using boost::bad_lexical_cast;

namespace aux_ {
//These classes must be complete before ConfigurationManager can be defined, hence,
//they appear before ConfigurationManager

//Base class for Dynamic Parameters
struct ParameterBase {
  //The Parameter definition object describing the command line options
  std::string theName;
  std::string theDescription;
  std::string theSwitch;
  const std::string & theConfig;

  //Non-copyable
  ParameterBase(const ParameterBase&) = delete;
  ParameterBase& operator=(const ParameterBase&) = delete;

  //Constructor taking a ParameterDefinition object.  Defined after ConfigurationManager
  //as it requires the declaration of theConfigurationManager.
  ParameterBase(const std::string & aName, const std::string & aDescr, const std::string & aSwitch, const std::string & aConfig);

  //This setValue() method is overridden in the derived DynamicParameter classes
  //This is the key to getting dynamic parameters to work.  When this is invoked,
  //the derived class converts the string stored in lexical_value into the appropriate
  //type for the parameter
  virtual void setValue(std::string aValue) = 0;
  virtual bool isOverridden() = 0;
  virtual std::string lexicalValue() = 0;

  virtual ~ParameterBase() {}
};

} //End aux_

struct ConfigurationManager{
  //Sets up a configuration based on anArgc and anArgv
  virtual void processCommandLineConfiguration(int32_t anArgc, char ** anArgv) = 0;

  //Print out the command line options
  virtual void printParams() = 0;
  virtual void checkAllOverrides() = 0;

  //Print out the command line options
  virtual void printConfiguration(std::ostream &) = 0;
  virtual void parseConfiguration(std::istream & anIstream) = 0;
  virtual void set(std::string const & aName, std::string const & aValue) = 0;

  //Adds a DynamicParameter to theParameters
  virtual void registerParameter(aux_::ParameterBase & aParam) = 0;

  static ConfigurationManager & getConfigurationManager();
  // added by PLotfi
  static std::string getParameterValue(std::string const & aName, bool exact = true);
  // end PLotfi

  //Non-copyable
  ConfigurationManager(const ConfigurationManager&) = delete;
  ConfigurationManager& operator=(const ConfigurationManager&) = delete;
protected:
  //Constructible by derived classes only
  ConfigurationManager() {}
  //Suppress warning
  virtual ~ConfigurationManager() {}
  // added by PLotfi
  virtual void determine(std::string const & aName, bool exact) = 0;
  // end PLotfi
};

namespace aux_ {

#if 0
template <class ParamStruct, class Type, Type ParamStruct:: * Member>
struct FLEXUS_CONFIGURATION_WARNING {
  FLEXUS_CONFIGURATION_WARNING() {
    int32_t WARNING__FLEXUS_PARAMETER_VALUE_IS_NOT_SPECIFIED_IN_WIRING;
  }
};

template <class ParamStruct, class Type, Type ParamStruct:: * Member>
struct FLEXUS_CONFIGURATION_ERROR {
  struct ERROR;

  ERROR ERROR__FLEXUS_PARAMETER_VALUE_IS_NOT_SPECIFIED_IN_WIRING;
};

template < class ParamStruct, class Type, Type ParamStruct::* Member>
struct CompileValue {
  std::string theValue;
  CompileValue(const std::string & aValue)
    : theValue(aValue) {
  }
  CompileValue(const std::string & aValue, bool /*automatic*/)
    : theValue(aValue) {
#ifndef FLEXUS__NO_DEFAULT_PARAMETER_WARNING
    //Force warning
    FLEXUS_CONFIGURATION_WARNING<ParamStruct, Type, Member> warning;
#endif //FLEXUS__NO_DEFAULT_PARAMETER_WARNING

#ifdef FLEXUS__DISALLOW_DEFAULT_PARAMETER_VALUES
    FLEXUS_CONFIGURATION_ERROR<ParamStruct, Type, Member> error;
    (void)error;
#endif //FLEXUS__DISALLOW_DEFAULT_PARAMETER_VALUES
  }

};
#endif

//Dynamic Parameter Implementation
//******************************
//Template for defininig dynamic parameters
template < class ParamStruct, class ParamTag >
struct DynamicParameter : public ParameterBase {
  ParamStruct & theConfig;
  bool theOverridden;

  typedef typename ParamStruct::template get<ParamTag> param_def;

  //Default Constructor initializes to default value
  DynamicParameter(ParamStruct & aConfig)
    : ParameterBase(param_def::name(), param_def::description(), param_def::cmd_line_switch(), aConfig.theConfigName_)
    , theConfig(aConfig)
    , theOverridden(false) {
    theConfig.*(param_def::member()) = param_def::defaultValue();
  }

  void initialize(typename param_def::type const & aVal) {
    theConfig.*(param_def::member()) = aVal;
    theOverridden = true;
  }

  //When setValue is called, we process the command line parameter
  //and fill in value
  virtual void setValue(std::string aValue) {
    try {
      theConfig.*(param_def::member()) = boost::lexical_cast<typename param_def::type>(aValue);
      theOverridden = true;
    } catch (bad_lexical_cast e) {
      DBG_(Crit, ( << "Bad Lexical Cast attempting to set dynamic parameter." << std::endl
                   << "WARNING: Unable to set parameter " << param_def::name() << " to "
                   << aValue << ": " << e.what() ));
    }
  }

  bool isOverridden() {
    return theOverridden;
  }

  template < typename T > T & toString(T & v) {
    return v;
  }
  template < bool > const char * toString(bool & v) {
    return (v ? "true" : "false");
  }

  std::string lexicalValue() {
    std::stringstream lex;
    lex << toString(theConfig.*(param_def::member()));
    return lex.str();
  }

};
}//End aux_

} //namespace Configuration
}//namespace Flexus

#endif //FLEXUS_CONFIGURATION_HPP_INCLUDED

