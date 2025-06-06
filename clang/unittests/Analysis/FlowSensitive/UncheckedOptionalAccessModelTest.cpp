//===- UncheckedOptionalAccessModelTest.cpp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// FIXME: Move this to clang/unittests/Analysis/FlowSensitive/Models.

#include "clang/Analysis/FlowSensitive/Models/UncheckedOptionalAccessModel.h"
#include "TestingSupport.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/TextDiagnostic.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace clang;
using namespace dataflow;
using namespace test;

using ::testing::ContainerEq;

// FIXME: Move header definitions in separate file(s).
static constexpr char CSDtdDefHeader[] = R"(
#ifndef CSTDDEF_H
#define CSTDDEF_H

namespace std {

typedef decltype(sizeof(char)) size_t;

using nullptr_t = decltype(nullptr);

} // namespace std

#endif // CSTDDEF_H
)";

static constexpr char StdTypeTraitsHeader[] = R"(
#ifndef STD_TYPE_TRAITS_H
#define STD_TYPE_TRAITS_H

#include "cstddef.h"

namespace std {

template <typename T, T V>
struct integral_constant {
  static constexpr T value = V;
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template< class T > struct remove_reference      {typedef T type;};
template< class T > struct remove_reference<T&>  {typedef T type;};
template< class T > struct remove_reference<T&&> {typedef T type;};

template <class T>
  using remove_reference_t = typename remove_reference<T>::type;

template <class T>
struct remove_extent {
  typedef T type;
};

template <class T>
struct remove_extent<T[]> {
  typedef T type;
};

template <class T, size_t N>
struct remove_extent<T[N]> {
  typedef T type;
};

template <class T>
struct is_array : false_type {};

template <class T>
struct is_array<T[]> : true_type {};

template <class T, size_t N>
struct is_array<T[N]> : true_type {};

template <class>
struct is_function : false_type {};

template <class Ret, class... Args>
struct is_function<Ret(Args...)> : true_type {};

namespace detail {

template <class T>
struct type_identity {
  using type = T;
};  // or use type_identity (since C++20)

template <class T>
auto try_add_pointer(int) -> type_identity<typename remove_reference<T>::type*>;
template <class T>
auto try_add_pointer(...) -> type_identity<T>;

}  // namespace detail

template <class T>
struct add_pointer : decltype(detail::try_add_pointer<T>(0)) {};

template <bool B, class T, class F>
struct conditional {
  typedef T type;
};

template <class T, class F>
struct conditional<false, T, F> {
  typedef F type;
};

template <class T>
struct remove_cv {
  typedef T type;
};
template <class T>
struct remove_cv<const T> {
  typedef T type;
};
template <class T>
struct remove_cv<volatile T> {
  typedef T type;
};
template <class T>
struct remove_cv<const volatile T> {
  typedef T type;
};

template <class T>
using remove_cv_t = typename remove_cv<T>::type;

template <class T>
struct decay {
 private:
  typedef typename remove_reference<T>::type U;

 public:
  typedef typename conditional<
      is_array<U>::value, typename remove_extent<U>::type*,
      typename conditional<is_function<U>::value, typename add_pointer<U>::type,
                           typename remove_cv<U>::type>::type>::type type;
};

template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
  typedef T type;
};

template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <class T, class U>
struct is_same : false_type {};

template <class T>
struct is_same<T, T> : true_type {};

template <class T>
struct is_void : is_same<void, typename remove_cv<T>::type> {};

namespace detail {

template <class T>
auto try_add_lvalue_reference(int) -> type_identity<T&>;
template <class T>
auto try_add_lvalue_reference(...) -> type_identity<T>;

template <class T>
auto try_add_rvalue_reference(int) -> type_identity<T&&>;
template <class T>
auto try_add_rvalue_reference(...) -> type_identity<T>;

}  // namespace detail

template <class T>
struct add_lvalue_reference : decltype(detail::try_add_lvalue_reference<T>(0)) {
};

template <class T>
struct add_rvalue_reference : decltype(detail::try_add_rvalue_reference<T>(0)) {
};

template <class T>
typename add_rvalue_reference<T>::type declval() noexcept;

namespace detail {

template <class T>
auto test_returnable(int)
    -> decltype(void(static_cast<T (*)()>(nullptr)), true_type{});
template <class>
auto test_returnable(...) -> false_type;

template <class From, class To>
auto test_implicitly_convertible(int)
    -> decltype(void(declval<void (&)(To)>()(declval<From>())), true_type{});
template <class, class>
auto test_implicitly_convertible(...) -> false_type;

}  // namespace detail

template <class From, class To>
struct is_convertible
    : integral_constant<bool,
                        (decltype(detail::test_returnable<To>(0))::value &&
                         decltype(detail::test_implicitly_convertible<From, To>(
                             0))::value) ||
                            (is_void<From>::value && is_void<To>::value)> {};

template <class From, class To>
inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

template <class...>
using void_t = void;

template <class, class T, class... Args>
struct is_constructible_ : false_type {};

template <class T, class... Args>
struct is_constructible_<void_t<decltype(T(declval<Args>()...))>, T, Args...>
    : true_type {};

template <class T, class... Args>
using is_constructible = is_constructible_<void_t<>, T, Args...>;

template <class T, class... Args>
inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template <class _Tp>
struct __uncvref {
  typedef typename remove_cv<typename remove_reference<_Tp>::type>::type type;
};

template <class _Tp>
using __uncvref_t = typename __uncvref<_Tp>::type;

template <bool _Val>
using _BoolConstant = integral_constant<bool, _Val>;

template <class _Tp, class _Up>
using _IsSame = _BoolConstant<__is_same(_Tp, _Up)>;

template <class _Tp, class _Up>
using _IsNotSame = _BoolConstant<!__is_same(_Tp, _Up)>;

template <bool>
struct _MetaBase;
template <>
struct _MetaBase<true> {
  template <class _Tp, class _Up>
  using _SelectImpl = _Tp;
  template <template <class...> class _FirstFn, template <class...> class,
            class... _Args>
  using _SelectApplyImpl = _FirstFn<_Args...>;
  template <class _First, class...>
  using _FirstImpl = _First;
  template <class, class _Second, class...>
  using _SecondImpl = _Second;
  template <class _Result, class _First, class... _Rest>
  using _OrImpl =
      typename _MetaBase<_First::value != true && sizeof...(_Rest) != 0>::
          template _OrImpl<_First, _Rest...>;
};

template <>
struct _MetaBase<false> {
  template <class _Tp, class _Up>
  using _SelectImpl = _Up;
  template <template <class...> class, template <class...> class _SecondFn,
            class... _Args>
  using _SelectApplyImpl = _SecondFn<_Args...>;
  template <class _Result, class...>
  using _OrImpl = _Result;
};

template <bool _Cond, class _IfRes, class _ElseRes>
using _If = typename _MetaBase<_Cond>::template _SelectImpl<_IfRes, _ElseRes>;

template <class... _Rest>
using _Or = typename _MetaBase<sizeof...(_Rest) !=
                               0>::template _OrImpl<false_type, _Rest...>;

template <bool _Bp, class _Tp = void>
using __enable_if_t = typename enable_if<_Bp, _Tp>::type;

template <class...>
using __expand_to_true = true_type;
template <class... _Pred>
__expand_to_true<__enable_if_t<_Pred::value>...> __and_helper(int);
template <class...>
false_type __and_helper(...);
template <class... _Pred>
using _And = decltype(__and_helper<_Pred...>(0));

template <class _Pred>
struct _Not : _BoolConstant<!_Pred::value> {};

struct __check_tuple_constructor_fail {
  static constexpr bool __enable_explicit_default() { return false; }
  static constexpr bool __enable_implicit_default() { return false; }
  template <class...>
  static constexpr bool __enable_explicit() {
    return false;
  }
  template <class...>
  static constexpr bool __enable_implicit() {
    return false;
  }
};

template <typename, typename _Tp>
struct __select_2nd {
  typedef _Tp type;
};
template <class _Tp, class _Arg>
typename __select_2nd<decltype((declval<_Tp>() = declval<_Arg>())),
                      true_type>::type
__is_assignable_test(int);
template <class, class>
false_type __is_assignable_test(...);
template <class _Tp, class _Arg,
          bool = is_void<_Tp>::value || is_void<_Arg>::value>
struct __is_assignable_imp
    : public decltype((__is_assignable_test<_Tp, _Arg>(0))) {};
template <class _Tp, class _Arg>
struct __is_assignable_imp<_Tp, _Arg, true> : public false_type {};
template <class _Tp, class _Arg>
struct is_assignable : public __is_assignable_imp<_Tp, _Arg> {};

template <class _Tp>
struct __libcpp_is_integral : public false_type {};
template <>
struct __libcpp_is_integral<bool> : public true_type {};
template <>
struct __libcpp_is_integral<char> : public true_type {};
template <>
struct __libcpp_is_integral<signed char> : public true_type {};
template <>
struct __libcpp_is_integral<unsigned char> : public true_type {};
template <>
struct __libcpp_is_integral<wchar_t> : public true_type {};
template <>
struct __libcpp_is_integral<short> : public true_type {};  // NOLINT
template <>
struct __libcpp_is_integral<unsigned short> : public true_type {};  // NOLINT
template <>
struct __libcpp_is_integral<int> : public true_type {};
template <>
struct __libcpp_is_integral<unsigned int> : public true_type {};
template <>
struct __libcpp_is_integral<long> : public true_type {};  // NOLINT
template <>
struct __libcpp_is_integral<unsigned long> : public true_type {};  // NOLINT
template <>
struct __libcpp_is_integral<long long> : public true_type {};  // NOLINT
template <>                                                    // NOLINTNEXTLINE
struct __libcpp_is_integral<unsigned long long> : public true_type {};
template <class _Tp>
struct is_integral
    : public __libcpp_is_integral<typename remove_cv<_Tp>::type> {};

template <class _Tp>
struct __libcpp_is_floating_point : public false_type {};
template <>
struct __libcpp_is_floating_point<float> : public true_type {};
template <>
struct __libcpp_is_floating_point<double> : public true_type {};
template <>
struct __libcpp_is_floating_point<long double> : public true_type {};
template <class _Tp>
struct is_floating_point
    : public __libcpp_is_floating_point<typename remove_cv<_Tp>::type> {};

template <class _Tp>
struct is_arithmetic
    : public integral_constant<bool, is_integral<_Tp>::value ||
                                         is_floating_point<_Tp>::value> {};

template <class _Tp>
struct __libcpp_is_pointer : public false_type {};
template <class _Tp>
struct __libcpp_is_pointer<_Tp*> : public true_type {};
template <class _Tp>
struct is_pointer : public __libcpp_is_pointer<typename remove_cv<_Tp>::type> {
};

template <class _Tp>
struct __libcpp_is_member_pointer : public false_type {};
template <class _Tp, class _Up>
struct __libcpp_is_member_pointer<_Tp _Up::*> : public true_type {};
template <class _Tp>
struct is_member_pointer
    : public __libcpp_is_member_pointer<typename remove_cv<_Tp>::type> {};

template <class _Tp>
struct __libcpp_union : public false_type {};
template <class _Tp>
struct is_union : public __libcpp_union<typename remove_cv<_Tp>::type> {};

template <class T>
struct is_reference : false_type {};
template <class T>
struct is_reference<T&> : true_type {};
template <class T>
struct is_reference<T&&> : true_type {};

template <class T>
inline constexpr bool is_reference_v = is_reference<T>::value;

struct __two {
  char __lx[2];
};

namespace __is_class_imp {
template <class _Tp>
char __test(int _Tp::*);
template <class _Tp>
__two __test(...);
}  // namespace __is_class_imp
template <class _Tp>
struct is_class
    : public integral_constant<bool,
                               sizeof(__is_class_imp::__test<_Tp>(0)) == 1 &&
                                   !is_union<_Tp>::value> {};

template <class _Tp>
struct __is_nullptr_t_impl : public false_type {};
template <>
struct __is_nullptr_t_impl<nullptr_t> : public true_type {};
template <class _Tp>
struct __is_nullptr_t
    : public __is_nullptr_t_impl<typename remove_cv<_Tp>::type> {};
template <class _Tp>
struct is_null_pointer
    : public __is_nullptr_t_impl<typename remove_cv<_Tp>::type> {};

template <class _Tp>
struct is_enum
    : public integral_constant<
          bool, !is_void<_Tp>::value && !is_integral<_Tp>::value &&
                    !is_floating_point<_Tp>::value && !is_array<_Tp>::value &&
                    !is_pointer<_Tp>::value && !is_reference<_Tp>::value &&
                    !is_member_pointer<_Tp>::value && !is_union<_Tp>::value &&
                    !is_class<_Tp>::value && !is_function<_Tp>::value> {};

template <class _Tp>
struct is_scalar
    : public integral_constant<
          bool, is_arithmetic<_Tp>::value || is_member_pointer<_Tp>::value ||
                    is_pointer<_Tp>::value || __is_nullptr_t<_Tp>::value ||
                    is_enum<_Tp>::value> {};
template <>
struct is_scalar<nullptr_t> : public true_type {};

} // namespace std

#endif // STD_TYPE_TRAITS_H
)";

static constexpr char AbslTypeTraitsHeader[] = R"(
#ifndef ABSL_TYPE_TRAITS_H
#define ABSL_TYPE_TRAITS_H

#include "std_type_traits.h"

namespace absl {

template <typename... Ts>
struct conjunction : std::true_type {};

template <typename T, typename... Ts>
struct conjunction<T, Ts...>
    : std::conditional<T::value, conjunction<Ts...>, T>::type {};

template <typename T>
struct conjunction<T> : T {};

template <typename T>
struct negation : std::integral_constant<bool, !T::value> {};

template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

} // namespace absl

#endif // ABSL_TYPE_TRAITS_H
)";

static constexpr char StdStringHeader[] = R"(
#ifndef STRING_H
#define STRING_H

namespace std {

struct string {
  string(const char*);
  ~string();
  bool empty();
};
bool operator!=(const string &LHS, const char *RHS);

} // namespace std

#endif // STRING_H
)";

static constexpr char StdUtilityHeader[] = R"(
#ifndef UTILITY_H
#define UTILITY_H

#include "std_type_traits.h"

namespace std {

template <typename T>
constexpr remove_reference_t<T>&& move(T&& x);

template <typename T>
void swap(T& a, T& b) noexcept;

} // namespace std

#endif // UTILITY_H
)";

static constexpr char StdInitializerListHeader[] = R"(
#ifndef INITIALIZER_LIST_H
#define INITIALIZER_LIST_H

namespace std {

template <typename T>
class initializer_list {
 public:
  const T *a, *b;
  initializer_list() noexcept;
};

} // namespace std

#endif // INITIALIZER_LIST_H
)";

static constexpr char StdOptionalHeader[] = R"(
#include "std_initializer_list.h"
#include "std_type_traits.h"
#include "std_utility.h"

namespace std {

struct in_place_t {};
constexpr in_place_t in_place;

struct nullopt_t {
  constexpr explicit nullopt_t() {}
};
constexpr nullopt_t nullopt;

template <class _Tp>
struct __optional_destruct_base {
  constexpr void reset() noexcept;
};

template <class _Tp>
struct __optional_storage_base : __optional_destruct_base<_Tp> {
  constexpr bool has_value() const noexcept;
};

template <typename _Tp>
class optional : private __optional_storage_base<_Tp> {
  using __base = __optional_storage_base<_Tp>;

 public:
  using value_type = _Tp;

 private:
  struct _CheckOptionalArgsConstructor {
    template <class _Up>
    static constexpr bool __enable_implicit() {
      return is_constructible_v<_Tp, _Up&&> && is_convertible_v<_Up&&, _Tp>;
    }

    template <class _Up>
    static constexpr bool __enable_explicit() {
      return is_constructible_v<_Tp, _Up&&> && !is_convertible_v<_Up&&, _Tp>;
    }
  };
  template <class _Up>
  using _CheckOptionalArgsCtor =
      _If<_IsNotSame<__uncvref_t<_Up>, in_place_t>::value &&
              _IsNotSame<__uncvref_t<_Up>, optional>::value,
          _CheckOptionalArgsConstructor, __check_tuple_constructor_fail>;
  template <class _QualUp>
  struct _CheckOptionalLikeConstructor {
    template <class _Up, class _Opt = optional<_Up>>
    using __check_constructible_from_opt =
        _Or<is_constructible<_Tp, _Opt&>, is_constructible<_Tp, _Opt const&>,
            is_constructible<_Tp, _Opt&&>, is_constructible<_Tp, _Opt const&&>,
            is_convertible<_Opt&, _Tp>, is_convertible<_Opt const&, _Tp>,
            is_convertible<_Opt&&, _Tp>, is_convertible<_Opt const&&, _Tp>>;
    template <class _Up, class _QUp = _QualUp>
    static constexpr bool __enable_implicit() {
      return is_convertible<_QUp, _Tp>::value &&
             !__check_constructible_from_opt<_Up>::value;
    }
    template <class _Up, class _QUp = _QualUp>
    static constexpr bool __enable_explicit() {
      return !is_convertible<_QUp, _Tp>::value &&
             !__check_constructible_from_opt<_Up>::value;
    }
  };

  template <class _Up, class _QualUp>
  using _CheckOptionalLikeCtor =
      _If<_And<_IsNotSame<_Up, _Tp>, is_constructible<_Tp, _QualUp>>::value,
          _CheckOptionalLikeConstructor<_QualUp>,
          __check_tuple_constructor_fail>;


  template <class _Up, class _QualUp>
  using _CheckOptionalLikeAssign = _If<
      _And<
          _IsNotSame<_Up, _Tp>,
          is_constructible<_Tp, _QualUp>,
          is_assignable<_Tp&, _QualUp>
      >::value,
      _CheckOptionalLikeConstructor<_QualUp>,
      __check_tuple_constructor_fail
    >;

 public:
  constexpr optional() noexcept {}
  constexpr optional(const optional&) = default;
  constexpr optional(optional&&) = default;
  constexpr optional(nullopt_t) noexcept {}

  template <
      class _InPlaceT, class... _Args,
      class = enable_if_t<_And<_IsSame<_InPlaceT, in_place_t>,
                             is_constructible<value_type, _Args...>>::value>>
  constexpr explicit optional(_InPlaceT, _Args&&... __args);

  template <class _Up, class... _Args,
            class = enable_if_t<is_constructible_v<
                value_type, initializer_list<_Up>&, _Args...>>>
  constexpr explicit optional(in_place_t, initializer_list<_Up> __il,
                              _Args&&... __args);

  template <
      class _Up = value_type,
      enable_if_t<_CheckOptionalArgsCtor<_Up>::template __enable_implicit<_Up>(),
                int> = 0>
  constexpr optional(_Up&& __v);

