if(DRACO_CMAKE_TOOLCHAINS_ARM64_ANDROID_NDK_LIBCPP_CMAKE_)
  return()
endif()
set(DRACO_CMAKE_TOOLCHAINS_ARM64_ANDROID_NDK_LIBCPP_CMAKE_ 1)

include("${CMAKE_CURRENT_LIST_DIR}/../util.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/android-ndk-common.cmake")

set_variable_if_unset(ANDROID_PLATFORM android-21)
set_variable_if_unset(ANDROID_ABI arm64-v8a)

include("${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake")