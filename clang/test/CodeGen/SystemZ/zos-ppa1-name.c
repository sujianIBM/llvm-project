// test default
// RUN: %clang_cc1 -triple s390x-ibm-zos -emit-llvm %s -o -\
// RUN:   | FileCheck %s -check-prefix=PPA1-NAME

// test the positive and negative options
// RUN: %clang_cc1 -triple s390x-ibm-zos -mzos-ppa1-name -emit-llvm %s -o -\
// RUN:   | FileCheck %s -check-prefix=PPA1-NAME
// RUN: %clang_cc1 -triple s390x-ibm-zos -mno-zos-ppa1-name -emit-llvm %s -o -\
// RUN:   | FileCheck %s -check-prefix=NO-PPA1-NAME

// test -Oz implies -mno-zos-ppa1-name
// RUN: %clang_cc1 -triple s390x-ibm-zos -Oz -emit-llvm %s -o -\
// RUN:   | FileCheck %s -check-prefix=NO-PPA1-NAME

// PPA1-NAME-NOT: attributes #0 = {{{.*}}"zos-ppa1-no-name"{{.*}}}
// NO-PPA1-NAME: attributes #0 = {{{.*}}"zos-ppa1-no-name"{{.*}}}

int main() {
  return 0;
}