  template <
      class _Up,
      enable_if_t<_CheckOptionalArgsCtor<_Up>::template __enable_explicit<_Up>(),
                int> = 0>
  constexpr explicit optional(_Up&& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeCtor<_Up, _Up const&>::
                                     template __enable_implicit<_Up>(),
                                 int> = 0>
  constexpr optional(const optional<_Up>& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeCtor<_Up, _Up const&>::
                                     template __enable_explicit<_Up>(),
                                 int> = 0>
  constexpr explicit optional(const optional<_Up>& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeCtor<_Up, _Up&&>::
                                     template __enable_implicit<_Up>(),
                                 int> = 0>
  constexpr optional(optional<_Up>&& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeCtor<_Up, _Up&&>::
                                     template __enable_explicit<_Up>(),
                                 int> = 0>
  constexpr explicit optional(optional<_Up>&& __v);

  constexpr optional& operator=(nullopt_t) noexcept;

  optional& operator=(const optional&);

  optional& operator=(optional&&);

  template <class _Up = value_type,
            class = enable_if_t<_And<_IsNotSame<__uncvref_t<_Up>, optional>,
                                   _Or<_IsNotSame<__uncvref_t<_Up>, value_type>,
                                       _Not<is_scalar<value_type>>>,
                                   is_constructible<value_type, _Up>,
                                   is_assignable<value_type&, _Up>>::value>>
  constexpr optional& operator=(_Up&& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeAssign<_Up, _Up const&>::
                                     template __enable_assign<_Up>(),
                                 int> = 0>
  constexpr optional& operator=(const optional<_Up>& __v);

  template <class _Up, enable_if_t<_CheckOptionalLikeCtor<_Up, _Up&&>::
                                     template __enable_assign<_Up>(),
                                 int> = 0>
  constexpr optional& operator=(optional<_Up>&& __v);

  const _Tp& operator*() const&;
  _Tp& operator*() &;
  const _Tp&& operator*() const&&;
  _Tp&& operator*() &&;

  const _Tp* operator->() const;
  _Tp* operator->();

  const _Tp& value() const&;
  _Tp& value() &;
  const _Tp&& value() const&&;
  _Tp&& value() &&;

  template <typename U>
  constexpr _Tp value_or(U&& v) const&;
  template <typename U>
  _Tp value_or(U&& v) &&;

  template <typename... Args>
  _Tp& emplace(Args&&... args);

  template <typename U, typename... Args>
  _Tp& emplace(std::initializer_list<U> ilist, Args&&... args);

  using __base::reset;

  constexpr explicit operator bool() const noexcept;
  using __base::has_value;

  constexpr void swap(optional& __opt) noexcept;
};

template <typename T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& v);

template <typename T, typename... Args>
constexpr optional<T> make_optional(Args&&... args);

template <typename T, typename U, typename... Args>
constexpr optional<T> make_optional(std::initializer_list<U> il,
                                    Args&&... args);

template <typename T, typename U>
constexpr bool operator==(const optional<T> &lhs, const optional<U> &rhs);
template <typename T, typename U>
constexpr bool operator!=(const optional<T> &lhs, const optional<U> &rhs);

template <typename T>
constexpr bool operator==(const optional<T> &opt, nullopt_t);

// C++20 and later do not define the following overloads because they are
// provided by rewritten candidates instead.
#if __cplusplus < 202002L
template <typename T>
constexpr bool operator==(nullopt_t, const optional<T> &opt);
template <typename T>
constexpr bool operator!=(const optional<T> &opt, nullopt_t);
template <typename T>
constexpr bool operator!=(nullopt_t, const optional<T> &opt);
#endif  // __cplusplus < 202002L

template <typename T, typename U>
constexpr bool operator==(const optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator==(const T &value, const optional<U> &opt);
template <typename T, typename U>
constexpr bool operator!=(const optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator!=(const T &value, const optional<U> &opt);

} // namespace std
)";

static constexpr char AbslOptionalHeader[] = R"(
#include "absl_type_traits.h"
#include "std_initializer_list.h"
#include "std_type_traits.h"
#include "std_utility.h"

namespace absl {

struct nullopt_t {
  constexpr explicit nullopt_t() {}
};
constexpr nullopt_t nullopt;

struct in_place_t {};
constexpr in_place_t in_place;

template <typename T>
class optional;

namespace optional_internal {

template <typename T, typename U>
struct is_constructible_convertible_from_optional
    : std::integral_constant<
          bool, std::is_constructible<T, optional<U>&>::value ||
                    std::is_constructible<T, optional<U>&&>::value ||
                    std::is_constructible<T, const optional<U>&>::value ||
                    std::is_constructible<T, const optional<U>&&>::value ||
                    std::is_convertible<optional<U>&, T>::value ||
                    std::is_convertible<optional<U>&&, T>::value ||
                    std::is_convertible<const optional<U>&, T>::value ||
                    std::is_convertible<const optional<U>&&, T>::value> {};

template <typename T, typename U>
struct is_constructible_convertible_assignable_from_optional
    : std::integral_constant<
          bool, is_constructible_convertible_from_optional<T, U>::value ||
                    std::is_assignable<T&, optional<U>&>::value ||
                    std::is_assignable<T&, optional<U>&&>::value ||
                    std::is_assignable<T&, const optional<U>&>::value ||
                    std::is_assignable<T&, const optional<U>&&>::value> {};

}  // namespace optional_internal

template <typename T>
class optional {
 public:
  constexpr optional() noexcept;

  constexpr optional(nullopt_t) noexcept;

  optional(const optional&) = default;

  optional(optional&&) = default;

  template <typename InPlaceT, typename... Args,
            absl::enable_if_t<absl::conjunction<
                std::is_same<InPlaceT, in_place_t>,
                std::is_constructible<T, Args&&...>>::value>* = nullptr>
  constexpr explicit optional(InPlaceT, Args&&... args);

  template <typename U, typename... Args,
            typename = typename std::enable_if<std::is_constructible<
                T, std::initializer_list<U>&, Args&&...>::value>::type>
  constexpr explicit optional(in_place_t, std::initializer_list<U> il,
                              Args&&... args);

  template <
      typename U = T,
      typename std::enable_if<
          absl::conjunction<absl::negation<std::is_same<
                                in_place_t, typename std::decay<U>::type>>,
                            absl::negation<std::is_same<
                                optional<T>, typename std::decay<U>::type>>,
                            std::is_convertible<U&&, T>,
                            std::is_constructible<T, U&&>>::value,
          bool>::type = false>
  constexpr optional(U&& v);

  template <
      typename U = T,
      typename std::enable_if<
          absl::conjunction<absl::negation<std::is_same<
                                in_place_t, typename std::decay<U>::type>>,
                            absl::negation<std::is_same<
                                optional<T>, typename std::decay<U>::type>>,
                            absl::negation<std::is_convertible<U&&, T>>,
                            std::is_constructible<T, U&&>>::value,
          bool>::type = false>
  explicit constexpr optional(U&& v);

  template <typename U,
            typename std::enable_if<
                absl::conjunction<
                    absl::negation<std::is_same<T, U>>,
                    std::is_constructible<T, const U&>,
                    absl::negation<
                        optional_internal::
                            is_constructible_convertible_from_optional<T, U>>,
                    std::is_convertible<const U&, T>>::value,
                bool>::type = false>
  optional(const optional<U>& rhs);

  template <typename U,
            typename std::enable_if<
                absl::conjunction<
                    absl::negation<std::is_same<T, U>>,
                    std::is_constructible<T, const U&>,
                    absl::negation<
                        optional_internal::
                            is_constructible_convertible_from_optional<T, U>>,
                    absl::negation<std::is_convertible<const U&, T>>>::value,
                bool>::type = false>
  explicit optional(const optional<U>& rhs);

  template <
      typename U,
      typename std::enable_if<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>, std::is_constructible<T, U&&>,
              absl::negation<
                  optional_internal::is_constructible_convertible_from_optional<
                      T, U>>,
              std::is_convertible<U&&, T>>::value,
          bool>::type = false>
  optional(optional<U>&& rhs);

  template <
      typename U,
      typename std::enable_if<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>, std::is_constructible<T, U&&>,
              absl::negation<
                  optional_internal::is_constructible_convertible_from_optional<
                      T, U>>,
              absl::negation<std::is_convertible<U&&, T>>>::value,
          bool>::type = false>
  explicit optional(optional<U>&& rhs);

  optional& operator=(nullopt_t) noexcept;

  optional& operator=(const optional& src);

  optional& operator=(optional&& src);

  template <
      typename U = T,
      typename = typename std::enable_if<absl::conjunction<
          absl::negation<
              std::is_same<optional<T>, typename std::decay<U>::type>>,
          absl::negation<
              absl::conjunction<std::is_scalar<T>,
                                std::is_same<T, typename std::decay<U>::type>>>,
          std::is_constructible<T, U>, std::is_assignable<T&, U>>::value>::type>
  optional& operator=(U&& v);

  template <
      typename U,
      typename = typename std::enable_if<absl::conjunction<
          absl::negation<std::is_same<T, U>>,
          std::is_constructible<T, const U&>, std::is_assignable<T&, const U&>,
          absl::negation<
              optional_internal::
                  is_constructible_convertible_assignable_from_optional<
                      T, U>>>::value>::type>
  optional& operator=(const optional<U>& rhs);

  template <typename U,
            typename = typename std::enable_if<absl::conjunction<
                absl::negation<std::is_same<T, U>>, std::is_constructible<T, U>,
                std::is_assignable<T&, U>,
                absl::negation<
                    optional_internal::
                        is_constructible_convertible_assignable_from_optional<
                            T, U>>>::value>::type>
  optional& operator=(optional<U>&& rhs);

  const T& operator*() const&;
  T& operator*() &;
  const T&& operator*() const&&;
  T&& operator*() &&;

  const T* operator->() const;
  T* operator->();

  const T& value() const&;
  T& value() &;
  const T&& value() const&&;
  T&& value() &&;

  template <typename U>
  constexpr T value_or(U&& v) const&;
  template <typename U>
  T value_or(U&& v) &&;

  template <typename... Args>
  T& emplace(Args&&... args);

  template <typename U, typename... Args>
  T& emplace(std::initializer_list<U> ilist, Args&&... args);

  void reset() noexcept;

  constexpr explicit operator bool() const noexcept;
  constexpr bool has_value() const noexcept;

  void swap(optional& rhs) noexcept;
};

template <typename T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& v);

template <typename T, typename... Args>
constexpr optional<T> make_optional(Args&&... args);

template <typename T, typename U, typename... Args>
constexpr optional<T> make_optional(std::initializer_list<U> il,
                                    Args&&... args);

template <typename T, typename U>
constexpr bool operator==(const optional<T> &lhs, const optional<U> &rhs);
template <typename T, typename U>
constexpr bool operator!=(const optional<T> &lhs, const optional<U> &rhs);

template <typename T>
constexpr bool operator==(const optional<T> &opt, nullopt_t);
template <typename T>
constexpr bool operator==(nullopt_t, const optional<T> &opt);
template <typename T>
constexpr bool operator!=(const optional<T> &opt, nullopt_t);
template <typename T>
constexpr bool operator!=(nullopt_t, const optional<T> &opt);

template <typename T, typename U>
constexpr bool operator==(const optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator==(const T &value, const optional<U> &opt);
template <typename T, typename U>
constexpr bool operator!=(const optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator!=(const T &value, const optional<U> &opt);

} // namespace absl
)";

static constexpr char BaseOptionalHeader[] = R"(
#include "std_initializer_list.h"
#include "std_type_traits.h"
#include "std_utility.h"

namespace base {

struct in_place_t {};
constexpr in_place_t in_place;

struct nullopt_t {
  constexpr explicit nullopt_t() {}
};
constexpr nullopt_t nullopt;

template <typename T>
class Optional;

namespace internal {

template <typename T>
using RemoveCvRefT = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T, typename U>
struct IsConvertibleFromOptional
    : std::integral_constant<
          bool, std::is_constructible<T, Optional<U>&>::value ||
                    std::is_constructible<T, const Optional<U>&>::value ||
                    std::is_constructible<T, Optional<U>&&>::value ||
                    std::is_constructible<T, const Optional<U>&&>::value ||
                    std::is_convertible<Optional<U>&, T>::value ||
                    std::is_convertible<const Optional<U>&, T>::value ||
                    std::is_convertible<Optional<U>&&, T>::value ||
                    std::is_convertible<const Optional<U>&&, T>::value> {};

template <typename T, typename U>
struct IsAssignableFromOptional
    : std::integral_constant<
          bool, IsConvertibleFromOptional<T, U>::value ||
                    std::is_assignable<T&, Optional<U>&>::value ||
                    std::is_assignable<T&, const Optional<U>&>::value ||
                    std::is_assignable<T&, Optional<U>&&>::value ||
                    std::is_assignable<T&, const Optional<U>&&>::value> {};

}  // namespace internal

template <typename T>
class Optional {
 public:
  using value_type = T;

  constexpr Optional() = default;
  constexpr Optional(const Optional& other) noexcept = default;
  constexpr Optional(Optional&& other) noexcept = default;

  constexpr Optional(nullopt_t);

  template <typename U,
            typename std::enable_if<
                std::is_constructible<T, const U&>::value &&
                    !internal::IsConvertibleFromOptional<T, U>::value &&
                    std::is_convertible<const U&, T>::value,
                bool>::type = false>
  Optional(const Optional<U>& other) noexcept;

  template <typename U,
            typename std::enable_if<
                std::is_constructible<T, const U&>::value &&
                    !internal::IsConvertibleFromOptional<T, U>::value &&
                    !std::is_convertible<const U&, T>::value,
                bool>::type = false>
  explicit Optional(const Optional<U>& other) noexcept;

  template <typename U,
            typename std::enable_if<
                std::is_constructible<T, U&&>::value &&
                    !internal::IsConvertibleFromOptional<T, U>::value &&
                    std::is_convertible<U&&, T>::value,
                bool>::type = false>
  Optional(Optional<U>&& other) noexcept;

  template <typename U,
            typename std::enable_if<
                std::is_constructible<T, U&&>::value &&
                    !internal::IsConvertibleFromOptional<T, U>::value &&
                    !std::is_convertible<U&&, T>::value,
                bool>::type = false>
  explicit Optional(Optional<U>&& other) noexcept;

  template <class... Args>
  constexpr explicit Optional(in_place_t, Args&&... args);

  template <class U, class... Args,
            class = typename std::enable_if<std::is_constructible<
                value_type, std::initializer_list<U>&, Args...>::value>::type>
  constexpr explicit Optional(in_place_t, std::initializer_list<U> il,
                              Args&&... args);

  template <
      typename U = value_type,
      typename std::enable_if<
          std::is_constructible<T, U&&>::value &&
              !std::is_same<internal::RemoveCvRefT<U>, in_place_t>::value &&
              !std::is_same<internal::RemoveCvRefT<U>, Optional<T>>::value &&
              std::is_convertible<U&&, T>::value,
          bool>::type = false>
  constexpr Optional(U&& value);

  template <
      typename U = value_type,
      typename std::enable_if<
          std::is_constructible<T, U&&>::value &&
              !std::is_same<internal::RemoveCvRefT<U>, in_place_t>::value &&
              !std::is_same<internal::RemoveCvRefT<U>, Optional<T>>::value &&
              !std::is_convertible<U&&, T>::value,
          bool>::type = false>
  constexpr explicit Optional(U&& value);

  Optional& operator=(const Optional& other) noexcept;

  Optional& operator=(Optional&& other) noexcept;

  Optional& operator=(nullopt_t);

  template <typename U>
  typename std::enable_if<
      !std::is_same<internal::RemoveCvRefT<U>, Optional<T>>::value &&
          std::is_constructible<T, U>::value &&
          std::is_assignable<T&, U>::value &&
          (!std::is_scalar<T>::value ||
           !std::is_same<typename std::decay<U>::type, T>::value),
      Optional&>::type
  operator=(U&& value) noexcept;

  template <typename U>
  typename std::enable_if<!internal::IsAssignableFromOptional<T, U>::value &&
                              std::is_constructible<T, const U&>::value &&
                              std::is_assignable<T&, const U&>::value,
                          Optional&>::type
  operator=(const Optional<U>& other) noexcept;

  template <typename U>
  typename std::enable_if<!internal::IsAssignableFromOptional<T, U>::value &&
                              std::is_constructible<T, U>::value &&
                              std::is_assignable<T&, U>::value,
                          Optional&>::type
  operator=(Optional<U>&& other) noexcept;

  const T& operator*() const&;
  T& operator*() &;
  const T&& operator*() const&&;
  T&& operator*() &&;

  const T* operator->() const;
  T* operator->();

  const T& value() const&;
  T& value() &;
  const T&& value() const&&;
  T&& value() &&;

  template <typename U>
  constexpr T value_or(U&& v) const&;
  template <typename U>
  T value_or(U&& v) &&;

  template <typename... Args>
  T& emplace(Args&&... args);

  template <typename U, typename... Args>
  T& emplace(std::initializer_list<U> ilist, Args&&... args);

  void reset() noexcept;

  constexpr explicit operator bool() const noexcept;
  constexpr bool has_value() const noexcept;

  void swap(Optional& other);
};

template <typename T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& v);

template <typename T, typename... Args>
constexpr Optional<T> make_optional(Args&&... args);

template <typename T, typename U, typename... Args>
constexpr Optional<T> make_optional(std::initializer_list<U> il,
                                    Args&&... args);

template <typename T, typename U>
constexpr bool operator==(const Optional<T> &lhs, const Optional<U> &rhs);
template <typename T, typename U>
constexpr bool operator!=(const Optional<T> &lhs, const Optional<U> &rhs);

template <typename T>
constexpr bool operator==(const Optional<T> &opt, nullopt_t);
template <typename T>
constexpr bool operator==(nullopt_t, const Optional<T> &opt);
template <typename T>
constexpr bool operator!=(const Optional<T> &opt, nullopt_t);
template <typename T>
constexpr bool operator!=(nullopt_t, const Optional<T> &opt);

template <typename T, typename U>
constexpr bool operator==(const Optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator==(const T &value, const Optional<U> &opt);
template <typename T, typename U>
constexpr bool operator!=(const Optional<T> &opt, const U &value);
template <typename T, typename U>
constexpr bool operator!=(const T &value, const Optional<U> &opt);

} // namespace base
)";

/// Replaces all occurrences of `Pattern` in `S` with `Replacement`.
static void ReplaceAllOccurrences(std::string &S, const std::string &Pattern,
                                  const std::string &Replacement) {
  size_t Pos = 0;
  while (true) {
    Pos = S.find(Pattern, Pos);
    if (Pos == std::string::npos)
      break;
    S.replace(Pos, Pattern.size(), Replacement);
  }
}

struct OptionalTypeIdentifier {
  std::string NamespaceName;
  std::string TypeName;
};

static raw_ostream &operator<<(raw_ostream &OS,
                               const OptionalTypeIdentifier &TypeId) {
  OS << TypeId.NamespaceName << "::" << TypeId.TypeName;
  return OS;
}

class UncheckedOptionalAccessTest
    : public ::testing::TestWithParam<OptionalTypeIdentifier> {
protected:
  // Check that after running the analysis on SourceCode, it produces the
  // expected diagnostics according to [[unsafe]] annotations.
  // - No annotations => no diagnostics.
  // - Given "// [[unsafe]]" annotations on a line, we expect a diagnostic on
  //   that line.
  // - Given "// [[unsafe:range_text]]" annotations on a line, we expect a
  //   diagnostic on that line, and we expect the diagnostic Range (printed as
  //   a string) to match the "range_text".
  void ExpectDiagnosticsFor(std::string SourceCode,
                            bool IgnoreSmartPointerDereference = true) {
    ExpectDiagnosticsFor(SourceCode, ast_matchers::hasName("target"),
                         IgnoreSmartPointerDereference);
  }

  void ExpectDiagnosticsForLambda(std::string SourceCode,
                                  bool IgnoreSmartPointerDereference = true) {
    ExpectDiagnosticsFor(
        SourceCode,
        ast_matchers::hasDeclContext(
            ast_matchers::cxxRecordDecl(ast_matchers::isLambda())),
        IgnoreSmartPointerDereference);
  }

  template <typename FuncDeclMatcher>
  void ExpectDiagnosticsFor(std::string SourceCode, FuncDeclMatcher FuncMatcher,
                            bool IgnoreSmartPointerDereference = true) {
    // Run in C++17 and C++20 mode to cover differences in the AST between modes
    // (e.g. C++20 can contain `CXXRewrittenBinaryOperator`).
    for (const char *CxxMode : {"-std=c++17", "-std=c++20"})
      ExpectDiagnosticsFor(SourceCode, FuncMatcher, CxxMode,
                           IgnoreSmartPointerDereference);
  }

  template <typename FuncDeclMatcher>
  void ExpectDiagnosticsFor(std::string SourceCode, FuncDeclMatcher FuncMatcher,
                            const char *CxxMode,
                            bool IgnoreSmartPointerDereference) {
    ReplaceAllOccurrences(SourceCode, "$ns", GetParam().NamespaceName);
    ReplaceAllOccurrences(SourceCode, "$optional", GetParam().TypeName);

    std::vector<std::pair<std::string, std::string>> Headers;
    Headers.emplace_back("cstddef.h", CSDtdDefHeader);
    Headers.emplace_back("std_initializer_list.h", StdInitializerListHeader);
    Headers.emplace_back("std_string.h", StdStringHeader);
    Headers.emplace_back("std_type_traits.h", StdTypeTraitsHeader);
    Headers.emplace_back("std_utility.h", StdUtilityHeader);
    Headers.emplace_back("std_optional.h", StdOptionalHeader);
    Headers.emplace_back("absl_type_traits.h", AbslTypeTraitsHeader);
    Headers.emplace_back("absl_optional.h", AbslOptionalHeader);
    Headers.emplace_back("base_optional.h", BaseOptionalHeader);
    Headers.emplace_back("unchecked_optional_access_test.h", R"(
      #include "absl_optional.h"
      #include "base_optional.h"
      #include "std_initializer_list.h"
      #include "std_optional.h"
      #include "std_string.h"
      #include "std_utility.h"

      template <typename T>
      T Make();
    )");
    UncheckedOptionalAccessModelOptions Options{IgnoreSmartPointerDereference};
    std::vector<UncheckedOptionalAccessDiagnostic> Diagnostics;
    llvm::Error Error = checkDataflow<UncheckedOptionalAccessModel>(
        AnalysisInputs<UncheckedOptionalAccessModel>(
            SourceCode, std::move(FuncMatcher),
            [](ASTContext &Ctx, Environment &Env) {
              return UncheckedOptionalAccessModel(Ctx, Env);
            })
            .withDiagnosisCallbacks(
                {/*Before=*/[&Diagnostics,
                             Diagnoser =
                                 UncheckedOptionalAccessDiagnoser(Options)](
                                ASTContext &Ctx, const CFGElement &Elt,
                                const TransferStateForDiagnostics<
                                    UncheckedOptionalAccessLattice>
                                    &State) mutable {
                   auto EltDiagnostics = Diagnoser(Elt, Ctx, State);
                   llvm::move(EltDiagnostics, std::back_inserter(Diagnostics));
                 },
                 /*After=*/nullptr})
            .withASTBuildArgs(
                {"-fsyntax-only", CxxMode, "-Wno-undefined-inline"})
            .withASTBuildVirtualMappedFiles(
                tooling::FileContentMappings(Headers.begin(), Headers.end())),
        /*VerifyResults=*/[&Diagnostics](
                              const llvm::DenseMap<unsigned, std::string>
                                  &Annotations,
                              const AnalysisOutputs &AO) {
          llvm::DenseSet<unsigned> AnnotationLines;
          llvm::DenseMap<unsigned, std::string> AnnotationRangesInLines;
          for (const auto &[Line, AnnotationWithMaybeRange] : Annotations) {
            AnnotationLines.insert(Line);
            auto it = AnnotationWithMaybeRange.find(':');
            if (it != std::string::npos) {
              AnnotationRangesInLines[Line] =
                  AnnotationWithMaybeRange.substr(it + 1);
            }
          }
          auto &SrcMgr = AO.ASTCtx.getSourceManager();
          llvm::DenseSet<unsigned> DiagnosticLines;
          for (const UncheckedOptionalAccessDiagnostic &Diag : Diagnostics) {
            unsigned Line = SrcMgr.getPresumedLineNumber(Diag.Range.getBegin());
            DiagnosticLines.insert(Line);
            if (!AnnotationLines.contains(Line)) {
              IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts(
                  new DiagnosticOptions());
              TextDiagnostic TD(llvm::errs(), AO.ASTCtx.getLangOpts(),
                                DiagOpts.get());
              TD.emitDiagnostic(FullSourceLoc(Diag.Range.getBegin(), SrcMgr),
                                DiagnosticsEngine::Error,
                                "unexpected diagnostic", {Diag.Range}, {});
            } else {
              auto it = AnnotationRangesInLines.find(Line);
              if (it != AnnotationRangesInLines.end()) {
                EXPECT_EQ(Diag.Range.getAsRange().printToString(SrcMgr),
                          it->second);
              }
            }
          }

          EXPECT_THAT(DiagnosticLines, ContainerEq(AnnotationLines));
        });
    if (Error)
      FAIL() << llvm::toString(std::move(Error));
  }
};

INSTANTIATE_TEST_SUITE_P(
    UncheckedOptionalUseTestInst, UncheckedOptionalAccessTest,
    ::testing::Values(OptionalTypeIdentifier{"std", "optional"},
                      OptionalTypeIdentifier{"absl", "optional"},
                      OptionalTypeIdentifier{"base", "Optional"}),
    [](const ::testing::TestParamInfo<OptionalTypeIdentifier> &Info) {
      return Info.param.NamespaceName;
    });

// Verifies that similarly-named types are ignored.
TEST_P(UncheckedOptionalAccessTest, NonTrackedOptionalType) {
  ExpectDiagnosticsFor(
      R"(
    namespace other {
    namespace $ns {
    template <typename T>
    struct $optional {
      T value();
    };
    }

    void target($ns::$optional<int> opt) {
      opt.value();
    }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EmptyFunctionBody) {
  ExpectDiagnosticsFor(R"(
    void target() {
      (void)0;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UnwrapUsingValueNoCheck) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      std::move(opt).value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UnwrapUsingOperatorStarNoCheck) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      *opt; // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      *std::move(opt); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UnwrapUsingOperatorArrowNoCheck) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      void foo();
    };

    void target($ns::$optional<Foo> opt) {
      opt->foo(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      void foo();
    };

    void target($ns::$optional<Foo> opt) {
      std::move(opt)->foo(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, HasValueCheck) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      if (opt.has_value()) {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OperatorBoolCheck) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      if (opt) {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UnwrapFunctionCallResultNoCheck) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      Make<$ns::$optional<int>>().value(); // [[unsafe]]
      (void)0;
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      std::move(opt).value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, DefaultConstructor) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt;
      opt.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NulloptConstructor) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt($ns::nullopt);
      opt.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NulloptConstructorWithSugaredType) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"
    template <typename T>
    using wrapper = T;

    template <typename T>
    wrapper<T> wrap(T);

    void target() {
      $ns::$optional<int> opt(wrap($ns::nullopt));
      opt.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InPlaceConstructor) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt($ns::in_place, 3);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    void target() {
      $ns::$optional<Foo> opt($ns::in_place);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      explicit Foo(int, bool);
    };

    void target() {
      $ns::$optional<Foo> opt($ns::in_place, 3, false);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      explicit Foo(std::initializer_list<int>);
    };

    void target() {
      $ns::$optional<Foo> opt($ns::in_place, {3});
      opt.value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ValueConstructor) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt(21);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = $ns::$optional<int>(21);
      opt.value();
    }
  )");
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<$ns::$optional<int>> opt(Make<$ns::$optional<int>>());
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct MyString {
      MyString(const char*);
    };

    void target() {
      $ns::$optional<MyString> opt("foo");
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Bar> opt(Make<Foo>());
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      explicit Foo(int);
    };

    void target() {
      $ns::$optional<Foo> opt(3);
      opt.value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ConvertibleOptionalConstructor) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Bar> opt(Make<$ns::$optional<Foo>>());
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      explicit Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Bar> opt(Make<$ns::$optional<Foo>>());
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1 = $ns::nullopt;
      $ns::$optional<Bar> opt2(opt1);
      opt2.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1(Make<Foo>());
      $ns::$optional<Bar> opt2(opt1);
      opt2.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      explicit Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1(Make<Foo>());
      $ns::$optional<Bar> opt2(opt1);
      opt2.value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, MakeOptional) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = $ns::make_optional(0);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      Foo(int, int);
    };

