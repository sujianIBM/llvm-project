//===------------------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/TableGen/Error.h"
#include "TargetLibraryInfo.h"

using namespace llvm;

void TargetLibcallPredicateExpander::expand(SetTheory &ST, const Record *Def,
                                            SetTheory::RecSet &Elts) {
  assert(Def->isSubClassOf("TargetLibCalls"));

  SetTheory::RecSet TmpElts;
  ST.evaluate(Def->getValueInit("MemberList"), TmpElts, Def->getLoc());
  Elts.insert(TmpElts.begin(), TmpElts.end());

  AvailabilityPred AP(Def->getValueAsDef("AvailabilityPredicate"));

  for (const Record *Libcall : TmpElts) {
    if (!AP.isAlwaysAvailable()) {
      auto [It, Inserted] = Libcall2Pred.insert({Libcall, {}});
      if (!Inserted) {
        StringRef FieldName = Libcall->isSubClassOf("TargetLibCall")
                              ? "Name" : "CustomName";
        PrintError(
            Def,
            "combining nested libcall set predicates currently unhandled: '" +
                Libcall->getValueAsString(FieldName) + "'");
      }
      It->second.push_back(AP.getDef());
    }
  }
}
