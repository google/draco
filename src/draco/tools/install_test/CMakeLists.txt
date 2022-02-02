# Copyright 2022 The Draco Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
project(install_test C CXX)

include(GNUInstallDirs)

# Tell find_package() where Draco is installed.
if(BUILD_SHARED_LIBS)
  set(CMAKE_PREFIX_PATH
      "${CMAKE_CURRENT_LIST_DIR}/_draco_install_shared/share/cmake")
else()
  set(CMAKE_PREFIX_PATH
      "${CMAKE_CURRENT_LIST_DIR}/_draco_install_static/share/cmake")
endif()

find_package(draco REQUIRED)

if(BUILD_SHARED_LIBS)
  # Add rpath to resolve dylib dependency on Linux/MacOS.
  list(APPEND CMAKE_BUILD_RPATH "${DRACO_LIBRARY_DIR}")
  list(APPEND CMAKE_INSTALL_RPATH "${DRACO_LIBRARY_DIR}")
endif()

list(APPEND install_test_sources "main.cc")
add_executable(install_check ${install_test_sources})

# Update include paths and dependencies to allow for successful build of the
# install_check target using Draco as configured for the current installation.
target_include_directories(install_check PUBLIC "${DRACO_INCLUDE_DIR}")
target_link_libraries(install_check "${DRACO_LIBRARY}")

install(TARGETS install_check DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

if(BUILD_SHARED_LIBS AND WIN32)
  # Copy the Draco DLL into the bin dir for Windows: Windows doesn't really have
  # a concept of rpath, but it does look in the current directory by default
  # when a program tries to load a DLL.
  install(FILES "${DRACO_LIBRARY_DIR}/draco.dll"
          DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif()
