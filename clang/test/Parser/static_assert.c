// RUN: %clang_cc1 -fsyntax-only -std=c17 -DTEST_SPELLING -Wc2x-compat -verify=c17 %s
// RUN: %clang_cc1 -fsyntax-only -std=c17 -DTEST_SPELLING -fms-compatibility -verify=c17-ms %s
// RUN: %clang_cc1 -fsyntax-only -std=c2x -Wpre-c2x-compat -verify=c2x-compat %s
// RUN: %clang_cc1 -fsyntax-only -std=c99 -verify=c99 %s
// RUN: %clang_cc1 -fsyntax-only -std=c99 -pedantic -verify=c99-pedantic %s
// RUN: %clang_cc1 -fsyntax-only -std=c++17 -verify=cxx17 -x c++ %s
// RUN: %clang_cc1 -fsyntax-only -std=c++17 -pedantic -verify=cxx17-pedantic -x c++ %s
// RUN: %clang_cc1 -fsyntax-only -std=c++98 -verify=cxx98 -x c++ %s
// RUN: %clang_cc1 -fsyntax-only -std=c++98 -pedantic -verify=cxx98-pedantic -x c++ %s
// RUN: %clang_cc1 -fsyntax-only -std=c++17 -Wpre-c++17-compat -verify=cxx17-compat -x c++ %s

// cxx17-no-diagnostics

#ifdef TEST_SPELLING
// Only test the C++ spelling in C mode in some of the tests, to reduce the
// amount of diagnostics to have to check. This spelling is allowed in MS-
// compatibility mode in C, but otherwise produces errors.
static_assert(1, ""); // c17-warning {{'static_assert' is a keyword in C23}} \
                      // c17-error {{expected parameter declarator}} \
                      // c17-error {{expected ')'}} \
                      // c17-note {{to match this '('}} \
                      // c17-error {{type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int}} \

#endif

// We support _Static_assert as an extension in older C modes and in all C++
// modes, but only as a pedantic warning.
_Static_assert(1, ""); // c99-pedantic-warning {{'_Static_assert' is a C11 extension}} \
                       // cxx17-pedantic-warning {{'_Static_assert' is a C11 extension}} \
                       // cxx98-pedantic-warning {{'_Static_assert' is a C11 extension}}

// _Static_assert without a message has more complex diagnostic logic:
//   * In C++17 or C2x mode, it's supported by default.
//   * But there is a special compat warning flag to warn about portability to
//     older standards.
//   * In older standard pedantic modes, warn about supporting without a
//     message as an extension.
_Static_assert(1); // c99-pedantic-warning {{'_Static_assert' with no message is a C23 extension}} \
                   // c99-warning {{'_Static_assert' with no message is a C23 extension}} \
                   // cxx98-pedantic-warning {{'static_assert' with no message is a C++17 extension}} \
                   // cxx98-warning {{'static_assert' with no message is a C++17 extension}} \
                   // c2x-compat-warning {{'_Static_assert' with no message is incompatible with C standards before C23}} \
                   // cxx17-compat-warning {{'static_assert' with no message is incompatible with C++ standards before C++17}} \
                   // c99-pedantic-warning {{'_Static_assert' is a C11 extension}} \
                   // cxx17-pedantic-warning {{'_Static_assert' is a C11 extension}} \
                   // cxx98-pedantic-warning {{'_Static_assert' is a C11 extension}} \
                   // c17-warning {{'_Static_assert' with no message is a C23 extension}} \
                   // c17-ms-warning {{'_Static_assert' with no message is a C23 extension}}
