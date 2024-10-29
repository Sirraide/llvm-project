// RUN: %clang_cc1 -fsyntax-only -verify -std=c++2c %s

constexpr int f() {
  int x = 0;
  template for (int i : {1}) x += i;
  template for (int i : {1, 2, 3, 4}) x += i;
  return x;
}

template <int ...xs>
constexpr int tf() {
  int x = 0;
  template for (int i : {xs...}) x += i;
  return x;
}

static_assert(tf<>() == 0);
static_assert(tf<1>() == 1);
static_assert(tf<1, 2, 3, 4>() == 10);
