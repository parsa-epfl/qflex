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
#ifndef FLEXUS_QEMU_TRAMPOLINE_HPP_INCLUDED
#define FLEXUS_QEMU_TRAMPOLINE_HPP_INCLUDED
#include <core/qemu/api_wrappers.hpp>

namespace Flexus {
namespace Qemu {

template <class CppObjectClass>
struct QemuObject {
  API::conf_object_t conf_object;
  CppObjectClass theObject;
};

namespace Detail {

//These templates determing the signature of a function given a Typedef for it
template<typename Signature>
struct function_ptr_traits;

template<typename R>
struct function_ptr_traits<R ( *)()> {
  static const int arity = 0;
  typedef R result_type;
  typedef void arg1_type;
  typedef void arg2_type;
  typedef R (*free_fn_ptr)();
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)();
  };
};

template<typename R, typename T1>
struct function_ptr_traits<R ( *)(T1)> {
  static const int arity = 1;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef void arg2_type;
  typedef R (*free_fn_ptr)(T1);
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)();
  };
};

template<typename R, typename T1, typename T2>
struct function_ptr_traits<R ( *)(T1, T2)> {
  static const int arity = 2;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef T2 arg2_type;
  typedef R (*free_fn_ptr)(T1, T2);
  template <class T>
  struct member {
    typedef R (T::* member_fn_ptr)(T2);
  };
};

template<typename R, typename T1, typename T2, typename T3>
struct function_ptr_traits<R ( *)(T1, T2, T3)> {
  static const int arity = 3;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef T2 arg2_type;
  typedef T3 arg3_type;
  typedef R (*free_fn_ptr)(T1, T2, T3);
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)(T2, T3);
  };
};

template<typename R, typename T1, typename T2, typename T3, typename T4>
struct function_ptr_traits<R ( *)(T1, T2, T3, T4)> {
  static const int arity = 4;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef T2 arg2_type;
  typedef T3 arg3_type;
  typedef T4 arg4_type;
  typedef R (*free_fn_ptr)(T1, T2, T3, T4);
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)(T2, T3, T4);
  };
};

template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5>
struct function_ptr_traits<R ( *)(T1, T2, T3, T4, T5)> {
  static const int arity = 5;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef T2 arg2_type;
  typedef T3 arg3_type;
  typedef T4 arg4_type;
  typedef T5 arg5_type;
  typedef R (*free_fn_ptr)(T1, T2, T3, T4, T5);
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)(T2, T3, T4, T5);
  };
};

template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
struct function_ptr_traits<R ( *)(T1, T2, T3, T4, T5, T6, T7, T8, T9)> {
  static const int arity = 9;
  typedef R result_type;
  typedef T1 arg1_type;
  typedef T2 arg2_type;
  typedef T3 arg3_type;
  typedef T4 arg4_type;
  typedef T5 arg5_type;
  typedef T6 arg6_type;
  typedef T7 arg7_type;
  typedef T8 arg8_type;
  typedef T9 arg9_type;
  typedef R (*free_fn_ptr)(T1, T2, T3, T4, T5, T6, T7, T8, T9);
  template <class T>
  struct member {
    typedef R (T::*member_fn_ptr)(T2, T3, T4, T5, T6, T7, T8, T9);
  };
};

//The select_arity template chooses an implementation of the trampoline function based on the
//arity of the target signature
template <int Arity>
struct select_arity;

template <>
struct select_arity<0> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn >
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline() {
      return FreeFn ();
    }
  };
};

template <>
struct select_arity<1> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn >
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1) {
      return FreeFn (arg1);
    }
  };
  template <class TargetSignature, class T, typename TargetSignature::template member<T>::member_fn_ptr MemberFn>
  struct MemberFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1) {
      return (static_cast<T *>(arg1)->*MemberFn)();
    }
  };
  template <class TargetSignature, class T, class TImpl, typename TargetSignature::template member<TImpl>::member_fn_ptr MemberFn>
  struct AddInFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1) {
      return ( &( (reinterpret_cast< QemuObject<T> * > (arg1) )->theObject) ->* MemberFn)();
    }
  };

};

template <>
struct select_arity<2> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn>
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2) {
      return FreeFn(arg1, arg2);
    }
  };
  template <class TargetSignature, class T, typename TargetSignature::template member<T>::member_fn_ptr MemberFn>
  struct MemberFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2) {
      return (static_cast<T *>(arg1)->*MemberFn)(arg2);
    }
  };

  template <class TargetSignature, class T, class TImpl, typename TargetSignature::template member<TImpl>::member_fn_ptr MemberFn>
  struct AddInFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2) {
      return ( &( (reinterpret_cast< QemuObject<T> * > (arg1) )->theObject) ->* MemberFn)(arg2);
    }
  };
};

template <>
struct select_arity<3> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn>
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3) {
      return FreeFn(arg1, arg2, arg3);
    }
  };
  template <class TargetSignature, class T, typename TargetSignature::template member<T>::member_fn_ptr MemberFn>
  struct MemberFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3) {
      return (static_cast<T *>(arg1)->*MemberFn)(arg2, arg3);
    }
  };

  template <class TargetSignature, class T, class TImpl, typename TargetSignature::template member<TImpl>::member_fn_ptr MemberFn>
  struct AddInFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3) {
      return ( &( (reinterpret_cast< QemuObject<T> * > (arg1) )->theObject) ->* MemberFn)(arg2, arg3);
    }
  };

};

