// RUN: %clang_cc1 -std=c++2c -verify %s

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
