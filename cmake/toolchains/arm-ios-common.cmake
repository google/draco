if (NOT DRACO_CMAKE_TOOLCHAINS_ARM_IOS_COMMON_CMAKE_)
set(DRACO_CMAKE_ARM_IOS_COMMON_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Darwin")
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_ARG1 "-arch ${CMAKE_SYSTEM_PROCESSOR}")
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_ARG1 "-arch ${CMAKE_SYSTEM_PROCESSOR}")

# Assembler sources must be converted for ARM iOS targets.
set(AOM_ADS2GAS_REQUIRED 1)
set(AOM_ADS2GAS "${CMAKE_CURRENT_SOURCE_DIR}/build/make/ads2gas_apple.pl")
set(AOM_GAS_EXT "S")

# No runtime cpu detect for arm*-ios targets.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE NUMBER "")

# TODO(tomfinegan): Handle bit code embedding.

endif ()  # DRACO_CMAKE_TOOLCHAINS_ARM_IOS_COMMON_CMAKE_
