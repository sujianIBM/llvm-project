//===- TargetLibraryInfoEmitter.cpp - Properties from TargetLibraryInfo.td ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SequenceToOffsetTable.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/CodeGenHelpers.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/SetTheory.h"
#include "llvm/TableGen/StringToOffsetTable.h"
#include "llvm/TableGen/TableGenBackend.h"
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

public:
  TargetLibraryInfoEmitter(const RecordKeeper &R);

  void run(raw_ostream &OS);
};

} // End anonymous namespace.

TargetLibraryInfoEmitter::TargetLibraryInfoEmitter(const RecordKeeper &R)
    : Records(R) {
/*
  ArrayRef<const Record *> All =
      Records.getAllDerivedDefinitions("TargetLibCall");
  AllTargetLibcalls.append(All.begin(), All.end());
  // Make sure that the records are in the same order as the input.
  // TODO Find a better sorting order when all is migrated.
  sort(AllTargetLibcalls, [](const Record *A, const Record *B) {
    return A->getID() < B->getID();
  });
*/

  ArrayRef<const Record *> AllLibs =
      Records.getAllDerivedDefinitions("TLILibrary");

  // There is only one library now.
  // TODO: use the predicate.
  for (const Record *Lib : AllLibs) {

    SetTheory ST;
    SetTheory::RecSet Impls;
    ST.evaluate(Lib->getValueInit("MemberList"), Impls, Lib->getLoc());

    for (const Record *LibcallImpl : Impls) {
      const Record *Libcall = LibcallImpl->getValueAsDef("Provides");
      StringRef EnumName = Libcall->getValueAsString("LibFuncEnumName");
      if (!EnumName.empty() && !EnumName.contains("_none_enum"))
        AllTargetLibcalls.push_back(LibcallImpl);
    }

    sort(AllTargetLibcalls, [](const Record *A, const Record *B) {
      return A->getID() < B->getID();
    });
  }
}

static StringRef getEnumName(const Record *R) {
  R = R->getValueAsDef("Provides");
  return R->getValueAsString("LibFuncEnumName");
}

// Emits the LibFunc enumeration, which is an abstract name for each library
// function.
void TargetLibraryInfoEmitter::emitTargetLibraryInfoEnum(
    raw_ostream &OS) const {

  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_ENUM");
  OS << "enum LibFunc : unsigned {\n";
  OS.indent(2) << "NotLibFunc = 0,\n";
  for (const auto *R : AllTargetLibcalls)
    OS.indent(2) << "LibFunc_" << getEnumName(R) << ",\n";
  OS.indent(2) << "NumLibFuncs,\n";
  OS.indent(2) << "End_LibFunc = NumLibFuncs,\n";
  if (AllTargetLibcalls.size()) {
    OS.indent(2) << "Begin_LibFunc = LibFunc_"
                 << getEnumName(AllTargetLibcalls[0]) << ",\n";
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
    Table.GetOrAddStringOffset(R->getValueAsString("LibCallFuncName"));

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
      StringRef Str = R->getValueAsString("LibCallFuncName");
      OS.indent(2) << Table.GetStringOffset(Str) << ", // " << Str << "\n";
    }
    OS << "};\n";
    OS << "const uint8_t TargetLibraryInfoImpl::StandardNamesSizeTable["
       << NumEl << "] = {\n";
    OS << "  0,\n";
    for (const auto *R : AllTargetLibcalls)
      OS.indent(2) << R->getValueAsString("LibCallFuncName").size() << ",\n";
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
    R = R->getValueAsDef("Provides");
    const auto *Tys = R->getValueAsListInit("ArgumentTypes");
    Signature Sig;
    Sig.reserve(Tys->size() + 1);
    const Record *RetType = R->getValueAsOptionalDef("ReturnType");
    if (RetType && (RetType->getName() != "NoneType"))
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
  for (const auto *R : FuncTypeArgs) {
    if (R->getName() == "NoneType")
      continue;
    OS.indent(2) << R->getName() << ",\n";
  }
  OS << "};\n";
  OS << "static const FuncArgTypeID SignatureTable[] = {\n";
  SignatureTable.emit(OS, [](raw_ostream &OS, StringRef E) { OS << E; });
  OS << "};\n";
  OS << "static const uint16_t SignatureOffset[] = {\n";
  OS.indent(2) << SignatureTable.get(NoFuncSig) << ", //\n";
  for (const auto *R : AllTargetLibcalls) {
    OS.indent(2) << SignatureTable.get(GetSignature(R)) << ", // "
                 << getEnumName(R) << "\n";
  }
  OS << "};\n";
}

void TargetLibraryInfoEmitter::run(raw_ostream &OS) {
  emitSourceFileHeader("Target Library Info Source Fragment", OS, Records);

  emitTargetLibraryInfoEnum(OS);
  emitTargetLibraryInfoStringTable(OS);
  emitTargetLibraryInfoSignatureTable(OS);
}

static TableGen::Emitter::OptClass<TargetLibraryInfoEmitter>
    X("gen-target-library-info", "Generate TargetLibraryInfo");
