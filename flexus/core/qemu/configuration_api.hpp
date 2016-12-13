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
#ifndef FLEXUS_QEMU_CONFIGURATION_API_HPP_INCLUDED
#define FLEXUS_QEMU_CONFIGURATION_API_HPP_INCLUDED

#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include <core/debug/debug.hpp>
#include <core/qemu/trampoline.hpp>
#include <core/qemu/attribute_value.hpp>
#include <core/qemu/api_wrappers.hpp>

#include <type_traits>
#include <functional>

#include <core/boost_extensions/member_function_traits.hpp>

namespace Flexus {
namespace Qemu {
namespace aux_ {
//Stub function declarations which assist in making this code behave properly
//both with and without simics
API::conf_object_t * NewObject_stub(
		  API::conf_class_t * aClass
		, std::string const & aName
		);
} //namespace aux_

enum Persistence {
  Persistent = API::QEMU_Class_Kind_Vanilla,
  Session = API::QEMU_Class_Kind_Session,
  Transient = API::QEMU_Class_Kind_Pseudo
};

template <class ObjectClassImpl>
class BuiltInObject : public aux_::Object, public aux_::BuiltIn {
protected:
  ObjectClassImpl theImpl;
public:
  using implementation_class = ObjectClassImpl;

  BuiltInObject () : theImpl(0) {}
  explicit BuiltInObject (ObjectClassImpl & anImpl) : theImpl(anImpl) {}
  explicit BuiltInObject (API::conf_object_t * anObject) : theImpl(anObject) {}

  //This overloaded address-of operator is used internally by the
  //configuration-api implementation
  ObjectClassImpl * operator&() {
    return &theImpl;
  }

  //These forwarding functions make the wrapper class behave like a
  //pointer to the simics object
  ObjectClassImpl * operator->() {
    return &theImpl;
  }
  ObjectClassImpl & operator *() {
    return theImpl;
  }
  ObjectClassImpl const * operator->() const {
    return &theImpl;
  }
  ObjectClassImpl const  & operator *() const {
    return theImpl;
  }
  //operator bool const () const {
  //  return theImpl.isNull();
  //}

  //defineClass does nothing for built-in classes, since they are defined
  //by Qemu
  template <class Class>
  static void defineClass(Class & aClass) {}
};

template <class ObjectClassImpl>
class AddInObject /*: public aux_::Object */{
  ObjectClassImpl * theImpl;
public:
  using implementation_class = ObjectClassImpl;
  AddInObject() : theImpl(0) {}
  explicit AddInObject(API::conf_object_t * aQemuObject) : 
	  theImpl(new ObjectClassImpl(aQemuObject)) {}
  explicit AddInObject(ObjectClassImpl * anImpl) : theImpl(anImpl) {}

  //This overloaded address-of operator is used internally by the
  //configuration-api implementation
  ObjectClassImpl * operator&() {
    return theImpl;
  }

  //These forwarding functions make the wrapper class behave like a
  //pointer to the simics object
  ObjectClassImpl * operator->() {
    return theImpl;
  }
  ObjectClassImpl & operator *() {
    return *theImpl;
  }
  ObjectClassImpl const * operator->() const {
    return theImpl;
  }
  ObjectClassImpl const  & operator *() const {
    return *theImpl;
  }
  operator bool const () const {
    return theImpl;
  }

  //Unlike built-in objects, add-in objects must provide a destruct
  //which simics can call.  This deletes theImpl, which will invoke
  //the destructor of theImpl to do any cleanup that the implementation
  //object requires.
  void destruct() {
    delete theImpl;
  }

  //The default defineClass() implementation does nothing.  This is
  //overridden in order to add attributes or commands to a class.
  template <class Class>
  static void defineClass(Class & aClass) {}
};
namespace aux_ {
//This tag is used to choose between the "registering" and "non-registering"
//constructor for Addin classes
struct no_class_registration_tag {};

//trait template for determining of CppObjectClass represents a Qemu Builtin class
template <class CppObjectClass>
struct is_builtin {
  static const bool value = std::is_base_of<BuiltIn, CppObjectClass>::value;
};

//This class collects functionality common to both the Builtin and Addin
//implementations of Class.  Also, it allows us to store pointers to
//Class objects as BaseClassImpl *, so we don't have to know what the
//CppObjectClass template argument is.
class BaseClassImpl{
protected:
  API::conf_class_t * theQemuClass; //Qemu class data structure

public:
  //Default construction and destruction.
  API::conf_class_t /*const*/ * getQemuClass() const {
    return theQemuClass;
  }

