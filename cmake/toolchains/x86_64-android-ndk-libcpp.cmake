if(DRACO_CMAKE_TOOLCHAINS_X86_64_ANDROID_NDK_LIBCPP_CMAKE_)
  return()
endif()
set(DRACO_CMAKE_TOOLCHAINS_X86_64_ANDROID_NDK_LIBCPP_CMAKE_ 1)

include("${CMAKE_CURRENT_LIST_DIR}/../util.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/android-ndk-common.cmake")

set_variable_if_unset(ANDROID_PLATFORM android-21)
set_variable_if_unset(ANDROID_ABI x86_64)

include("${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake")