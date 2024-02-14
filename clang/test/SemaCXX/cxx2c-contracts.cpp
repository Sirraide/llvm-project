// RUN: %clang_cc1 -std=c++2c -verify %s

namespace std {
class source_location {
  struct __impl;

public:
  static constexpr source_location
    current(const __impl *__p = __builtin_source_location()) noexcept {
      source_location __loc;
      __loc.__m_impl = __p;
      return __loc;
  }
  constexpr source_location() = default;
  constexpr source_location(source_location const &) = default;
  constexpr unsigned int line() const noexcept { return __m_impl ? __m_impl->_M_line : 0; }
  constexpr unsigned int column() const noexcept { return __m_impl ? __m_impl->_M_column : 0; }
  constexpr const char *file() const noexcept { return __m_impl ? __m_impl->_M_file_name : ""; }
  constexpr const char *function() const noexcept { return __m_impl ? __m_impl->_M_function_name : ""; }

private:
  // Note: The type name "std::source_location::__impl", and its constituent
  // field-names are required by __builtin_source_location().
  struct __impl {
    const char *_M_file_name;
    const char *_M_function_name;
    unsigned _M_line;
    unsigned _M_column;
  };
  const __impl *__m_impl = nullptr;
};
} // namespace std

struct A {
  explicit operator bool() { return true; }
};

struct B {
  template <typename T>
  T foo() { return T{42}; }

};

template <typename T>
void instantiate(T t) {
  contract_assert(t); // expected-warning {{implicit conversion of nullptr constant}} \
                      // expected-error {{value of type 'B' is not contextually convertible to 'bool'}}
}

void f() {
  contract_assert(true);
  contract_assert(false);
  contract_assert(nullptr); // expected-warning {{implicit conversion of nullptr constant}}
  contract_assert("abcd");
  contract_assert(1);
  contract_assert(42);
  contract_assert(A{});
  contract_assert(B{}.foo<int>());
  contract_assert(B{}.foo<double>());
  contract_assert((contract_assert(42), 42)); // check that contract_assert() has side effects.
  instantiate(true);
  instantiate(false);
  instantiate(nullptr); // expected-note {{in instantiation of}}
  instantiate("abcd");
  instantiate(1);
  instantiate(42);
  instantiate(A{});
  instantiate(B{}.foo<int>());
  instantiate(B{}.foo<double>());

  contract_assert(B{}); // expected-error {{value of type 'B' is not contextually convertible to 'bool'}}
  contract_assert((void)1); // expected-error {{value of type 'void' is not contextually convertible to 'bool'}}
  instantiate(B{}); // expected-note {{in instantiation of}}
}
