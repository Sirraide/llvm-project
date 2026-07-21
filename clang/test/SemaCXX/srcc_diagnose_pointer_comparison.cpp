// RUN: %clang_cc1 -verify -fsyntax-only %s

#define NULL ((void*)0)

using nullptr_t = decltype(nullptr);

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

// Comparisons against a nullptr literal are ok.
void nullptr_comparison(S* s) {
  using P = S*;

  (void)(s == nullptr);
  (void)(s == (nullptr));
  (void)(s == P(nullptr));
  (void)(s == static_cast<S*>(nullptr));
  (void)(s == (S*)(nullptr));

  (void)(nullptr == s);
  (void)((nullptr) == s);
  (void)(P(nullptr) == s);
  (void)(static_cast<S*>(nullptr) == s);
  (void)((S*)(nullptr) == s);

  (void)(s == NULL);
  (void)(s == (NULL));
  (void)(s == P(NULL));
  (void)(s == static_cast<S*>(NULL));
  (void)(s == reinterpret_cast<S*>(NULL));
  (void)(s == (S*)(NULL));

  (void)(NULL == s);
  (void)((NULL) == s);
  (void)(P(NULL) == s);
  (void)(static_cast<S*>(NULL) == s);
  (void)(reinterpret_cast<S*>(NULL) == s);
  (void)((S*)(NULL) == s);
}

// Ok, because 'T' might be nullptr_t.
template <typename T>
auto dependent_comparison(S* s, T t) {
  (void)(s == t);
  (void)(s == (t));
  (void)(s == (S*)t);
  (void)(s == static_cast<S*>(t));
}

template <typename T>
auto dependent_comparison2(S* s, T t) {
  (void)(s == t); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(s == (t)); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(s == (S*)t); // expected-error {{pointer comparison involving type 'S *' is disabled}}
  (void)(s == static_cast<S*>(t)); // expected-error {{pointer comparison involving type 'S *' is disabled}}
}

void f(S* s) {
  dependent_comparison(s, nullptr);
  dependent_comparison2(s, s); // expected-note {{in instantiation of}}
}