  bool operator ==(const BaseClassImpl & aClass) {
    return theQemuClass == aClass.theQemuClass;
  }

  bool operator !=(const BaseClassImpl & aClass) {
    return !(*this == aClass);
  }

  //Non-copyable
  BaseClassImpl() = default;
  BaseClassImpl(const BaseClassImpl&) = delete;
  BaseClassImpl& operator=(const BaseClassImpl&) = delete;
};

//Implementation of the Class type for Qemu classes
template < class CppObjectClass, bool IsBuiltin /*true*/ >
class ClassImpl : public BaseClassImpl {
protected:
  //Class objects for built in classes, never have to register the class, since Qemu
  //already knows about the class.  Thus, the "register" and "no-register" versions
  //of the constructor are the same.
  ClassImpl(no_class_registration_tag const & = no_class_registration_tag()) {
	  theQemuClass = 0;
  }
public:
  //In order to cast a conf_object_t * to the corresponding C++ object, we simply
  //create a CppObjectClass to wrap the conf_object_t * and return it.  CppObjectClass
  //should behave like a pointer.  Note that CppObjectClass never owns the storage
  //pointed to by the conf_object_t * for builtin types.
  static CppObjectClass cast_object(API::conf_object_t * aQemuObject) {
    return CppObjectClass(aQemuObject);
  }
};

using YesType = char;
struct NoType {
  char a[2];
} ;

template <class MemberFunction, int Arity>
struct member_function_arity {
  static const bool value = (boost::member_function_traits<MemberFunction>::arity == Arity);
};

//Implementation of the Class type for Addin classes
template <class CppObjectClass>
class ClassImpl < CppObjectClass, false /* ! builtin */ > : public BaseClassImpl {

  //Note: the ordering of the elements in this struct is significant. conf_object
  //must come first, and this struct must be a POD, to guarantee that the
  //reinterpret_casts below are legal.
  using Qemu_object = QemuObject<CppObjectClass>;

public:

  //Note: the reintepret_casts in these methods are safe since QemuObject is POD and
  //conf_object comes first in QemuObject.

  // This function is the constructor Qemu will call to create objects of the type 
  // this class represents.
  static API::conf_object_t * constructor(void *aThing) {
    // Note: the reintepret cast here is safe since QemuObject is POD and
    // conf_object comes first in QemuObject.
	  return 0;
  }

  // The corresponding destructor
  static int destructor(API::conf_object_t * anInstance) {
	  delete anInstance;
	  return 0;
  }

  // Cast a bare conf_object_t * to the C++ object type
  static CppObjectClass cast_object(API::conf_object_t * aQemuObject) {
	return CppObjectClass(aQemuObject);
  }

protected:
  // The no-argument constructor registers the Addin class represented by this 
  // object with Qemu.
  ClassImpl() {
	// this call does nothing
    theQemuClass = new API::conf_class_t;
    CppObjectClass::defineClass(*this);
  }

  // This constructor does not register the class.  Used when the class is already 
  // registered, by some of the engine of attribute_cast<>
  ClassImpl(no_class_registration_tag const &) {
	  theQemuClass = 0;
  }

};
}//End aux_

template <class CppObjectClass>
struct Class : public aux_::ClassImpl<
						  CppObjectClass
						, aux_::is_builtin<CppObjectClass>::value 
						> 
{
  using base = aux_::ClassImpl<
	    CppObjectClass
	  , aux_::is_builtin<CppObjectClass>::value 
	  >;
  using object_type = CppObjectClass;

  //Forwarding constructors
  Class() {}
  Class(aux_::no_class_registration_tag const & aTag ) : base( aTag ) {}
};

template <class CppObjectClass >
class Factory {
  using class_ = Class<CppObjectClass>;
  class_ * theClass;
public:
  Factory() {
    static class_ theStaticClass;
    theClass = &theStaticClass;
  }
  typename class_::object_type create(std::string aQemuName) {
    API::conf_object_t * object = aux_::NewObject_stub(
              const_cast<API::conf_class_t *>
              ( theClass->getQemuClass() )
              , aQemuName
            );

    if (!object) {
      throw QemuException(std::string("An exception occured while attempting to"
				  " create object ") + aQemuName);
    }
    //Ask the QemuClass to walk the magic data structure to get to the C++ object
    return theClass->cast_object(object);
  }

  API::conf_class_t * getQemuClass() const {
    return theClass->getQemuClass();
  }

  class_ & getClass() {
    return *theClass;
  }
};

}  //End Namespace Qemu
} //namespace Flexus

#endif //FLEXUS_QEMU_CONFIGURATION_API_HPP_INCLUDED
