// RUN: %clang_cc1 -std=c++2c -verify -fsyntax-only %s

contract_assert; // expected-error {{expected unqualified-id}}
contract_assert(true); // expected-error {{expected unqualified-id}}

void f() {
  int x;

  contract_assert; // expected-error {{expected '(' after 'contract_assert'}}
  contract_assert(; // expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  contract_assert(true; // expected-error {{expected ')'}} expected-note {{to match this '('}}
  contract_assert(); // expected-error {{expected expression}}
  contract_assert(x = 2); // expected-error {{parentheses are required}}
  contract_assert(1, 2); // expected-error {{parentheses are required}}

  contract_assert(true);
  contract_assert(1 + 2);
  contract_assert((x = 2));
}