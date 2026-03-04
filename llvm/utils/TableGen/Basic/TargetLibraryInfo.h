//===------------------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_COMMON_TARGETLIBRARYINFO_H
#define LLVM_UTILS_TABLEGEN_COMMON_TARGETLIBRARYINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/SetTheory.h"

namespace llvm {

class AvailabilityPred {
  const Record *TheDef = nullptr;
  StringRef PredicateString;

public:
  AvailabilityPred() = default;
  AvailabilityPred(const Record *Def) : TheDef(Def) {
    if (TheDef)
      PredicateString = TheDef->getValueAsString("Cond");
  }

  const Record *getDef() const { return TheDef; }
  bool isAlwaysAvailable() const { return PredicateString.empty(); }

  void emitIf(raw_ostream &OS) const {
    OS << "if (" << PredicateString << ") {\n";
  }
  void emitEndIf(raw_ostream &OS) const { OS << "}\n"; }
};

/// Used to apply predicates to nested sets of libcalls.
struct TargetLibcallPredicateExpander : SetTheory::Expander {
  // Only one Predicate is uspported for a Libcall on a target
  // Nested Predicates are not allowed
  DenseMap<const Record *, std::vector<const Record *>> &Libcall2Pred;

  TargetLibcallPredicateExpander(
      DenseMap<const Record *, std::vector<const Record *>> &Libcall2Pred)
      : Libcall2Pred(Libcall2Pred) {}

  void expand(SetTheory &ST, const Record *Def,
              SetTheory::RecSet &Elts) override;
};

} // namespace llvm

#endif // LLVM_UTILS_TABLEGEN_COMMON_TARGETLIBRARYINFO_H