template <>
struct select_arity<4> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn>
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3, typename TargetSignature::arg4_type arg4) {
      return FreeFn(arg1, arg2, arg3, arg4);
    }
  };

  template <class TargetSignature, class T, typename TargetSignature::template member<T>::member_fn_ptr MemberFn>
  struct MemberFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3, typename TargetSignature::arg4_type arg4) {
      return (static_cast<T *>(arg1)->*MemberFn)(arg2, arg3, arg4);
    }
  };

  template <class TargetSignature, class T, class TImpl, typename TargetSignature::template member<TImpl>::member_fn_ptr MemberFn>
  struct AddInFnImpl {
    static typename TargetSignature::result_type trampoline(typename TargetSignature::arg1_type arg1, typename TargetSignature::arg2_type arg2, typename TargetSignature::arg3_type arg3, typename TargetSignature::arg4_type arg4) {
      return ( &( (reinterpret_cast< QemuObject<T> * > (arg1) )->theObject) ->* MemberFn)(arg2, arg3, arg4);
    }
  };
};

template <>
struct select_arity<9> {
  template <class TargetSignature, typename TargetSignature::free_fn_ptr FreeFn>
  struct FreeFnImpl {
    static typename TargetSignature::result_type trampoline
    ( typename TargetSignature::arg1_type arg1
      , typename TargetSignature::arg2_type arg2
      , typename TargetSignature::arg3_type arg3
      , typename TargetSignature::arg4_type arg4
      , typename TargetSignature::arg5_type arg5
      , typename TargetSignature::arg6_type arg6
      , typename TargetSignature::arg7_type arg7
      , typename TargetSignature::arg8_type arg8
      , typename TargetSignature::arg9_type arg9
    ) {
      return FreeFn(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    }
  };

  template <class TargetSignature, class T, typename TargetSignature::template member<T>::member_fn_ptr MemberFn>
  struct MemberFnImpl {
    static typename TargetSignature::result_type trampoline
    ( typename TargetSignature::arg1_type arg1
      , typename TargetSignature::arg2_type arg2
      , typename TargetSignature::arg3_type arg3
      , typename TargetSignature::arg4_type arg4
      , typename TargetSignature::arg5_type arg5
      , typename TargetSignature::arg6_type arg6
      , typename TargetSignature::arg7_type arg7
      , typename TargetSignature::arg8_type arg8
      , typename TargetSignature::arg9_type arg9
    ) {
      return (static_cast<T *>(arg1)->*MemberFn)(arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    }
  };

  template <class TargetSignature, class T, class TImpl, typename TargetSignature::template member<TImpl>::member_fn_ptr MemberFn>
  struct AddInFnImpl {
    static typename TargetSignature::result_type trampoline
    ( typename TargetSignature::arg1_type arg1
      , typename TargetSignature::arg2_type arg2
      , typename TargetSignature::arg3_type arg3
      , typename TargetSignature::arg4_type arg4
      , typename TargetSignature::arg5_type arg5
      , typename TargetSignature::arg6_type arg6
      , typename TargetSignature::arg7_type arg7
      , typename TargetSignature::arg8_type arg8
      , typename TargetSignature::arg9_type arg9
    ) {
      return ( &( (reinterpret_cast< QemuObject<T> * > (arg1) )->theObject) ->* MemberFn)(arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    }
  };
};

} //end Detail

template <typename Signature>
struct make_signature_from_free_fn {
private:
  typedef Detail::function_ptr_traits< Signature > signature_traits;
  typedef Detail::select_arity<signature_traits::arity> arity;
public:
  template <typename signature_traits::free_fn_ptr FreeFnPtr>
  struct with:
    public arity::template FreeFnImpl<signature_traits, FreeFnPtr> {};

};

template <typename Signature>
struct make_signature_from_addin_fn {
private:
  typedef Detail::function_ptr_traits< Signature > signature_traits;
  typedef Detail::select_arity<signature_traits::arity> arity;
public:

  template <class T, class TImpl, typename signature_traits::template member<TImpl>::member_fn_ptr MemberFnPtr>
  struct with:
    public arity::template AddInFnImpl<signature_traits, T, TImpl, MemberFnPtr> {};

};

template <typename Signature>
struct make_signature_from_addin_fn2 {
private:
  typedef Detail::function_ptr_traits< Signature > signature_traits;
  typedef Detail::select_arity<signature_traits::arity> arity;
public:

  template <class T, class TImpl, typename signature_traits::template member<TImpl>::member_fn_ptr MemberFnPtr>
  struct with:
    public arity::template AddInFnImpl<signature_traits, T, TImpl, MemberFnPtr> {};

};

template <typename Signature>
struct make_signature_from_member_fn {
private:
  typedef Detail::function_ptr_traits< Signature > signature_traits;
  typedef Detail::select_arity<signature_traits::arity> arity;
public:
  template <class T>
  struct for_class {
    template <typename signature_traits::template member<T>::member_fn_ptr MemberFnPtr>
    struct with :
      public arity::template MemberFnImpl<signature_traits, T, MemberFnPtr> {};
  };

};

}  //End Namespace Qemu
} //namespace Flexus

#endif //FLEXUS_QEMU_TRAMPOLINE_HPP_INCLUDED

