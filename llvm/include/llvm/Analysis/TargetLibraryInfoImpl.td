//===-- TargetLibraryInfoImpl.td - File that describes library functions --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/TableGen/SetTheory.td"

// Return type or argument type of library function.
class FuncArgType {
  string Name = NAME;
}

// Recognized types of library function arguments and return types.
def Void : FuncArgType;
def Bool : FuncArgType;     // 8 bits on all targets
def Int16 : FuncArgType;
def Int32 : FuncArgType;
def Int : FuncArgType;
def IntPlus : FuncArgType;  // Int or bigger.
def Long : FuncArgType;     // Either 32 or 64 bits.
def IntX : FuncArgType;     // Any integer type.
def Int64 : FuncArgType;
def LLong : FuncArgType;    // 64 bits on all targets.
def SizeT : FuncArgType;    // size_t.
def SSizeT : FuncArgType;   // POSIX ssize_t.
def Flt : FuncArgType;      // IEEE float.
def Dbl : FuncArgType;      // IEEE double.
def LDbl : FuncArgType;     // Any floating type (TODO: tighten this up).
def Floating : FuncArgType; // Any floating type.
def Ptr : FuncArgType;      // Any pointer type.
def Struct : FuncArgType;   // Any struct type.
def Ellip : FuncArgType;    // The ellipsis (...).
def Same : FuncArgType;     // Same argument type as the previous one.


// Definition of library function.
class TargetLibCall<string Str, FuncArgType RetType,
                    list<FuncArgType> ArgTypes = []> {
  string Name = NAME;
  string String = Str;
  FuncArgType ReturnType = RetType;
  list<FuncArgType> ArgumentTypes = ArgTypes;
}

// Predicate for whether a libcall should be used for the current target.
class TargetLibCallPredicate<code cond> {
  code Cond = cond;
}
def AlwaysAvailable : TargetLibCallPredicate<[{}]>;

class TargetLibCallCustomName<TargetLibCall LibCall, string Name> {
  TargetLibCall Provides = LibCall;
  string CustomName = Name;
}

// Define a set of TargetLibCall / TargetLibCallCustomName
// with a predicate.
class TargetLibCalls<dag FuncList,
                     TargetLibCallPredicate Pred = AlwaysAvailable> {
  TargetLibCallPredicate AvailabilityPredicate = Pred;
  dag MemberList = FuncList;
}

// Define sets of libcalls to set available/unavailable/availableWithCustomName
// for a target.
// When IsAvailable is true, "TLI.disableAllFunctions();" will be called at first.
class TargetLibrary<TargetLibCallPredicate Pred, dag FuncList, dag NameList> {
  TargetLibCallPredicate TriplePred = Pred;
  TargetLibCalls LibCallList = TargetLibCalls<FuncList>;
  TargetLibCalls CustomNameList = TargetLibCalls<NameList>;
  bit IsAvailable = false;
}
