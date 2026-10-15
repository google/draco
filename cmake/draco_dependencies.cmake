# Copyright 2022 The Draco Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

if(DRACO_CMAKE_DRACO_DEPENDENCIES_CMAKE)
  return()
endif()
set(DRACO_CMAKE_DRACO_DEPENDENCIES_CMAKE 1)

include("${draco_root}/cmake/draco_variables.cmake")

# Each variable holds a user specified custom path to a local copy of the
# sources that belong to each project that Draco depends on. When paths are
# empty the build will be generated pointing to the Draco git submodules.
# Otherwise the paths specified by the user will be used in the build
# configuration.

# Path to the Eigen. The path must contain the Eigen directory.
set(DRACO_EIGEN_PATH)
draco_track_configuration_variable(DRACO_EIGEN_PATH)

# Path to the gulrak/filesystem installation. The path specified must contain
# the ghc subdirectory that houses the filesystem includes.
set(DRACO_FILESYSTEM_PATH)
draco_track_configuration_variable(DRACO_FILESYSTEM_PATH)

# Path to the googletest installation. The path must be to the root of the
# Googletest project directory.
set(DRACO_GOOGLETEST_PATH)
draco_track_configuration_variable(DRACO_GOOGLETEST_PATH)

# Path to the syoyo/tinygltf installation. The path must be to the root of the
# project directory.
set(DRACO_TINYGLTF_PATH)
draco_track_configuration_variable(DRACO_TINYGLTF_PATH)

# Path to the AOMedia PMC installation. The path must be to the root of the
# project directory.
set(DRACO_PMC_PATH)
draco_track_configuration_variable(DRACO_PMC_PATH)

# Utility macro for killing the build due to a missing submodule directory.
macro(draco_die_missing_submodule dir)
  message(FATAL_ERROR "${dir} missing, run git submodule update --init")
endmacro()

# Determines the Eigen location and updates the build configuration accordingly.
macro(draco_setup_eigen)
  if(DRACO_EIGEN_PATH)
    set(eigen_path "${DRACO_EIGEN_PATH}")

    if(NOT IS_DIRECTORY "${eigen_path}")
      message(FATAL_ERROR "DRACO_EIGEN_PATH does not exist.")
    endif()
  else()
    set(eigen_path "${draco_root}/third_party/eigen")

    if(NOT IS_DIRECTORY "${eigen_path}")
      draco_die_missing_submodule("${eigen_path}")
    endif()
  endif()

  set(eigen_include_path "${eigen_path}/Eigen")

  if(NOT EXISTS "${eigen_path}/Eigen")
    message(FATAL_ERROR "The eigen path does not contain an Eigen directory.")
  endif()

  list(APPEND draco_include_paths "${eigen_path}")
endmacro()

# Determines the gulrak/filesystem location and updates the build configuration
# accordingly.
macro(draco_setup_filesystem)
  if(DRACO_FILESYSTEM_PATH)
    set(fs_path "${DRACO_FILESYSTEM_PATH}")

    if(NOT IS_DIRECTORY "${fs_path}")
      message(FATAL_ERROR "DRACO_FILESYSTEM_PATH does not exist.")
    endif()
  else()
    set(fs_path "${draco_root}/third_party/filesystem/include")

    if(NOT IS_DIRECTORY "${fs_path}")
      draco_die_missing_submodule("${fs_path}")
    endif()
  endif()

  list(APPEND draco_include_paths "${fs_path}")
endmacro()

# Determines the Googletest location and sets up include and source list vars
# for the draco_tests build.
macro(draco_setup_googletest)
  if(DRACO_GOOGLETEST_PATH)
    set(gtest_path "${DRACO_GOOGLETEST_PATH}")
    if(NOT IS_DIRECTORY "${gtest_path}")
      message(FATAL_ERROR "DRACO_GOOGLETEST_PATH does not exist.")
    endif()
  else()
    set(gtest_path "${draco_root}/third_party/googletest")
  endif()

  list(APPEND draco_test_include_paths ${draco_include_paths}
              "${gtest_path}/include" "${gtest_path}/googlemock"
              "${gtest_path}/googletest/include" "${gtest_path}/googletest")

  list(APPEND draco_gtest_all "${gtest_path}/googletest/src/gtest-all.cc")
  list(APPEND draco_gtest_main "${gtest_path}/googletest/src/gtest_main.cc")
endmacro()


# Determines the location of TinyGLTF and updates the build configuration
# accordingly.
macro(draco_setup_tinygltf)
  if(DRACO_TINYGLTF_PATH)
    set(tinygltf_path "${DRACO_TINYGLTF_PATH}")

    if(NOT IS_DIRECTORY "${tinygltf_path}")
      message(FATAL_ERROR "DRACO_TINYGLTF_PATH does not exist.")
    endif()
  else()
    set(tinygltf_path "${draco_root}/third_party/tinygltf")

    if(NOT IS_DIRECTORY "${tinygltf_path}")
      draco_die_missing_submodule("${tinygltf_path}")
    endif()
  endif()

  list(APPEND draco_include_paths "${tinygltf_path}")
endmacro()

# Determines the PMC location and updates the build configuration accordingly.
macro(draco_setup_pmc)
  if(DRACO_PMC_PATH)
    set(pmc_path "${DRACO_PMC_PATH}")

    if(NOT IS_DIRECTORY "${pmc_path}")
      message(FATAL_ERROR "DRACO_PMC_PATH does not exist.")
    endif()
  else()
    set(pmc_path "${draco_root}/third_party/pmc")

    if(NOT IS_DIRECTORY "${pmc_path}")
      draco_die_missing_submodule("${pmc_path}")
    endif()
  endif()

  include(ExternalProject)

  set(pmc_build_dir "${draco_build}/pmc-build")

  # Determine library file name extension
  if(MSVC)
    set(pmc_lib_name "pmc.lib")
  else()
    set(pmc_lib_name "libpmc.a")
  endif()
  set(pmc_lib_path "${pmc_build_dir}/lib/${pmc_lib_name}")

  # Configure PMC as an external project.
  # This avoids path issues (PMC using CMAKE_SOURCE_DIR) when built in-tree.
  ExternalProject_Add(pmc_ext
    SOURCE_DIR "${pmc_path}"
    BINARY_DIR "${pmc_build_dir}"
    BUILD_COMMAND
      ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target genversion
      COMMAND
      ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target pmc
    BUILD_BYPRODUCTS "${pmc_lib_path}"
    INSTALL_COMMAND ""
    CMAKE_ARGS
      -DPMC_LIB_ONLY=ON
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${pmc_build_dir}/lib"
      "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${pmc_build_dir}/lib"
      "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${pmc_build_dir}/lib"
      "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL=${pmc_build_dir}/lib"
      "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO=${pmc_build_dir}/lib"
  )

  # Define the imported target that Draco can link against.
  add_library(pmc STATIC IMPORTED GLOBAL)
  set_target_properties(pmc PROPERTIES
    IMPORTED_LOCATION "${pmc_lib_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${pmc_path}/lib/public"
  )
  add_dependencies(pmc pmc_ext)

  list(APPEND draco_include_paths "${pmc_path}/lib/public")
endmacro()