    void target() {
      $ns::$optional<Foo> opt = $ns::make_optional<Foo>(21, 22);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      constexpr Foo(std::initializer_list<char>);
    };

    void target() {
      char a = 'a';
      $ns::$optional<Foo> opt = $ns::make_optional<Foo>({a});
      opt.value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ValueOr) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt;
      opt.value_or(0);
      (void)0;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ValueOrComparisonPointers) {
  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int*> opt) {
      if (opt.value_or(nullptr) != nullptr) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )code");
}

TEST_P(UncheckedOptionalAccessTest, ValueOrComparisonIntegers) {
  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      if (opt.value_or(0) != 0) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )code");
}

TEST_P(UncheckedOptionalAccessTest, ValueOrComparisonStrings) {
  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<std::string> opt) {
      if (!opt.value_or("").empty()) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )code");

  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<std::string> opt) {
      if (opt.value_or("") != "") {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )code");
}

TEST_P(UncheckedOptionalAccessTest, ValueOrComparisonPointerToOptional) {
  // FIXME: make `opt` a parameter directly, once we ensure that all `optional`
  // values have a `has_value` property.
  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> p) {
      $ns::$optional<int> *opt = &p;
      if (opt->value_or(0) != 0) {
        opt->value();
      } else {
        opt->value(); // [[unsafe]]
      }
    }
  )code");
}

