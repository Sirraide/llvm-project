// RUN: %clang_cc1 -fsyntax-only -verify=disabled %s
// RUN: %clang_cc1 -fsyntax-only -verify=enabled -fexperimental-defer-ts %s

// disabled-no-diagnostics
int defer; // enabled-error {{expected unqualified-id}}
