//===- TargetLibraryInfoEmitter.cpp - Properties from TargetLibraryInfo.td ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SequenceToOffsetTable.h"
#include "TargetLibraryInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/CodeGenHelpers.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/SetTheory.h"
#include "llvm/TableGen/StringToOffsetTable.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <cassert>
#include <cstddef>

#define DEBUG_TYPE "target-library-info-emitter"

using namespace llvm;

namespace {
class TargetLibraryInfoEmitter {
private:
  const RecordKeeper &Records;
  SmallVector<const Record *, 1024> AllTargetLibcalls;

private:
  void emitTargetLibraryInfoEnum(raw_ostream &OS) const;
  void emitTargetLibraryInfoStringTable(raw_ostream &OS) const;
  void emitTargetLibraryInfoSignatureTable(raw_ostream &OS) const;
  void emitTargetLibraryInfoInitializeLibCalls(raw_ostream &OS) const;

public:
  TargetLibraryInfoEmitter(const RecordKeeper &R);

  void run(raw_ostream &OS);
};

} // End anonymous namespace.

TargetLibraryInfoEmitter::TargetLibraryInfoEmitter(const RecordKeeper &R)
    : Records(R) {
  ArrayRef<const Record *> All =
      Records.getAllDerivedDefinitions("TargetLibCall");
  AllTargetLibcalls.append(All.begin(), All.end());
  // Make sure that the records are in the same order as the input.
  // TODO Find a better sorting order when all is migrated.
  sort(AllTargetLibcalls, [](const Record *A, const Record *B) {
    return A->getID() < B->getID();
  });
}

// Emits the LibFunc enumeration, which is an abstract name for each library
// function.
void TargetLibraryInfoEmitter::emitTargetLibraryInfoEnum(
    raw_ostream &OS) const {
  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_ENUM");
  OS << "enum LibFunc : unsigned {\n";
  OS.indent(2) << "NotLibFunc = 0,\n";
  for (const auto *R : AllTargetLibcalls)
    OS.indent(2) << "LibFunc_" << R->getName() << ",\n";
  OS.indent(2) << "NumLibFuncs,\n";
  OS.indent(2) << "End_LibFunc = NumLibFuncs,\n";
  if (AllTargetLibcalls.size()) {
    OS.indent(2) << "Begin_LibFunc = LibFunc_"
                 << AllTargetLibcalls[0]->getName() << ",\n";
  } else {
    OS.indent(2) << "Begin_LibFunc = NotLibFunc,\n";
  }
  OS << "};\n";
}

// The names of the functions are stored in a long string, along with support
// tables for accessing the offsets of the function names from the beginning of
// the string.
void TargetLibraryInfoEmitter::emitTargetLibraryInfoStringTable(
    raw_ostream &OS) const {
  llvm::StringToOffsetTable Table(
      /*AppendZero=*/true,
      "TargetLibraryInfoImpl::", /*UsePrefixForStorageMember=*/false);
  for (const auto *R : AllTargetLibcalls)
    Table.GetOrAddStringOffset(R->getValueAsString("String"));

  size_t NumEl = AllTargetLibcalls.size() + 1;

  {
    IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_STRING_TABLE");
    Table.EmitStringTableDef(OS, "StandardNamesStrTable");
    OS << "\n";
    OS << "const llvm::StringTable::Offset "
          "TargetLibraryInfoImpl::StandardNamesOffsets["
       << NumEl
       << "] = "
          "{\n";
    OS.indent(2) << "0, //\n";
    for (const auto *R : AllTargetLibcalls) {
      StringRef Str = R->getValueAsString("String");
      OS.indent(2) << Table.GetStringOffset(Str) << ", // " << Str << "\n";
    }
    OS << "};\n";
    OS << "const uint8_t TargetLibraryInfoImpl::StandardNamesSizeTable["
       << NumEl << "] = {\n";
    OS << "  0,\n";
    for (const auto *R : AllTargetLibcalls)
      OS.indent(2) << R->getValueAsString("String").size() << ",\n";
    OS << "};\n";
  }

  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_IMPL_DECL");
  OS << "LLVM_ABI static const llvm::StringTable StandardNamesStrTable;\n";
  OS << "LLVM_ABI static const llvm::StringTable::Offset StandardNamesOffsets["
     << NumEl << "];\n";
  OS << "LLVM_ABI static const uint8_t StandardNamesSizeTable[" << NumEl
     << "];\n";
}

// Since there are much less type signatures then library functions, the type
// signatures are stored reusing existing entries. To access a table entry, an
// offset table is used.
void TargetLibraryInfoEmitter::emitTargetLibraryInfoSignatureTable(
    raw_ostream &OS) const {
  SmallVector<const Record *, 1024> FuncTypeArgs(
      Records.getAllDerivedDefinitions("FuncArgType"));

  // Sort the records by ID.
  sort(FuncTypeArgs, [](const Record *A, const Record *B) {
    return A->getID() < B->getID();
  });

  using Signature = std::vector<StringRef>;
  SequenceToOffsetTable<Signature> SignatureTable("NoFuncArgType");
  auto GetSignature = [](const Record *R) -> Signature {
    const auto *Tys = R->getValueAsListInit("ArgumentTypes");
    Signature Sig;
    Sig.reserve(Tys->size() + 1);
    const Record *RetType = R->getValueAsOptionalDef("ReturnType");
    if (RetType)
      Sig.push_back(RetType->getName());
    for (unsigned I = 0, E = Tys->size(); I < E; ++I) {
      Sig.push_back(Tys->getElementAsRecord(I)->getName());
    }
    return Sig;
  };
  Signature NoFuncSig({StringRef("Void")});
  SignatureTable.add(NoFuncSig);
  for (const auto *R : AllTargetLibcalls)
    SignatureTable.add(GetSignature(R));
  SignatureTable.layout();

  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_SIGNATURE_TABLE");
  OS << "enum FuncArgTypeID : char {\n";
  OS.indent(2) << "NoFuncArgType = 0,\n";
  for (const auto *R : FuncTypeArgs)
    OS.indent(2) << R->getName() << ",\n";
  OS << "};\n";
  OS << "static const FuncArgTypeID SignatureTable[] = {\n";
  SignatureTable.emit(OS, [](raw_ostream &OS, StringRef E) { OS << E; });
  OS << "};\n";
  OS << "static const uint16_t SignatureOffset[] = {\n";
  OS.indent(2) << SignatureTable.get(NoFuncSig) << ", //\n";
  for (const auto *R : AllTargetLibcalls) {
    OS.indent(2) << SignatureTable.get(GetSignature(R)) << ", // "
                 << R->getName() << "\n";
  }
  OS << "};\n";
}