TEST_P(UncheckedOptionalAccessTest, Emplace) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt;
      opt.emplace(0);
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> *opt) {
      opt->emplace(0);
      opt->value();
    }
  )");

  // FIXME: Add tests that call `emplace` in conditional branches:
  //  ExpectDiagnosticsFor(
  //      R"(
  //    #include "unchecked_optional_access_test.h"
  //
  //    void target($ns::$optional<int> opt, bool b) {
  //      if (b) {
  //        opt.emplace(0);
  //      }
  //      if (b) {
  //        opt.value();
  //      } else {
  //        opt.value(); // [[unsafe]]
  //      }
  //    }
  //  )");
}

TEST_P(UncheckedOptionalAccessTest, Reset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = $ns::make_optional(0);
      opt.reset();
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> &opt) {
      if (opt.has_value()) {
        opt.reset();
        opt.value(); // [[unsafe]]
      }
    }
  )");

  // FIXME: Add tests that call `reset` in conditional branches:
  //  ExpectDiagnosticsFor(
  //      R"(
  //    #include "unchecked_optional_access_test.h"
  //
  //    void target(bool b) {
  //      $ns::$optional<int> opt = $ns::make_optional(0);
  //      if (b) {
  //        opt.reset();
  //      }
  //      if (b) {
  //        opt.value(); // [[unsafe]]
  //      } else {
  //        opt.value();
  //      }
  //    }
  //  )");
}

