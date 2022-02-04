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

if(DRACO_CMAKE_DRACO_DEPENDENCIES_CMAKE_)
  return()
endif() # DRACO_CMAKE_DRACO_DEPENDENCIES_CMAKE_
set(DRACO_CMAKE_DRACO_DEPENDENCIES_CMAKE_ 1)

include(ExternalProject)
include(FetchContent)

set(DRACO_EXT "${draco_build}/_external")
set(DRACO_INSTALL_PREFIX "${DRACO_EXT}/_install")

macro(draco_add_tinygltf)
  # Tell CMake how to download and build TinyGLTF.
  ExternalProject_Add(
    tinygltf
    PREFIX "${DRACO_EXT}/tinygltf"
    BINARY_DIR "${DRACO_EXT}/tinygltf/_build"
    SOURCE_DIR "${DRACO_EXT}/tinygltf/_source"
    GIT_REPOSITORY "https://github.com/syoyo/tinygltf.git"
    GIT_TAG "a11f6e19399f6af67d7c57909e8ce99d20beb369"
    GIT_PROGRESS ON
    # Requiring both of these seems like I'm repeating myself.
    INSTALL_DIR "${DRACO_INSTALL_PREFIX}"
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DRACO_INSTALL_PREFIX}"
               "-DTINYGLTF_BUILD_EXAMPLES=OFF"
  )

  # Make every object library depend on TinyGLTF.
  foreach(dep ${draco_objlib_targets})
    add_dependencies(${dep} tinygltf)
  endforeach()
endmacro()

macro(draco_add_filesystem)
  # Tell CMake how to download and build filesystem.
  #ExternalProject_Add(
  #  filesystem
  #  PREFIX "${DRACO_EXT}/filesystem"
  #  BINARY_DIR "${DRACO_EXT}/filesystem/_build"
  #  SOURCE_DIR "${DRACO_EXT}/filesystem/_source"
  #  GIT_REPOSITORY "https://github.com/gulrak/filesystem"
  #  GIT_TAG "a07ddedeae722c09e819895e1c31ae500e9abad6"
  #  GIT_PROGRESS ON
  #  # Requiring both of these seems like I'm repeating myself.
  #  INSTALL_DIR "${DRACO_INSTALL_PREFIX}"
  #  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DRACO_INSTALL_PREFIX}"
  #             "-DGHC_FILESYSTEM_BUILD_EXAMPLES=OFF"
  #             "-DGHC_FILESYSTEM_BUILD_STD_TESTING=OFF"
  #             "-DGHC_FILESYSTEM_BUILD_TESTING=OFF"
  #)

  # Make every object library depend on TinyGLTF.
  #foreach(dep ${draco_objlib_targets})
  #  add_dependencies(${dep} filesystem)
  #endforeach()

  FetchContent_Declare(
    filesystem
    PREFIX "${DRACO_EXT}/filesystem"
    BINARY_DIR "${DRACO_EXT}/filesystem/_build"
    SOURCE_DIR "${DRACO_EXT}/filesystem/_source"
    GIT_REPOSITORY "https://github.com/gulrak/filesystem"
    GIT_TAG "a07ddedeae722c09e819895e1c31ae500e9abad6"
    GIT_PROGRESS ON
    # Requiring both of these seems like I'm repeating myself.
    INSTALL_DIR "${DRACO_INSTALL_PREFIX}"
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DRACO_INSTALL_PREFIX}"
               "-DGHC_FILESYSTEM_BUILD_EXAMPLES=OFF"
               "-DGHC_FILESYSTEM_BUILD_STD_TESTING=OFF"
               "-DGHC_FILESYSTEM_BUILD_TESTING=OFF"
  )

  FetchContent_MakeAvailable(filesystem)

endmacro()

macro(draco_add_eigen)
  # Tell CMake how to download and build filesystem.
  ExternalProject_Add(
    eigen
    PREFIX "${DRACO_EXT}/eigen"
    BINARY_DIR "${DRACO_EXT}/eigen/_build"
    SOURCE_DIR "${DRACO_EXT}/eigen/_source"
    GIT_REPOSITORY "https://gitlab.com/libeigen/eigen.git"
    GIT_TAG "a07ddedeae722c09e819895e1c31ae500e9abad6"
    GIT_PROGRESS ON
    # Requiring both of these seems like I'm repeating myself.
    INSTALL_DIR "${DRACO_INSTALL_PREFIX}"
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DRACO_INSTALL_PREFIX}"
               "-DGHC_FILESYSTEM_BUILD_EXAMPLES=OFF"
               "-DGHC_FILESYSTEM_BUILD_STD_TESTING=OFF"
               "-DGHC_FILESYSTEM_BUILD_TESTING=OFF"
  )

  # Make every object library depend on TinyGLTF.
  foreach(dep ${draco_objlib_targets})
    add_dependencies(${dep} filesystem)
  endforeach()
endmacro()
