// RUN: %clang_cc1 -verify -fsyntax-only -std=c++26 %s

[[clang::srcc_diagnose_unless_caller("Allowed")]]
void f() {}

struct [[clang::srcc_diagnose_unless_caller("Allowed")]] X { // expected-warning {{'clang::srcc_diagnose_unless_caller' attribute only applies to functions}}
  [[clang::srcc_diagnose_unless_caller("Allowed")]] int x; // expected-warning {{'clang::srcc_diagnose_unless_caller' attribute only applies to functions}}
  void f([[clang::srcc_diagnose_unless_caller("Allowed")]] int y) { // expected-warning {{'clang::srcc_diagnose_unless_caller' attribute only applies to functions}}
    [[clang::srcc_diagnose_unless_caller("Allowed")]]; // expected-error {{'clang::srcc_diagnose_unless_caller' attribute cannot be applied to a statement}}
    [[clang::srcc_diagnose_unless_caller("Allowed")]] int x; // expected-warning {{'clang::srcc_diagnose_unless_caller' attribute only applies to functions}}
  }
};

struct S {
  [[clang::srcc_diagnose_unless_caller("Allowed")]]
  void f1() {}

  [[clang::srcc_diagnose_unless_caller("Allowed")]]
  static void f2() {}

  [[clang::srcc_diagnose_unless_caller("Allowed")]]
  void f3(this auto&&) {}

  template <typename>
  [[clang::srcc_diagnose_unless_caller("Allowed")]]
  void t() {}
};

void g() {
  f(); // expected-error {{function 'f' must not be called directly; use it only to implement 'Allowed'}}
  S().f1(); // expected-error {{function 'f1' must not be called directly; use it only to implement 'Allowed'}}
  S::f2(); // expected-error {{function 'f2' must not be called directly; use it only to implement 'Allowed'}}
  S().f3(); // expected-error {{function 'f3' must not be called directly; use it only to implement 'Allowed'}}
  S().t<int>(); // expected-error {{function 't' must not be called directly; use it only to implement 'Allowed'}}
}

void Allowed() {
  f();
  S().f1();
  S::f2();
  S().f3();
  S().t<int>();
}

struct M {
  void g() {
    f(); // expected-error {{function 'f' must not be called directly; use it only to implement 'Allowed'}}
    S().f1(); // expected-error {{function 'f1' must not be called directly; use it only to implement 'Allowed'}}
    S::f2(); // expected-error {{function 'f2' must not be called directly; use it only to implement 'Allowed'}}
    S().f3(); // expected-error {{function 'f3' must not be called directly; use it only to implement 'Allowed'}}
    S().t<int>(); // expected-error {{function 't' must not be called directly; use it only to implement 'Allowed'}}
  }

  void Allowed() {
    f();
    S().f1();
    S::f2();
    S().f3();
    S().t<int>();
  }
};

namespace A {
void g() {
  f(); // expected-error {{function 'f' must not be called directly; use it only to implement 'Allowed'}}
  S().f1(); // expected-error {{function 'f1' must not be called directly; use it only to implement 'Allowed'}}
  S::f2(); // expected-error {{function 'f2' must not be called directly; use it only to implement 'Allowed'}}
  S().f3(); // expected-error {{function 'f3' must not be called directly; use it only to implement 'Allowed'}}
  S().t<int>(); // expected-error {{function 't' must not be called directly; use it only to implement 'Allowed'}}
}

void Allowed() {
  f();
  S().f1();
  S::f2();
  S().f3();
  S().t<int>();
}
}

namespace B {
template <typename>
void g() {
  f(); // expected-error 3 {{function 'f' must not be called directly; use it only to implement 'Allowed'}}
  S().f1(); // expected-error 3 {{function 'f1' must not be called directly; use it only to implement 'Allowed'}}
  S::f2(); // expected-error 3 {{function 'f2' must not be called directly; use it only to implement 'Allowed'}}
  S().f3(); // expected-error 3 {{function 'f3' must not be called directly; use it only to implement 'Allowed'}}
  S().t<int>(); // expected-error 3 {{function 't' must not be called directly; use it only to implement 'Allowed'}}
}

template void g<int>(); // expected-note {{in instantiation of}}
template void g<long>(); // expected-note {{in instantiation of}}

template <typename>
void Allowed() {
  f();
  S().f1();
  S::f2();
  S().f3();
  S().t<int>();
}

template void Allowed<int>();
template void Allowed<long>();
}
