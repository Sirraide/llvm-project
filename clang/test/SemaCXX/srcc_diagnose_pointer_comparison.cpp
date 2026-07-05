// RUN: %clang_cc1 -verify -fsyntax-only %s

struct [[clang::srcc_diagnose_pointer_comparison]] S {};
struct Derived : S {};

void f1(
  void* a, void* b,
  S* c, S* d,
  const S* e, const S* f,
  Derived* g
) {
  (void)(a == b);
  (void)(a == c); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(c == a); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(c == d); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(a == e); // expected-error {{pointer comparison involving type 'const S *' is disabled}}
  (void)(e == a); // expected-error {{pointer comparison involving type 'const S *' is disabled}}
  (void)(e == f); // expected-error {{pointer comparison involving type 'const S *' is disabled}}
  (void)(c == e); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(g == a); // expected-error {{pointer comparison involving type 'Derived *' is disabled}}
  (void)(a == g); // expected-error {{pointer comparison involving type 'Derived *' is disabled}}
  (void)(c == g); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(g == c); // expected-error {{pointer comparison involving type 'Derived *' is disabled}}
}

struct [[clang::srcc_diagnose_pointer_comparison]] Incomplete;
void f2(void* a, Incomplete* b) {
  (void)(a == b); // expected-error {{pointer comparison involving type 'Incomplete *' is disabled}}
  (void)(b == a); // expected-error {{pointer comparison involving type 'Incomplete *' is disabled}}
}
