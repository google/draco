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

list(APPEND install_test_sources "main.cc")
add_executable(install_check ${install_test_sources})

if(BUILD_SHARED_LIBS)
  set(CMAKE_PREFIX_PATH
      "${CMAKE_CURRENT_LIST_DIR}/_install_test_root_shared/share/cmake")
else()
  set(CMAKE_PREFIX_PATH
      "${CMAKE_CURRENT_LIST_DIR}/_install_test_root_static/share/cmake")
endif()

find_package(draco REQUIRED)

# Update include paths and dependencies to allow for successful build of the
# install_check target using Draco as configured for the current installation.
target_include_directories(install_check PUBLIC ${DRACO_INCLUDE_DIR})
target_link_libraries(install_check ${DRACO_LIBRARY})