void TargetLibraryInfoEmitter::emitTargetLibraryInfoInitializeLibCalls(
    raw_ostream &OS) const {
  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_INIT");

  SmallVector<const Record *, 1024> AllLibraries(
      Records.getAllDerivedDefinitions("TargetLibrary"));
  sort(AllLibraries, [](const Record *A, const Record *B) {
    return A->getID() < B->getID();
  });
  const Record *AlwaysAvailable = Records.getDef("AlwaysAvailable");

  for (const Record *R : AllLibraries) {
    AvailabilityPred TopLevelPredicate(R->getValueAsDef("TopPred"));

    unsigned IndentDepth = 2;
    if (!TopLevelPredicate.isAlwaysAvailable()) {
      OS << indent(IndentDepth);
      TopLevelPredicate.emitIf(OS);
      IndentDepth += 2;
    }

    bool DisableAll = R->getValueAsBit("DisableAll");
    if (DisableAll) {
      OS << indent(IndentDepth)
         << "TLI.disableAllFunctions();\n";
    }

    SetTheory Sets;
    DenseMap<const Record *, std::vector<const Record *>> Libcall2Pred;
    Sets.addExpander("TargetLibCalls",
                     std::make_unique<TargetLibcallPredicateExpander>(
                         Libcall2Pred));

    std::array<StringRef, 3> FieldNames = {
      "AvailableList",
      "UnAvailableList",
      "CustomNameList"
    };

    DenseMap<const Record *,
             std::array<std::vector<const Record *>, FieldNames.size()>>
    Pred2Libcalls;
    SetVector<const Record *> PredicateSet;

    for (unsigned idx = 0; idx < FieldNames.size(); ++idx) {
      Libcall2Pred.clear();
      const SetTheory::RecVec *Libcalls =
          Sets.expand(R->getValueAsDef(FieldNames[idx]));

      if (!Libcalls)
        continue;

      for (const Record *Libcall : *Libcalls) {
        auto It = Libcall2Pred.find(Libcall);
        if (It == Libcall2Pred.end()) {
          auto &Target = Pred2Libcalls[AlwaysAvailable];
          Target[idx].push_back(Libcall);
          PredicateSet.insert(AlwaysAvailable);
        } else {
          for (const Record *Pred : It->second) {
            auto &Target = Pred2Libcalls[Pred];
            Target[idx].push_back(Libcall);
            PredicateSet.insert(Pred);
          }
        }
      }
    }

    SmallVector<const Record *, 0> Predicates = PredicateSet.takeVector();
    for (const Record *Pred : Predicates) {
      auto It = Pred2Libcalls.find(Pred);

      AvailabilityPred SubsetPred(Pred);
      if (!SubsetPred.isAlwaysAvailable()) {
        OS << indent(IndentDepth);
        SubsetPred.emitIf(OS);
        IndentDepth += 2;
      }

      // emit TLI.setAvailable(LibFunc_xxx);
      for (const Record *R : It->second[0]) {
        OS << indent(IndentDepth)
           << "TLI.setAvailable(LibFunc_" << R->getName() << ");\n";
      }

      // TLI.setUnavailable(LibFunc_xxx);
      for (const Record *R : It->second[1]) {
        OS << indent(IndentDepth)
           << "TLI.setUnavailable(LibFunc_" << R->getName() << ");\n";
      }

      // emit TLI.setAvailableWithName(LibFunc_xxx, "xxx");
      for (const Record *R : It->second[2]) {
        const Record *Provides = R->getValueAsDef("Provides");
        if (!Provides)
          PrintError(R, "TargetLibCallCustomName does not have a TargetLibCall");
        else {
          OS << indent(IndentDepth)
             << "TLI.setAvailableWithName(LibFunc_" << Provides->getName()
             << ", " << "\"" << R->getValueAsString("CustomName") << "\");\n";
        }
      }

      if (!SubsetPred.isAlwaysAvailable()) {
        IndentDepth -= 2;
        OS << indent(IndentDepth);
        SubsetPred.emitEndIf(OS);
      }
    }

    if (!TopLevelPredicate.isAlwaysAvailable()) {
      IndentDepth -= 2;
      OS << indent(IndentDepth);
      TopLevelPredicate.emitEndIf(OS);
    }
    OS << '\n';
  }
}

void TargetLibraryInfoEmitter::run(raw_ostream &OS) {
  emitSourceFileHeader("Target Library Info Source Fragment", OS, Records);

  emitTargetLibraryInfoEnum(OS);
  emitTargetLibraryInfoStringTable(OS);
  emitTargetLibraryInfoSignatureTable(OS);
  emitTargetLibraryInfoInitializeLibCalls(OS);
}

static TableGen::Emitter::OptClass<TargetLibraryInfoEmitter>
    X("gen-target-library-info", "Generate TargetLibraryInfo");
