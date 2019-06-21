if(DRACO_CMAKE_TOOLCHAINS_ARMV7_ANDROID_NDK_LIBCPP_CMAKE_)
  return()
endif()
set(DRACO_CMAKE_TOOLCHAINS_ARMV7_ANDROID_NDK_LIBCPP_CMAKE_ 1)

include("${CMAKE_CURRENT_LIST_DIR}/../util.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/android-ndk-common.cmake")

set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
set_variable_if_unset(CMAKE_SYSTEM_VERSION 18)