TEST_P(UncheckedOptionalAccessTest, ValueAssignment) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    void target() {
      $ns::$optional<Foo> opt;
      opt = Foo();
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    void target() {
      $ns::$optional<Foo> opt;
      (opt = Foo()).value();
      (void)0;
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct MyString {
      MyString(const char*);
    };

    void target() {
      $ns::$optional<MyString> opt;
      opt = "foo";
      opt.value();
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct MyString {
      MyString(const char*);
    };

    void target() {
      $ns::$optional<MyString> opt;
      (opt = "foo").value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OptionalConversionAssignment) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1 = Foo();
      $ns::$optional<Bar> opt2;
      opt2 = opt1;
      opt2.value();
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1;
      $ns::$optional<Bar> opt2;
      if (opt2.has_value()) {
        opt2 = opt1;
        opt2.value(); // [[unsafe]]
      }
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {};

    struct Bar {
      Bar(const Foo&);
    };

    void target() {
      $ns::$optional<Foo> opt1 = Foo();
      $ns::$optional<Bar> opt2;
      (opt2 = opt1).value();
      (void)0;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NulloptAssignment) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      opt = $ns::nullopt;
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      (opt = $ns::nullopt).value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OptionalSwap) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = 3;

      opt1.swap(opt2);

      opt1.value();

      opt2.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = 3;

      opt2.swap(opt1);

      opt1.value();

      opt2.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OptionalReturnedFromFuntionCall) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"
    
    struct S {
      $ns::$optional<float> x;
    } s;
    S getOptional() {
      return s;
    }

    void target() {
      getOptional().x = 0;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NonConstMethodMayClearOptionalField) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      $ns::$optional<std::string> opt;
      void clear();  // assume this may modify the opt field's state
    };

    void target(Foo& foo) {
      if (foo.opt) {
        foo.opt.value();
        foo.clear();
        foo.opt.value();  // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest,
       NonConstMethodMayNotClearConstOptionalField) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      const $ns::$optional<std::string> opt;
      void clear();
    };

    void target(Foo& foo) {
      if (foo.opt) {
        foo.opt.value();
        foo.clear();
        foo.opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, StdSwap) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = 3;

      std::swap(opt1, opt2);

      opt1.value();

      opt2.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = 3;

      std::swap(opt2, opt1);

      opt1.value();

      opt2.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledLocLeft) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct L { $ns::$optional<int> hd; L* tl; };

    void target() {
      $ns::$optional<int> foo = 3;
      L bar;

      // Any `tl` beyond the first is not modeled.
      bar.tl->tl->hd.swap(foo);

      bar.tl->tl->hd.value(); // [[unsafe]]
      foo.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledLocRight) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct L { $ns::$optional<int> hd; L* tl; };

    void target() {
      $ns::$optional<int> foo = 3;
      L bar;

      // Any `tl` beyond the first is not modeled.
      foo.swap(bar.tl->tl->hd);

      bar.tl->tl->hd.value(); // [[unsafe]]
      foo.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledValueLeftSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct S { int x; };
    struct A { $ns::$optional<S> late; };
    struct B { A f3; };
    struct C { B f2; };
    struct D { C f1; };

    void target() {
      $ns::$optional<S> foo = S{3};
      D bar;

      bar.f1.f2.f3.late.swap(foo);

      bar.f1.f2.f3.late.value();
      foo.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledValueLeftUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct S { int x; };
    struct A { $ns::$optional<S> late; };
    struct B { A f3; };
    struct C { B f2; };
    struct D { C f1; };

    void target() {
      $ns::$optional<S> foo;
      D bar;

      bar.f1.f2.f3.late.swap(foo);

      bar.f1.f2.f3.late.value(); // [[unsafe]]
      foo.value(); // [[unsafe]]
    }
  )");
}

// fixme: use recursion instead of depth.
TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledValueRightSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct S { int x; };
    struct A { $ns::$optional<S> late; };
    struct B { A f3; };
    struct C { B f2; };
    struct D { C f1; };

    void target() {
      $ns::$optional<S> foo = S{3};
      D bar;

      foo.swap(bar.f1.f2.f3.late);

      bar.f1.f2.f3.late.value();
      foo.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, SwapUnmodeledValueRightUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct S { int x; };
    struct A { $ns::$optional<S> late; };
    struct B { A f3; };
    struct C { B f2; };
    struct D { C f1; };

    void target() {
      $ns::$optional<S> foo;
      D bar;

      foo.swap(bar.f1.f2.f3.late);

      bar.f1.f2.f3.late.value(); // [[unsafe]]
      foo.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UniquePtrToOptional) {
  // We suppress diagnostics for optionals in smart pointers (other than
  // `optional` itself).
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    template <typename T>
    struct smart_ptr {
      T& operator*() &;
      T* operator->();
    };

    void target() {
      smart_ptr<$ns::$optional<bool>> foo;
      foo->value();
      (*foo).value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, UniquePtrToStructWithOptionalField) {
  // We suppress diagnostics for optional fields reachable from smart pointers
  // (other than `optional` itself) through (exactly) one member access.
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    template <typename T>
    struct smart_ptr {
      T& operator*() &;
      T* operator->();
    };

    struct Foo {
      $ns::$optional<int> opt;
    };

    void target() {
      smart_ptr<Foo> foo;
      *foo->opt;
      *(*foo).opt;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, CallReturningOptional) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    $ns::$optional<int> MakeOpt();

    void target() {
      $ns::$optional<int> opt = 0;
      opt = MakeOpt();
      opt.value(); // [[unsafe]]
    }
  )");
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    const $ns::$optional<int>& MakeOpt();

    void target() {
      $ns::$optional<int> opt = 0;
      opt = MakeOpt();
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using IntOpt = $ns::$optional<int>;
    IntOpt MakeOpt();

    void target() {
      IntOpt opt = 0;
      opt = MakeOpt();
      opt.value(); // [[unsafe]]
    }
  )");

  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using IntOpt = $ns::$optional<int>;
    const IntOpt& MakeOpt();

    void target() {
      IntOpt opt = 0;
      opt = MakeOpt();
      opt.value(); // [[unsafe]]
    }
  )");
}


TEST_P(UncheckedOptionalAccessTest, EqualityCheckLeftSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = 3;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 == opt2) {
        opt2.value();
      } else {
        opt2.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckRightSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = 3;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt2 == opt1) {
        opt2.value();
      } else {
        opt2.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckVerifySetAfterEq) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = Make<$ns::$optional<int>>();
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 == opt2) {
        if (opt1.has_value())
          opt2.value();
        if (opt2.has_value())
          opt1.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckLeftUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 == opt2) {
        opt2.value(); // [[unsafe]]
      } else {
        opt2.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckRightUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt2 == opt1) {
        opt2.value(); // [[unsafe]]
      } else {
        opt2.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckRightNullopt) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (opt == $ns::nullopt) {
        opt.value(); // [[unsafe]]
      } else {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckLeftNullopt) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if ($ns::nullopt == opt) {
        opt.value(); // [[unsafe]]
      } else {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckRightValue) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (opt == 3) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, EqualityCheckLeftValue) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (3 == opt) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckLeftSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = 3;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 != opt2) {
        opt2.value(); // [[unsafe]]
      } else {
        opt2.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckRightSet) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = 3;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt2 != opt1) {
        opt2.value(); // [[unsafe]]
      } else {
        opt2.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckVerifySetAfterEq) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = Make<$ns::$optional<int>>();
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 != opt2) {
        if (opt1.has_value())
          opt2.value(); // [[unsafe]]
        if (opt2.has_value())
          opt1.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckLeftUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt1 != opt2) {
        opt2.value();
      } else {
        opt2.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckRightUnset) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2 = Make<$ns::$optional<int>>();

      if (opt2 != opt1) {
        opt2.value();
      } else {
        opt2.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckRightNullopt) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (opt != $ns::nullopt) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckLeftNullopt) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if ($ns::nullopt != opt) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckRightValue) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (opt != 3) {
        opt.value(); // [[unsafe]]
      } else {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, InequalityCheckLeftValue) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = Make<$ns::$optional<int>>();

      if (3 != opt) {
        opt.value(); // [[unsafe]]
      } else {
        opt.value();
      }
    }
  )");
}

// Verifies that the model sees through aliases.
TEST_P(UncheckedOptionalAccessTest, WithAlias) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    template <typename T>
    using MyOptional = $ns::$optional<T>;

    void target(MyOptional<int> opt) {
      opt.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OptionalValueOptional) {
  // Basic test that nested values are populated.  We nest an optional because
  // its easy to use in a test, but the type of the nested value shouldn't
  // matter.
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using Foo = $ns::$optional<std::string>;

    void target($ns::$optional<Foo> foo) {
      if (foo && *foo) {
        foo->value();
      }
    }
  )");

  // Mutation is supported for nested values.
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using Foo = $ns::$optional<std::string>;

    void target($ns::$optional<Foo> foo) {
      if (foo && *foo) {
        foo->reset();
        foo->value(); // [[unsafe]]
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NestedOptionalAssignValue) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using OptionalInt = $ns::$optional<int>;

    void target($ns::$optional<OptionalInt> opt) {
      if (!opt) return;

      // Accessing the outer optional is OK now.
      *opt;

      // But accessing the nested optional is still unsafe because we haven't
      // checked it.
      **opt;  // [[unsafe]]

      *opt = 1;

      // Accessing the nested optional is safe after assigning a value to it.
      **opt;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, NestedOptionalAssignOptional) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using OptionalInt = $ns::$optional<int>;

    void target($ns::$optional<OptionalInt> opt) {
      if (!opt) return;

      // Accessing the outer optional is OK now.
      *opt;

      // But accessing the nested optional is still unsafe because we haven't
      // checked it.
      **opt;  // [[unsafe]]

      // Assign from `optional<short>` so that we trigger conversion assignment
      // instead of move assignment.
      *opt = $ns::$optional<short>();

      // Accessing the nested optional is still unsafe after assigning an empty
      // optional to it.
      **opt;  // [[unsafe]]
    }
  )");
}

