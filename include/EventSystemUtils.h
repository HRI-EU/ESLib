/*******************************************************************************

  Copyright (c) 2017, Honda Research Institute Europe GmbH.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef EVENTSYSTEMUTILS_H
#define EVENTSYSTEMUTILS_H

#include <functional>
#include <type_traits>
#include <cstring>
#include <string>

#if !defined (_MSC_VER)
#include <cxxabi.h>
using __cxxabiv1::__cxa_demangle;
#endif

namespace ES {

/**
 * @brief Returns the type argument unchanged.
 *
 * This struct has one member, type, which is exactly the input type T. In the realm of
 * template type transformations, this results in an identity transform.
 *
 * The concept is taken from the standard library. std::type_identity is added by c++20,
 * but the underlying system already works in C++11.
 *
 * While useless on the first glance, this type allows to block template argument deduction.
 * Usually, when a template parameter appears twice in the argument list, the deduction
 * algorithm tries to find a type that satisfies both uses. If the argument types are different,
 * the deduction fails, even if one argument value is implicitly convertible to the other
 * argument's type.
 * By replacing one parameter type with type_identity, the compiler will only use the other argument's
 * type for deduction.
 *
 * Our use case for this are member function pointers. If a function takes a pointer to an object T
 * and a pointer to a member of T, the default deduction will fail if the member is defined in a
 * parent of the passed object type. By replacing the object parameter's type with type_identity,
 * the compiler correctly uses the parent type for T.
 *
 * @tparam T input type
 */
template<typename T>
struct type_identity {
  /// @brief Defined to be T.
  typedef T type;
};
/**
 * @brief Short-notation alias for type_identity.
 *
 * Basically expands to T. See type_identity for a full description.
 */
template<typename T>
using type_identity_t = typename type_identity<T>::type;



// helpers to call a function with a tuple
namespace detail
{

// index_sequence is only available in C++14
// this code has been borrowed from pybind11
// https://github.com/pybind/pybind11/blob/bdbe8d0bde6aeb5360007cc57a453c18a33ec17b/include/pybind11/detail/common.h#L459-L462
template<size_t ...> struct index_sequence  { };
/** @cond */
template<size_t N, size_t ...S> struct make_index_sequence_impl : make_index_sequence_impl <N - 1, N - 1, S...> { };
/** @endcond */
template<size_t ...S> struct make_index_sequence_impl <0, S...>
{
  typedef index_sequence<S...> type;
};
template<size_t N> using make_index_sequence = typename make_index_sequence_impl<N>::type;

// apply is only available in C++17
// this is a stripped-down version limited to std::function and void return types
// for explanation see https://en.cppreference.com/w/cpp/utility/apply
template <class F, class Tuple, std::size_t... I>
void apply_impl(const std::function<F>& f, Tuple&& t, index_sequence<I...>)
{
  f(std::get<I>(std::forward<Tuple>(t))...);
}
template <class F, class Tuple>
void apply(const std::function<F>& f, Tuple&& t)
{
  apply_impl(f, std::forward<Tuple>(t),
             make_index_sequence<std::tuple_size<typename std::remove_reference<Tuple>::type>::value> {});
}


// getting a a type name from a template argument
// returns std::string to deal with demangling (which allocates memory)
template <typename T>
std::string getTypeName(void)
{
  const char* name = typeid(T).name();

#if !defined (_MSC_VER)
  // demangle name on linux
  char* demangled = __cxa_demangle(name, NULL, 0, NULL);
  std::string demStr = demangled;
  free(demangled);

  return demStr;
#else
  return name;
#endif
}


// std::bind with a variable argument count. For explanation see
// https://stackoverflow.com/questions/21192659/variadic-templates-and-stdbind?lq=1

template<int> // begin with 0 here!
struct placeholder_template
{};

}  // namespace detail

}  // namespace ES
// MUST BE TOP LEVEL NAMESPACE
namespace std
{
template<int N>
struct is_placeholder< ES::detail::placeholder_template<N> >
  : integral_constant<int, N+1> // the one is important
{};
}

namespace ES {

namespace detail {

// helper that is aware of the indices
template<typename Func, typename T, std::size_t... I>
inline auto bind_owner_helper(Func f, T* obj, index_sequence<I...>)
  -> decltype(std::bind(f, obj, placeholder_template<I>()...))
{
  return std::bind(f, obj, placeholder_template<I>()...);
}
// this helper function binds a member function to it's owner object
template<typename T, typename Return, typename...Args>
inline auto bind_owner(Return(T::*fp)(Args...), type_identity_t<T>* obj)
  -> decltype(bind_owner_helper(fp, obj, make_index_sequence<sizeof...(Args)>{}))
{
  return bind_owner_helper(fp, obj, make_index_sequence<sizeof...(Args)>{});
}
template<typename T, typename Return, typename...Args>
inline auto bind_owner(Return(T::*fp)(Args...)const, const type_identity_t<T>* obj)
-> decltype(bind_owner_helper(fp, obj, make_index_sequence<sizeof...(Args)>{}))
{
  return bind_owner_helper(fp, obj, make_index_sequence<sizeof...(Args)>{});
}

// To support lambdas in subscribe, we need to deduce the function arguments.
// How do we do that? ask pybind11.

// grab these shortcuts for C++11 - technically from pybind11, but trivial
template <bool B, typename T, typename F> using conditional_t = typename std::conditional<B, T, F>::type;
template <typename T> using remove_reference_t = typename std::remove_reference<T>::type;

/// Strip the class from a method type
// https://github.com/pybind/pybind11/blob/435dbdd114d135712a8a3b68eb9e640756ffe73b/include/pybind11/detail/common.h#L501
template <typename T> struct remove_class { };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...)> { typedef R type(A...); };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...) const> { typedef R type(A...); };


// extracts the function signature from a function object
// https://github.com/pybind/pybind11/blob/435dbdd114d135712a8a3b68eb9e640756ffe73b/include/pybind11/detail/common.h#L621
template <typename F> struct strip_function_object {
    using type = typename remove_class<decltype(&F::operator())>::type;
};

// Extracts the function signature from a function, function pointer or lambda.
// https://github.com/pybind/pybind11/blob/435dbdd114d135712a8a3b68eb9e640756ffe73b/include/pybind11/detail/common.h#L627
template <typename Function, typename F = remove_reference_t<Function>>
using function_signature_t = conditional_t<
    std::is_function<F>::value,
    F,
    typename conditional_t<
        std::is_pointer<F>::value || std::is_member_pointer<F>::value,
        std::remove_pointer<F>,
        strip_function_object<F>
    >::type
>;


// Turns out using a non-void returning function in a std::function<void(...)> is
// not generally supported, and actually forbidden in c++14.
// This wrapper functor forwards all arguments to the function, but discards the return value.
template<typename Func, typename...Args>
struct ignore_return_value_wrapper
{
private:
  Func func;
public:
  ignore_return_value_wrapper(Func func): func(func) {}
  void operator()(Args...args) {
    func(args...);
  }
};

}  // namespace detail

}  // namespace ES

#endif /* EVENTSYSTEM_UTILS_H */
