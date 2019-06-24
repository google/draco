if(DRACO_CMAKE_TOOLCHAINS_ANDROID_NDK_COMMON_CMAKE_)
  return()
endif()
set(DRACO_CMAKE_TOOLCHAINS_ANDROID_NDK_COMMON_CMAKE_ 1)

include("${CMAKE_CURRENT_LIST_DIR}/../util.cmake")

require_variable(CMAKE_ANDROID_NDK)
set(CMAKE_SYSTEM_NAME Android)
set_variable_if_unset(CMAKE_ANDROID_STL_TYPE c++_static)
set_variable_if_unset(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
