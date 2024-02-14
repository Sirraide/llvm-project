//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <contract>
#include <cstdio>
#include <cstdlib>

_LIBCPP_NORETURN void handle_contract_violation(const std::contracts::contract_violation& violation) {
  const bool exception        = violation.detection_mode() == std::contracts::detection_mode::evaluation_exception;
  const auto violation_string = [&]() {
    using k = std::contracts::contract_kind;
    switch (violation.kind()) {
    case k::pre:
      return "precondition";
    case k::post:
      return "postcondition";
    case k::assert:
      return "contract assertion";
    }
    return "<invalid>";
  };

  auto sloc = violation.location();
  std::fprintf(stderr, "%s:%d: in function %s\n", sloc.file_name(), sloc.line(), sloc.function_name());
  if (exception)
    std::fprintf("  evaluation of %s '%s' threw an exception\n", violation_string(), violation.comment());
  else
    std::fprintf("  %s '%s' evaluated to 'false'\n", violation_string(), violation.comment());

  std::abort();
}
