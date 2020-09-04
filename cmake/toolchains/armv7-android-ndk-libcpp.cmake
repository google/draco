if(DRACO_CMAKE_TOOLCHAINS_ARMV7_ANDROID_NDK_LIBCPP_CMAKE_)
  return()
endif()
set(DRACO_CMAKE_TOOLCHAINS_ARMV7_ANDROID_NDK_LIBCPP_CMAKE_ 1)

include("${CMAKE_CURRENT_LIST_DIR}/../util.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/android-ndk-common.cmake")

set_variable_if_unset(ANDROID_PLATFORM android-18)
set_variable_if_unset(ANDROID_ABI armeabi-v7a)

include("${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake")
