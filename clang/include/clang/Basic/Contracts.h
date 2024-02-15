//===--- Contracts.h - Types for C++ Contracts ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines types used to describe C++ contracts.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_CONTRACTS_H
#define LLVM_CLANG_BASIC_CONTRACTS_H

namespace clang {

enum class ContractKind {
  // Do not change these, as they correspond to the enum
  // values mandated by the standard.
  Precondition = 0,
  Postcondition = 1,
  Assert = 2,
};
} // end namespace clang

#endif // LLVM_CLANG_BASIC_CONTRACTS_H
