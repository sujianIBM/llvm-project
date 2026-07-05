set(LLVM_TARGET_DEFINITIONS ../IR/RuntimeLibcalls.td)
tablegen(LLVM TargetLibraryInfo.inc -gen-target-library-info)
add_public_tablegen_target(analysis_gen)
