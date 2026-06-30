; RUN: llc -mtriple s390x-ibm-zos < %s | FileCheck %s

; CHECK: * XPLINK Routine Layout Entry
; CHECK: L#EPM_void_test_0 DS 0H
; CHECK: * Eyecatcher 0x00C300C500C500
; CHECK:  DC XL7'00C300C500C500'
; CHECK: * Mark Type C'1'
; CHECK:  DC XL1'F1'
; CHECK: * Offset to PPA1
; CHECK:  DC AD(L#PPA1_void_test_0-L#EPM_void_test_0)
; CHECK: * DSA Size 0x0
; CHECK: * Entry Flags
; CHECK: *   Bit 1: 1 = Leaf function
; CHECK: *   Bit 2: 0 = Does not use alloca
; CHECK:  DC XL4'00000008'
; CHECK:  ENTRY void_test
; CHECK: L#func_end0 DS 0H
; CHECK: * PPA1
; CHECK: L#PPA1_void_test_0 DS 0H
; CHECK: * Version
; CHECK:  DC XL1'02'
; CHECK: * LE Signature X'CE'
; CHECK:  DC XL1'CE'
; CHECK: * Saved GPR Mask
; CHECK:  DC XL2'0000'
; CHECK: * Offset to PPA2
; CHECK:  DC AD(L#PPA2-L#PPA1_void_test_0)
; CHECK: * PPA1 Flags 1
; CHECK: *   Bit 0: 1 = 64-bit DSA
; CHECK:  DC XL1'80'
; CHECK: * PPA1 Flags 2
; CHECK: *   Bit 0: 1 = External procedure
; CHECK: *   Bit 3: 0 = STACKPROTECT is not enabled
; CHECK:  DC XL1'80'
; CHECK: * PPA1 Flags 3
; CHECK:  DC XL1'00'
; CHECK: * PPA1 Flags 4
; CHECK: *   Bit 7: 1 = Name Length and Name
; CHECK:  DC XL1'81'
; CHECK: * Length/4 of Parms
; CHECK:  DC XL2'0000'
; CHECK: * Length of Code
; CHECK:  DC AD(L#void_test_end_0-L#EPM_void_test_0)
; CHECK: * Length of Name
; CHECK:  DC XL2'0009'
; CHECK: * Name of Function
; CHECK:  DC XL9'A59689846DA385A2A3'
; CHECK: DC AD(L#EPM_void_test_0-L#PPA1_void_test_0)
define void @void_test() {
entry:
  ret void
}

; Attribute "zos-ppa1-name"="no-emit" removes the function name from PPA1.
; CHECK: * PPA1
; CHECK-NEXT: L#PPA1_void_test_no_name_0 DS 0H
; CHECK:      * PPA1 Flags 4
; CHECK-NEXT:  DC XL1'80'
; CHECK-NEXT: * Length/4 of Parms
; CHECK-NEXT:  DC XL2'0000'
; CHECK-NEXT: * Length of Code
; CHECK-NEXT:  DC AD(L#void_test_no_name_end_0-L#EPM_void_test_no_name_0)
; CHECK-NEXT:  DC AD(L#EPM_void_test_no_name_0-L#PPA1_void_test_no_name_0)
define void @void_test_no_name() #0 {
entry:
  ret void
}
attributes #0 = { "zos-ppa1-name"="no-emit" }

; Attribute minsize removes the function name from PPA1.
; CHECK: * PPA1
; CHECK-NEXT: L#PPA1_void_test_minsize_0 DS 0H
; CHECK:      * PPA1 Flags 4
; CHECK-NEXT:  DC XL1'80'
; CHECK-NEXT: * Length/4 of Parms
; CHECK-NEXT:  DC XL2'0000'
; CHECK-NEXT: * Length of Code
; CHECK-NEXT:  DC AD(L#void_test_minsize_end_0-L#EPM_void_test_minsize_0)
; CHECK-NEXT:  DC AD(L#EPM_void_test_minsize_0-L#PPA1_void_test_minsize_0)
define void @void_test_minsize() #1 {
entry:
  ret void
}
attributes #1 = { minsize }

; Attribute "zos-ppa1-name"="emit" takes precedence over minsize,
; and thus emits the function name in PPA1.
; CHECK: * PPA1
; CHECK-NEXT: L#PPA1_void_test_name_0 DS 0H
; CHECK: * PPA1 Flags 4
; CHECK-NEXT: *   Bit 7: 1 = Name Length and Name
; CHECK-NEXT:  DC XL1'81'
; CHECK-NEXT: * Length/4 of Parms
; CHECK-NEXT:  DC XL2'0000'
; CHECK-NEXT: * Length of Code
; CHECK-NEXT:  DC AD(L#void_test_name_end_0-L#EPM_void_test_name_0)
; CHECK-NEXT: * Length of Name
; CHECK-NEXT:  DC XL2'000E'
; CHECK-NEXT: * Name of Function
; CHECK-NEXT:  DC XL14'A59689846DA385A2A36D95819485'
; CHECK-NEXT: DC AD(L#EPM_void_test_name_0-L#PPA1_void_test_name_0)
define void @void_test_name() #2 {
entry:
  ret void
}
attributes #2 = { "zos-ppa1-name"="emit" minsize }