// Tests that structs can be nested. We use an optional field because its easy
// to use in a test, but the type of the field shouldn't matter.
TEST_P(UncheckedOptionalAccessTest, OptionalValueStruct) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      $ns::$optional<std::string> opt;
    };

    void target($ns::$optional<Foo> foo) {
      if (foo && foo->opt) {
        foo->opt.value();
      }
    }
  )");
}

// FIXME: A case that we should handle but currently don't.
// When there is a field of type reference to non-optional, we may
// stop recursively creating storage locations.
// E.g., the field `second` below in `pair` should eventually lead to
// the optional `x` in `A`.
TEST_P(UncheckedOptionalAccessTest, NestedOptionalThroughNonOptionalRefField) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> x;
    };

    struct pair {
      int first;
      const A &second;
    };

    struct B {
      $ns::$optional<pair>& nonConstGetRef();
    };

    void target(B b) {
      const auto& maybe_pair = b.nonConstGetRef();
      if (!maybe_pair.has_value())
        return;

      if(!maybe_pair->second.x.has_value())
        return;
      maybe_pair->second.x.value();  // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, OptionalValueInitialization) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    using Foo = $ns::$optional<std::string>;

    void target($ns::$optional<Foo> foo, bool b) {
      if (!foo.has_value()) return;
      if (b) {
        if (!foo->has_value()) return;
        // We have created `foo.value()`.
        foo->value();
      } else {
        if (!foo->has_value()) return;
        // We have created `foo.value()` again, in a different environment.
        foo->value();
      }
      // Now we merge the two values. UncheckedOptionalAccessModel::merge() will
      // throw away the "value" property.
      foo->value();
    }
  )");
}

// This test is aimed at the core model, not the diagnostic. It is a regression
// test against a crash when using non-trivial smart pointers, like
// `std::unique_ptr`. As such, it doesn't test the access itself, which would be
// ignored regardless because of `IgnoreSmartPointerDereference = true`, above.
TEST_P(UncheckedOptionalAccessTest, AssignThroughLvalueReferencePtr) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    template <typename T>
    struct smart_ptr {
      typename std::add_lvalue_reference<T>::type operator*() &;
    };

    void target() {
      smart_ptr<$ns::$optional<int>> x;
      // Verify that this assignment does not crash.
      *x = 3;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, CorrelatedBranches) {
  ExpectDiagnosticsFor(R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (b || opt.has_value()) {
        if (!b) {
          opt.value();
        }
      }
    }
  )code");

  ExpectDiagnosticsFor(R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (b && !opt.has_value()) return;
      if (b) {
        opt.value();
      }
    }
  )code");

  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (opt.has_value()) b = true;
      if (b) {
        opt.value(); // [[unsafe]]
      }
    }
  )code");

  ExpectDiagnosticsFor(R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (b) return;
      if (opt.has_value()) b = true;
      if (b) {
        opt.value();
      }
    }
  )code");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (opt.has_value() == b) {
        if (b) {
          opt.value();
        }
      }
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target(bool b, $ns::$optional<int> opt) {
      if (opt.has_value() != b) {
        if (!b) {
          opt.value();
        }
      }
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt1 = $ns::nullopt;
      $ns::$optional<int> opt2;
      if (b) {
        opt2 = $ns::nullopt;
      } else {
        opt2 = $ns::nullopt;
      }
      if (opt2.has_value()) {
        opt1.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, JoinDistinctValues) {
  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt;
      if (b) {
        opt = Make<$ns::$optional<int>>();
      } else {
        opt = Make<$ns::$optional<int>>();
      }
      if (opt.has_value()) {
        opt.value();
      } else {
        opt.value(); // [[unsafe]]
      }
    }
  )code");

  ExpectDiagnosticsFor(R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt;
      if (b) {
        opt = Make<$ns::$optional<int>>();
        if (!opt.has_value()) return;
      } else {
        opt = Make<$ns::$optional<int>>();
        if (!opt.has_value()) return;
      }
      opt.value();
    }
  )code");

  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt;
      if (b) {
        opt = Make<$ns::$optional<int>>();
        if (!opt.has_value()) return;
      } else {
        opt = Make<$ns::$optional<int>>();
      }
      opt.value(); // [[unsafe]]
    }
  )code");

  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt;
      if (b) {
        opt = 1;
      } else {
        opt = 2;
      }
      opt.value();
    }
  )code");

  ExpectDiagnosticsFor(
      R"code(
    #include "unchecked_optional_access_test.h"

    void target(bool b) {
      $ns::$optional<int> opt;
      if (b) {
        opt = 1;
      } else {
        opt = Make<$ns::$optional<int>>();
      }
      opt.value(); // [[unsafe]]
    }
  )code");
}

TEST_P(UncheckedOptionalAccessTest, AccessValueInLoop) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      while (Make<bool>()) {
        opt.value();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopWithCheckSafe) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      while (Make<bool>()) {
        opt.value();

        opt = Make<$ns::$optional<int>>();
        if (!opt.has_value()) return;
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopNoCheckUnsafe) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      while (Make<bool>()) {
        opt.value(); // [[unsafe]]

        opt = Make<$ns::$optional<int>>();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopToUnsetUnsafe) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      while (Make<bool>())
        opt = $ns::nullopt;
      $ns::$optional<int> opt2 = $ns::nullopt;
      if (opt.has_value())
        opt2 = $ns::$optional<int>(3);
      opt2.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopToSetUnsafe) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = $ns::nullopt;
      while (Make<bool>())
        opt = $ns::$optional<int>(3);
      $ns::$optional<int> opt2 = $ns::nullopt;
      if (!opt.has_value())
        opt2 = $ns::$optional<int>(3);
      opt2.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopToUnknownUnsafe) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = $ns::nullopt;
      while (Make<bool>())
        opt = Make<$ns::$optional<int>>();
      $ns::$optional<int> opt2 = $ns::nullopt;
      if (!opt.has_value())
        opt2 = $ns::$optional<int>(3);
      opt2.value(); // [[unsafe]]
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ReassignValueInLoopBadConditionUnsafe) {
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      $ns::$optional<int> opt = 3;
      while (Make<bool>()) {
        opt.value(); // [[unsafe]]

        opt = Make<$ns::$optional<int>>();
        if (!opt.has_value()) continue;
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, StructuredBindingsFromStruct) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct kv { $ns::$optional<int> opt; int x; };
    int target() {
      auto [contents, x] = Make<kv>();
      return contents ? *contents : x;
    }
  )");

  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    template <typename T1, typename T2>
    struct pair { T1 fst;  T2 snd; };
    int target() {
      auto [contents, x] = Make<pair<$ns::$optional<int>, int>>();
      return contents ? *contents : x;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, StructuredBindingsFromTupleLikeType) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    namespace std {
    template <class> struct tuple_size;
    template <size_t, class> struct tuple_element;
    template <class...> class tuple;

    template <class... T>
    struct tuple_size<tuple<T...>> : integral_constant<size_t, sizeof...(T)> {};

    template <size_t I, class... T>
    struct tuple_element<I, tuple<T...>> {
      using type =  __type_pack_element<I, T...>;
    };

    template <class...> class tuple {};
    template <size_t I, class... T>
    typename tuple_element<I, tuple<T...>>::type get(tuple<T...>);
    } // namespace std

    std::tuple<$ns::$optional<const char *>, int> get_opt();
    void target() {
      auto [content, ck] = get_opt();
      content ? *content : "";
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, CtorInitializerNullopt) {
  using namespace ast_matchers;
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Target {
      Target(): opt($ns::nullopt) {
        opt.value(); // [[unsafe]]
      }
      $ns::$optional<int> opt;
    };
  )",
      cxxConstructorDecl(ofClass(hasName("Target"))));
}

TEST_P(UncheckedOptionalAccessTest, CtorInitializerValue) {
  using namespace ast_matchers;
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"

    struct Target {
      Target(): opt(3) {
        opt.value();
      }
      $ns::$optional<int> opt;
    };
  )",
      cxxConstructorDecl(ofClass(hasName("Target"))));
}

// This is regression test, it shouldn't crash.
TEST_P(UncheckedOptionalAccessTest, Bitfield) {
  using namespace ast_matchers;
  ExpectDiagnosticsFor(
      R"(
    #include "unchecked_optional_access_test.h"
    struct Dst {
      unsigned int n : 1;
    };
    void target() {
      $ns::$optional<bool> v;
      Dst d;
      if (v.has_value())
        d.n = v.value();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaParam) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target() {
      []($ns::$optional<int> opt) {
        if (opt.has_value()) {
          opt.value();
        } else {
          opt.value(); // [[unsafe]]
        }
      }(Make<$ns::$optional<int>>());
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureByCopy) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      [opt]() {
        if (opt.has_value()) {
          opt.value();
        } else {
          opt.value(); // [[unsafe]]
        }
      }();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureByReference) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      [&opt]() {
        if (opt.has_value()) {
          opt.value();
        } else {
          opt.value(); // [[unsafe]]
        }
      }();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureWithInitializer) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      [opt2=opt]() {
        if (opt2.has_value()) {
          opt2.value();
        } else {
          opt2.value(); // [[unsafe]]
        }
      }();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureByCopyImplicit) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      [=]() {
        if (opt.has_value()) {
          opt.value();
        } else {
          opt.value(); // [[unsafe]]
        }
      }();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureByReferenceImplicit) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      [&]() {
        if (opt.has_value()) {
          opt.value();
        } else {
          opt.value(); // [[unsafe]]
        }
      }();
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureThis) {
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    struct Foo {
      $ns::$optional<int> opt;

      void target() {
        [this]() {
          if (opt.has_value()) {
            opt.value();
          } else {
            opt.value(); // [[unsafe]]
          }
        }();
      }
    };
  )");
}

