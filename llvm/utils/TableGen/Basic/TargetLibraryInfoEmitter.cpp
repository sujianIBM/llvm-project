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

static StringRef getEnumName(const Record *Libcall) {
  return Libcall->getValueAsString("LibFuncEnumName");
}

TargetLibraryInfoEmitter::TargetLibraryInfoEmitter(const RecordKeeper &R)
    : Records(R) {
  ArrayRef<const Record *> All =
      Records.getAllDerivedDefinitions("RuntimeLibcall");
  // AllTargetLibcalls.append(All.begin(), All.end());

  for (const Record *R : All) {
    StringRef EnumName = getEnumName(R);
    if (!EnumName.empty() && !EnumName.contains("_none_enum"))
      AllTargetLibcalls.push_back(R);
  }

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

  ArrayRef<const Record *> AllLibs =
      Records.getAllDerivedDefinitions("TLILibrary");
  DenseMap<const Record *, const Record *> Libcall2Impl;

  size_t NumEl = AllTargetLibcalls.size() + 1;

  auto getFuncName = [&Libcall2Impl](const Record *Libcall) -> StringRef {
    auto It = Libcall2Impl.find(Libcall);
    if (It == Libcall2Impl.end())
      return "";
    return It->second->getValueAsString("LibCallFuncName");
  };

  {
    IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_STRING_TABLE"); 

    OS << "void TargetLibraryInfoImpl::setTargetLibraryLibcallSets("
          "const Triple &TT) {\n";
    for (const Record *Lib : AllLibs) {

      const Record *Pred = Lib->getValueAsDef("TriplePred");
      OS.indent(2) << "if (" << Pred->getValueAsString("Cond") << ") {\n";

      SetTheory ST;
      SetTheory::RecSet Impls;
      ST.evaluate(Lib->getValueInit("MemberList"), Impls, Lib->getLoc());

      Libcall2Impl.clear();
      for (const Record *LibcallImpl : Impls) {
        const Record *Libcall = LibcallImpl->getValueAsDef("Provides");
        auto [It, Inserted] = Libcall2Impl.insert({Libcall, LibcallImpl});
        if (!Inserted)
          PrintError(Lib, "Libcall " + Libcall->getValueAsString("Name") +
                          " has more than one Impl");
      }

      llvm::StringToOffsetTable Table(
          /*AppendZero=*/true,
          /*ClassPrefix=*/"", /*UsePrefixForStorageMember=*/false);
      for (const auto *R : AllTargetLibcalls)
        Table.GetOrAddStringOffset(getFuncName(R));

      OS.indent(4)
          << "static constexpr char StandardNamesStrTableStorage[] =\n";
      Table.EmitString(OS);
      OS << ";\n";
      OS.indent(4) << "StandardNamesStrTable = "
          << "llvm::StringTable(StandardNamesStrTableStorage);\n";
      OS.indent(4) << "StandardNamesOffsets = {\n";
      OS.indent(6) << "0, //\n";
      for (const auto *R : AllTargetLibcalls) {
        StringRef Str = getFuncName(R);
        OS.indent(6) << Table.GetStringOffset(Str) << ", // " << Str << "\n";
      }
      OS.indent(4) << "};\n";
      OS.indent(4) << "StandardNamesSizeTable = {\n";
      OS.indent(4) << "  0,\n";
      for (const auto *R : AllTargetLibcalls)
        OS.indent(6) << getFuncName(R).size() << ",\n";
      OS.indent(4) << "};\n";

      OS.indent(4) << "return;\n";
      OS.indent(2) << "}\n";  // end of if (...)
    }
    OS.indent(2) << "return;\n";
    OS << "}\n";  // end of setTargetLibraryLibcallSets()
  }

  IfDefEmitter IfDef(OS, "GET_TARGET_LIBRARY_INFO_IMPL_DECL");
  OS << "LLVM_ABI llvm::StringTable StandardNamesStrTable{\"\\0\"};\n";
  OS << "LLVM_ABI std::array<llvm::StringTable::Offset, "
     << NumEl << "> StandardNamesOffsets;\n";
  OS << "LLVM_ABI std::array<uint8_t, " << NumEl
     << "> StandardNamesSizeTable;\n";
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