TEST_P(UncheckedOptionalAccessTest, LambdaCaptureStateNotPropagated) {
  // We can't propagate information from the surrounding context.
  ExpectDiagnosticsForLambda(R"(
    #include "unchecked_optional_access_test.h"

    void target($ns::$optional<int> opt) {
      if (opt.has_value()) {
        [&opt]() {
          opt.value(); // [[unsafe]]
        }();
      }
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ClassDerivedFromOptional) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Derived : public $ns::$optional<int> {};

    void target(Derived opt) {
      *opt;  // [[unsafe]]
      if (opt.has_value())
        *opt;

      // The same thing, but with a pointer receiver.
      Derived *popt = &opt;
      **popt;  // [[unsafe]]
      if (popt->has_value())
        **popt;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ClassTemplateDerivedFromOptional) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    template <class T>
    struct Derived : public $ns::$optional<T> {};

    void target(Derived<int> opt) {
      *opt;  // [[unsafe]]
      if (opt.has_value())
        *opt;

      // The same thing, but with a pointer receiver.
      Derived<int> *popt = &opt;
      **popt;  // [[unsafe]]
      if (popt->has_value())
        **popt;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ClassDerivedPrivatelyFromOptional) {
  // Classes that derive privately from optional can themselves still call
  // member functions of optional. Check that we model the optional correctly
  // in this situation.
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Derived : private $ns::$optional<int> {
      void Method() {
        **this;  // [[unsafe]]
        if (this->has_value())
          **this;
      }
    };
  )",
                       ast_matchers::hasName("Method"));
}

TEST_P(UncheckedOptionalAccessTest, ClassDerivedFromOptionalValueConstructor) {
  ExpectDiagnosticsFor(R"(
    #include "unchecked_optional_access_test.h"

    struct Derived : public $ns::$optional<int> {
      Derived(int);
    };

    void target(Derived opt) {
      *opt;  // [[unsafe]]
      opt = 1;
      *opt;
    }
  )");
}

TEST_P(UncheckedOptionalAccessTest, ConstRefAccessor) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }
      $ns::$optional<int> x;
    };

    void target(A& a) {
      if (a.get().has_value()) {
        a.get().value();
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstRefAccessorWithModInBetween) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }
      void clear();
      $ns::$optional<int> x;
    };

    void target(A& a) {
      if (a.get().has_value()) {
        a.clear();
        a.get().value();  // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstRefAccessorWithModReturningOptional) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }
      $ns::$optional<int> take();
      $ns::$optional<int> x;
    };

    void target(A& a) {
      if (a.get().has_value()) {
        $ns::$optional<int> other = a.take();
        a.get().value();  // [[unsafe]]
        if (other.has_value()) {
          other.value();
        }
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstRefAccessorDifferentObjects) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }
      $ns::$optional<int> x;
    };

    void target(A& a1, A& a2) {
      if (a1.get().has_value()) {
        a2.get().value();  // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstRefAccessorLoop) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }
      $ns::$optional<int> x;
    };

    void target(A& a, int N) {
      for (int i = 0; i < N; ++i) {
        if (a.get().has_value()) {
          a.get().value();
        }
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstByValueAccessor) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> get() const { return x; }
      $ns::$optional<int> x;
    };

    void target(A& a) {
      if (a.get().has_value()) {
        a.get().value();
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstByValueAccessorWithModInBetween) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> get() const { return x; }
      void clear();
      $ns::$optional<int> x;
    };

    void target(A& a) {
      if (a.get().has_value()) {
        a.clear();
        a.get().value();  // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstPointerAccessor) {
  ExpectDiagnosticsFor(R"cc(
     #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> x;
    };

    struct MyUniquePtr {
      A* operator->() const;
    };

    void target(MyUniquePtr p) {
      if (p->x) {
        *p->x;
      }
    }
  )cc",
                       /*IgnoreSmartPointerDereference=*/false);
}

TEST_P(UncheckedOptionalAccessTest, ConstPointerAccessorWithModInBetween) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> x;
    };

    struct MyUniquePtr {
      A* operator->() const;
      void reset(A*);
    };

    void target(MyUniquePtr p) {
      if (p->x) {
        p.reset(nullptr);
        *p->x;  // [[unsafe]]
      }
    }
  )cc",
                       /*IgnoreSmartPointerDereference=*/false);
}

TEST_P(UncheckedOptionalAccessTest, SmartPointerAccessorMixed) {
  ExpectDiagnosticsFor(R"cc(
     #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> x;
    };

    namespace absl {
    template<typename T>
    class StatusOr {
      public:
      bool ok() const;

      const T& operator*() const&;
      T& operator*() &;

      const T* operator->() const;
      T* operator->();

      const T& value() const;
      T& value();
    };
    }

    void target(absl::StatusOr<A> &mut, const absl::StatusOr<A> &imm) {
      if (!mut.ok() || !imm.ok())
        return;

      if (mut->x.has_value()) {
        mut->x.value();
        ((*mut).x).value();
        (mut.value().x).value();

        // check flagged after modifying
        mut = imm;
        mut->x.value();  // [[unsafe]]
      }
      if (imm->x.has_value()) {
        imm->x.value();
        ((*imm).x).value();
        (imm.value().x).value();
      }
    }
  )cc",
                       /*IgnoreSmartPointerDereference=*/false);
}

TEST_P(UncheckedOptionalAccessTest, ConstBoolAccessor) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      bool isFoo() const { return f; }
      bool f;
    };

    void target(A& a) {
      $ns::$optional<int> opt;
      if (a.isFoo()) {
        opt = 1;
      }
      if (a.isFoo()) {
        opt.value();
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstBoolAccessorWithModInBetween) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      bool isFoo() const { return f; }
      void clear();
      bool f;
    };

    void target(A& a) {
      $ns::$optional<int> opt;
      if (a.isFoo()) {
        opt = 1;
      }
      a.clear();
      if (a.isFoo()) {
        opt.value();  // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest,
       ConstRefAccessorToOptionalViaConstRefAccessorToHoldingObject) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      const A& getA() const { return a; }

      A a;
    };

    void target(B& b) {
      if (b.getA().get().has_value()) {
        b.getA().get().value();
      }
    }
  )cc");
}

TEST_P(
    UncheckedOptionalAccessTest,
    ConstRefAccessorToOptionalViaConstRefAccessorToHoldingObjectWithoutValueCheck) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      const A& getA() const { return a; }

      A a;
    };

    void target(B& b) {
      b.getA().get().value(); // [[unsafe]]
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest,
       ConstRefToOptionalSavedAsTemporaryVariable) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      const A& getA() const { return a; }

      A a;
    };

    void target(B& b) {
      const auto& opt = b.getA().get();
      if (opt.has_value()) {
        opt.value();
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest,
       ConstRefAccessorToOptionalViaAccessorToHoldingObjectByValue) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      const A copyA() const { return a; }

      A a;
    };

    void target(B& b) {
      if (b.copyA().get().has_value()) {
        b.copyA().get().value(); // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(UncheckedOptionalAccessTest,
       ConstRefAccessorToOptionalViaNonConstRefAccessorToHoldingObject) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      A& getA() { return a; }

      A a;
    };

    void target(B& b) {
      if (b.getA().get().has_value()) {
        b.getA().get().value(); // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(
    UncheckedOptionalAccessTest,
    ConstRefAccessorToOptionalViaConstRefAccessorToHoldingObjectWithModAfterCheck) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      const $ns::$optional<int>& get() const { return x; }

      $ns::$optional<int> x;
    };

    struct B {
      const A& getA() const { return a; }

      A& getA() { return a; }

      void clear() { a = A{}; }

      A a;
    };

    void target(B& b) {
      // changing field A via non-const getter after const getter check
      if (b.getA().get().has_value()) {
        b.getA() = A{};
        b.getA().get().value(); // [[unsafe]]
      }

      // calling non-const method which might change field A
      if (b.getA().get().has_value()) {
        b.clear();
        b.getA().get().value(); // [[unsafe]]
      }
    }
  )cc");
}

TEST_P(
    UncheckedOptionalAccessTest,
    ConstRefAccessorToOptionalViaConstRefAccessorToHoldingObjectWithAnotherConstCallAfterCheck) {
  ExpectDiagnosticsFor(R"cc(
      #include "unchecked_optional_access_test.h"

      struct A {
        const $ns::$optional<int>& get() const { return x; }

        $ns::$optional<int> x;
      };

      struct B {
        const A& getA() const { return a; }

        void callWithoutChanges() const { 
          // no-op 
        }

        A a;
      };

      void target(B& b) {  
        if (b.getA().get().has_value()) {
          b.callWithoutChanges(); // calling const method which cannot change A
          b.getA().get().value();
        }
      }
    )cc");
}

TEST_P(UncheckedOptionalAccessTest, ConstPointerRefAccessor) {
  // A crash reproducer for https://github.com/llvm/llvm-project/issues/125589
  // NOTE: we currently cache const ref accessors's locations.
  // If we want to support const ref to pointers or bools, we should initialize
  // their values.
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> x;
    };

    struct PtrWrapper {
      A*& getPtrRef() const;
      void reset(A*);
    };

    void target(PtrWrapper p) {
      if (p.getPtrRef()->x) {
        *p.getPtrRef()->x;  // [[unsafe]]
        p.reset(nullptr);
        *p.getPtrRef()->x;  // [[unsafe]]
      }
    }
  )cc",
                       /*IgnoreSmartPointerDereference=*/false);
}

TEST_P(UncheckedOptionalAccessTest, DiagnosticsHaveRanges) {
  ExpectDiagnosticsFor(R"cc(
    #include "unchecked_optional_access_test.h"

    struct A {
      $ns::$optional<int> fi;
    };
    struct B {
      $ns::$optional<A> fa;
    };

    void target($ns::$optional<B> opt) {
      opt.value();  // [[unsafe:<input.cc:12:7>]]
      if (opt) {
        opt  // [[unsafe:<input.cc:14:9, line:16:13>]]
          ->
            fa.value();
        if (opt->fa) {
          opt->fa->fi.value();  // [[unsafe:<input.cc:18:11, col:20>]]
        }
      }
    }
  )cc");
}

// FIXME: Add support for:
// - constructors (copy, move)
// - assignment operators (default, copy, move)
// - invalidation (passing optional by non-const reference/pointer)
