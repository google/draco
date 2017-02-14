cmake_minimum_required(VERSION 3.2)
project(draco C CXX)

set(draco_root "${CMAKE_CURRENT_SOURCE_DIR}")
set(draco_build_dir "${CMAKE_BINARY_DIR}")

include("${draco_root}/cmake/compiler_flags.cmake")

# Draco requires C++11 support.
require_cxx_flag_nomsvc("-std=c++11")

option(ENABLE_STANDARD_EDGEBREAKER "" ON)
option(ENABLE_PREDICTIVE_EDGEBREAKER "" ON)
option(ENABLE_EXTRA_WARNINGS "" OFF)
option(ENABLE_TESTS "Enables tests." OFF)
option(ENABLE_WERROR "" OFF)
option(ENABLE_WEXTRA "" OFF)
option(IGNORE_EMPTY_BUILD_TYPE "" OFF)

if (ENABLE_STANDARD_EDGEBREAKER)
  add_cxx_preproc_definition("DRACO_STANDARD_EDGEBREAKER_SUPPORTED")
endif ()
if (ENABLE_PREDICTIVE_EDGEBREAKER)
  add_cxx_preproc_definition("DRACO_PREDICTIVE_EDGEBREAKER_SUPPORTED")
endif ()

# Turn on more compiler warnings.
if (ENABLE_EXTRA_WARNINGS)
  if (MSVC)
    add_compiler_flag_if_supported("/W3")
    # Disable MSVC warnings that suggest making code non-portable.
    add_compiler_flag_if_supported("/wd4996")
    if (ENABLE_WERROR)
      add_compiler_flag_if_supported("/WX")
    endif ()
  else ()
    add_compiler_flag_if_supported("-Wall")
    add_compiler_flag_if_supported("-Wfloat-conversion")
    add_compiler_flag_if_supported("-Wimplicit-function-declaration")
    add_compiler_flag_if_supported("-Wpointer-arith")
    add_compiler_flag_if_supported("-Wshadow")
    add_compiler_flag_if_supported("-Wsign-compare")
    add_compiler_flag_if_supported("-Wtype-limits")
    add_compiler_flag_if_supported("-Wuninitialized")
    add_compiler_flag_if_supported("-Wunused")
  endif ()
endif ()

if (ENABLE_WERROR)
  add_compiler_flag_if_supported("-Werror")
endif ()
if (ENABLE_WEXTRA)
  add_compiler_flag_if_supported("-Wextra")
endif ()

# Generate a version file containing repository info.
include(FindGit)
find_package(Git)
# Default the hash and description to empty strings in case git is unavailable.
set(draco_git_hash "")
set(draco_git_desc "")
if (GIT_FOUND)
  set(draco_git_dir "${draco_root}/.git")
  if (NOT EXISTS "${draco_git_dir}")
    set(draco_git_dir "${draco_root}/../../../.git")
  endif ()
  if (EXISTS "${draco_git_dir}")
    execute_process(COMMAND ${GIT_EXECUTABLE}
                    --git-dir=${draco_git_dir} rev-parse HEAD
                    OUTPUT_VARIABLE draco_git_hash)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} --git-dir=${draco_git_dir}/.git describe
      OUTPUT_VARIABLE draco_git_desc ERROR_QUIET)
    # Consume newlines from Git output.
    string(STRIP "${draco_git_hash}" draco_git_hash)
    string(STRIP "${draco_git_desc}" draco_git_desc)
  endif ()
endif ()
if (draco_git_hash STREQUAL "")
  set(draco_git_desc "unknown")
endif ()
if (draco_git_desc STREQUAL "")
  set(draco_git_desc "unreleased")
endif ()
configure_file("${draco_root}/cmake/draco_version.cc.cmake"
               "${draco_build_dir}/draco_version.cc")
configure_file("${draco_root}/cmake/draco_version.h.cmake"
               "${draco_build_dir}/draco_version.h" COPYONLY)

if (EMSCRIPTEN)
  include(FindPythonInterp)
  if (NOT PYTHONINTERP_FOUND)
    message(FATAL_ERROR
            "Python required for Emscripten builds, but cmake cannot find it.")
  endif ()
else ()
  # When not building the JS library, and when a build type is not specified,
  # default to producing a release mode Draco library to avoid benchmarking
  # shenanigans, but only when Draco is not being pulled in via another cmake
  # file.
  if (CMAKE_BUILD_TYPE STREQUAL "" AND NOT IGNORE_EMPTY_BUILD_TYPE)
    if (CMAKE_CURRENT_LIST_FILE STREQUAL CMAKE_PARENT_LIST_FILE)
      message(INFO "|Draco: ignoring empty build type, forcing release mode.")
      set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Draco overridden build type"
          FORCE)
    endif ()
  endif ()

  if (ENABLE_TESTS)
    if (MSVC)
      # Default runtime selected by cmake collides with Googletest settings,
      # just force all Draco builds to be static (instead of dll/msvcrt).
      include("${draco_root}/cmake/msvc_runtime.cmake")
    endif ()
    # Googletest defaults.
    set(GTEST_SOURCE_DIR
        "${draco_root}/../googletest" CACHE STRING
        "Path to googletest source directory")
    set(GTEST_BUILD_DIR
        "${draco_build_dir}/googletest" CACHE STRING
        "Path to directory where googletest will be configured and built.")

    # Confirm Googletest is where expected.
    if (NOT EXISTS "${GTEST_SOURCE_DIR}/CMakeLists.txt")
      set(ENABLE_TESTS OFF)
      message("Tests disabled: Google test CMakeLists.txt does not exist.")
    else ()
      set(DRACO_TEST_DATA_DIR "${draco_root}/testdata")
      configure_file("${draco_root}/cmake/draco_test_config.h.cmake"
                     "${draco_build_dir}/testing/draco_test_config.h")
      add_subdirectory("${GTEST_SOURCE_DIR}" "${GTEST_BUILD_DIR}")
    endif ()

    include_directories("${GTEST_SOURCE_DIR}")
  endif ()
endif ()

# Draco source file listing variables.
set(draco_compression_attributes_decoder_sources
    "${draco_root}/compression/attributes/attributes_decoder.cc"
    "${draco_root}/compression/attributes/attributes_decoder.h"
    "${draco_root}/compression/attributes/kd_tree_attributes_decoder.cc"
    "${draco_root}/compression/attributes/kd_tree_attributes_decoder.h"
    "${draco_root}/compression/attributes/kd_tree_attributes_shared.h"
    "${draco_root}/compression/attributes/mesh_attribute_indices_encoding_data.h"
    "${draco_root}/compression/attributes/mesh_traversal_sequencer.h"
    "${draco_root}/compression/attributes/normal_compression_utils.h"
    "${draco_root}/compression/attributes/sequential_attribute_decoder.cc"
    "${draco_root}/compression/attributes/sequential_attribute_decoder.h"
    "${draco_root}/compression/attributes/sequential_attribute_decoders_controller.cc"
    "${draco_root}/compression/attributes/sequential_attribute_decoders_controller.h"
    "${draco_root}/compression/attributes/sequential_integer_attribute_decoder.cc"
    "${draco_root}/compression/attributes/sequential_integer_attribute_decoder.h"
    "${draco_root}/compression/attributes/sequential_normal_attribute_decoder.cc"
    "${draco_root}/compression/attributes/sequential_normal_attribute_decoder.h"
    "${draco_root}/compression/attributes/sequential_quantization_attribute_decoder.cc"
    "${draco_root}/compression/attributes/sequential_quantization_attribute_decoder.h")

set(draco_compression_attributes_encoder_sources
    "${draco_root}/compression/attributes/attributes_encoder.cc"
    "${draco_root}/compression/attributes/attributes_encoder.h"
    "${draco_root}/compression/attributes/kd_tree_attributes_encoder.cc"
    "${draco_root}/compression/attributes/kd_tree_attributes_encoder.h"
    "${draco_root}/compression/attributes/linear_sequencer.h"
    "${draco_root}/compression/attributes/mesh_attribute_indices_encoding_observer.h"
    "${draco_root}/compression/attributes/points_sequencer.h"
    "${draco_root}/compression/attributes/sequential_attribute_encoder.cc"
    "${draco_root}/compression/attributes/sequential_attribute_encoder.h"
    "${draco_root}/compression/attributes/sequential_attribute_encoders_controller.cc"
    "${draco_root}/compression/attributes/sequential_attribute_encoders_controller.h"
    "${draco_root}/compression/attributes/sequential_integer_attribute_encoder.cc"
    "${draco_root}/compression/attributes/sequential_integer_attribute_encoder.h"
    "${draco_root}/compression/attributes/sequential_normal_attribute_encoder.cc"
    "${draco_root}/compression/attributes/sequential_normal_attribute_encoder.h"
    "${draco_root}/compression/attributes/sequential_quantization_attribute_encoder.cc"
    "${draco_root}/compression/attributes/sequential_quantization_attribute_encoder.h")

set(draco_compression_attributes_pred_schemes_sources
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_interface.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_data.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_multi_parallelogram.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_shared.h"
    "${draco_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_decoder_factory.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_difference.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.cc"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_factory.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_transform.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_transform.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_wrap_transform.h")

set(draco_compression_config_sources
    "${draco_root}/compression/config/compression_shared.h"
    "${draco_root}/compression/config/encoder_options.cc"
    "${draco_root}/compression/config/encoder_options.h"
    "${draco_root}/compression/config/encoding_features.h")

set(draco_compression_decode_sources
    "${draco_root}/compression/decode.cc"
    "${draco_root}/compression/decode.h")

set(draco_compression_encode_sources
    "${draco_root}/compression/encode.cc"
    "${draco_root}/compression/encode.h")

set(draco_compression_mesh_decoder_sources
    "${draco_root}/compression/mesh/mesh_decoder.cc"
    "${draco_root}/compression/mesh/mesh_decoder.h"
    "${draco_root}/compression/mesh/mesh_decoder_helpers.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_decoder.cc"
    "${draco_root}/compression/mesh/mesh_edgebreaker_decoder.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_decoder_impl.cc"
    "${draco_root}/compression/mesh/mesh_edgebreaker_decoder_impl.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_decoder_impl_interface.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_shared.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_traversal_decoder.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_traversal_predictive_decoder.h"
    "${draco_root}/compression/mesh/mesh_sequential_decoder.cc"
    "${draco_root}/compression/mesh/mesh_sequential_decoder.h")

set(draco_compression_mesh_encoder_sources
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoder.cc"
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoder.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoder_impl.cc"
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoder_impl.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoder_impl_interface.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_shared.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_traversal_encoder.h"
    "${draco_root}/compression/mesh/mesh_edgebreaker_traversal_predictive_encoder.h"
    "${draco_root}/compression/mesh/mesh_encoder.cc"
    "${draco_root}/compression/mesh/mesh_encoder.h"
    "${draco_root}/compression/mesh/mesh_encoder_helpers.h"
    "${draco_root}/compression/mesh/mesh_sequential_encoder.cc"
    "${draco_root}/compression/mesh/mesh_sequential_encoder.h")

set(draco_compression_point_cloud_decoder_sources
    "${draco_root}/compression/point_cloud/point_cloud_decoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_decoder.h"
    "${draco_root}/compression/point_cloud/point_cloud_kd_tree_decoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_kd_tree_decoder.h"
    "${draco_root}/compression/point_cloud/point_cloud_sequential_decoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_sequential_decoder.h")

set(draco_compression_point_cloud_encoder_sources
    "${draco_root}/compression/point_cloud/point_cloud_encoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_encoder.h"
    "${draco_root}/compression/point_cloud/point_cloud_kd_tree_encoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_kd_tree_encoder.h"
    "${draco_root}/compression/point_cloud/point_cloud_sequential_encoder.cc"
    "${draco_root}/compression/point_cloud/point_cloud_sequential_encoder.h")

set(draco_core_sources
    "${draco_root}/core/adaptive_rans_coding.cc"
    "${draco_root}/core/adaptive_rans_coding.h"
    "${draco_root}/core/ans.h"
    "${draco_root}/core/bit_coder.cc"
    "${draco_root}/core/bit_coder.h"
    "${draco_root}/core/bit_utils.h"
    "${draco_root}/core/cycle_timer.cc"
    "${draco_root}/core/cycle_timer.h"
    "${draco_root}/core/data_buffer.cc"
    "${draco_root}/core/data_buffer.h"
    "${draco_root}/core/decoder_buffer.cc"
    "${draco_root}/core/decoder_buffer.h"
    "${draco_root}/core/direct_bit_coding.cc"
    "${draco_root}/core/direct_bit_coding.h"
    "${draco_root}/core/divide.cc"
    "${draco_root}/core/divide.h"
    "${draco_root}/core/draco_index_type.h"
    "${draco_root}/core/draco_index_type_vector.h"
    "${draco_root}/core/draco_types.cc"
    "${draco_root}/core/draco_types.h"
    "${draco_root}/core/encoder_buffer.cc"
    "${draco_root}/core/encoder_buffer.h"
    "${draco_root}/core/folded_bit32_coding.h"
    "${draco_root}/core/hash_utils.cc"
    "${draco_root}/core/hash_utils.h"
    "${draco_root}/core/macros.h"
    "${draco_root}/core/math_utils.h"
    "${draco_root}/core/options.cc"
    "${draco_root}/core/options.h"
    "${draco_root}/core/quantization_utils.cc"
    "${draco_root}/core/quantization_utils.h"
    "${draco_root}/core/rans_coding.cc"
    "${draco_root}/core/rans_coding.h"
    "${draco_root}/core/rans_symbol_coding.h"
    "${draco_root}/core/rans_symbol_decoder.h"
    "${draco_root}/core/rans_symbol_encoder.h"
    "${draco_root}/core/symbol_decoding.cc"
    "${draco_root}/core/symbol_decoding.h"
    "${draco_root}/core/symbol_encoding.cc"
    "${draco_root}/core/symbol_encoding.h"
    "${draco_root}/core/vector_d.h")

set(draco_io_sources
    "${draco_root}/io/mesh_io.cc"
    "${draco_root}/io/mesh_io.h"
    "${draco_root}/io/obj_decoder.cc"
    "${draco_root}/io/obj_decoder.h"
    "${draco_root}/io/obj_encoder.cc"
    "${draco_root}/io/obj_encoder.h"
    "${draco_root}/io/parser_utils.cc"
    "${draco_root}/io/parser_utils.h"
    "${draco_root}/io/ply_decoder.cc"
    "${draco_root}/io/ply_decoder.h"
    "${draco_root}/io/ply_encoder.cc"
    "${draco_root}/io/ply_encoder.h"
    "${draco_root}/io/ply_property_reader.h"
    "${draco_root}/io/ply_property_writer.h"
    "${draco_root}/io/ply_reader.cc"
    "${draco_root}/io/ply_reader.h"
    "${draco_root}/io/point_cloud_io.cc"
    "${draco_root}/io/point_cloud_io.h")

set(draco_mesh_sources
    "${draco_root}/mesh/corner_table.cc"
    "${draco_root}/mesh/corner_table.h"
    "${draco_root}/mesh/corner_table_indices.h"
    "${draco_root}/mesh/corner_table_iterators.h"
    "${draco_root}/mesh/corner_table_traversal_processor.h"
    "${draco_root}/mesh/edgebreaker_observer.h"
    "${draco_root}/mesh/edgebreaker_traverser.h"
    "${draco_root}/mesh/mesh.cc"
    "${draco_root}/mesh/mesh.h"
    "${draco_root}/mesh/mesh_attribute_corner_table.cc"
    "${draco_root}/mesh/mesh_attribute_corner_table.h"
    "${draco_root}/mesh/mesh_cleanup.cc"
    "${draco_root}/mesh/mesh_cleanup.h"
    "${draco_root}/mesh/mesh_indices.h"
    "${draco_root}/mesh/mesh_misc_functions.cc"
    "${draco_root}/mesh/mesh_misc_functions.h"
    "${draco_root}/mesh/triangle_soup_mesh_builder.cc"
    "${draco_root}/mesh/triangle_soup_mesh_builder.h")

set(draco_point_cloud_sources
    "${draco_root}/point_cloud/geometry_attribute.cc"
    "${draco_root}/point_cloud/geometry_attribute.h"
    "${draco_root}/point_cloud/geometry_indices.h"
    "${draco_root}/point_cloud/point_attribute.cc"
    "${draco_root}/point_cloud/point_attribute.h"
    "${draco_root}/point_cloud/point_cloud.cc"
    "${draco_root}/point_cloud/point_cloud.h"
    "${draco_root}/point_cloud/point_cloud_builder.cc"
    "${draco_root}/point_cloud/point_cloud_builder.h")

set(draco_points_common_sources
    "${draco_root}/compression/point_cloud/algorithms/point_cloud_types.h"
    "${draco_root}/compression/point_cloud/algorithms/quantize_points_3.h"
    "${draco_root}/compression/point_cloud/algorithms/queuing_policy.h")

set(draco_points_decoder_sources
    "${draco_root}/compression/point_cloud/algorithms/float_points_kd_tree_decoder.cc"
    "${draco_root}/compression/point_cloud/algorithms/float_points_kd_tree_decoder.h"
    "${draco_root}/compression/point_cloud/algorithms/integer_points_kd_tree_decoder.cc"
    "${draco_root}/compression/point_cloud/algorithms/integer_points_kd_tree_decoder.h")

set(draco_points_encoder_sources
    "${draco_root}/compression/point_cloud/algorithms/float_points_kd_tree_encoder.cc"
    "${draco_root}/compression/point_cloud/algorithms/float_points_kd_tree_encoder.h"
    "${draco_root}/compression/point_cloud/algorithms/integer_points_kd_tree_encoder.cc"
    "${draco_root}/compression/point_cloud/algorithms/integer_points_kd_tree_encoder.h")

set(draco_js_sources
    "${draco_root}/javascript/emscripten/draco_glue_wrapper.cc"
    "${draco_root}/javascript/emscripten/webidl_wrapper.cc")

set(draco_test_sources
    "${draco_root}/core/draco_tests.cc"
    "${draco_root}/core/draco_test_base.h"
    "${draco_root}/core/draco_test_utils.cc"
    "${draco_root}/core/draco_test_utils.h"
    "${draco_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_transform_test.cc"
    "${draco_root}/compression/attributes/sequential_integer_attribute_encoding_test.cc"
    "${draco_root}/compression/mesh/mesh_encoder_test.cc"
    "${draco_root}/compression/mesh/mesh_edgebreaker_encoding_test.cc"
    "${draco_root}/compression/point_cloud/point_cloud_kd_tree_encoding_test.cc"
    "${draco_root}/compression/point_cloud/point_cloud_sequential_encoding_test.cc"
    "${draco_root}/core/bit_coder_test.cc"
    "${draco_root}/core/math_utils_test.cc"
    "${draco_root}/core/quantization_utils_test.cc"
    "${draco_root}/core/rans_coding_test.cc"
    "${draco_root}/core/symbol_coding_test.cc"
    "${draco_root}/core/vector_d_test.cc"
    "${draco_root}/io/obj_decoder_test.cc"
    "${draco_root}/io/ply_decoder_test.cc"
    "${draco_root}/io/ply_reader_test.cc"
    "${draco_root}/io/point_cloud_io_test.cc"
    "${draco_root}/mesh/mesh_cleanup_test.cc"
    "${draco_root}/mesh/mesh_test.cc"
    "${draco_root}/mesh/triangle_soup_mesh_builder_test.cc"
    "${draco_root}/point_cloud/point_cloud_builder_test.cc")

set(draco_version_sources
    "${draco_build_dir}/draco_version.cc"
    "${draco_build_dir}/draco_version.h")

include_directories("${draco_root}")

#
# Draco targets.
#
if (EMSCRIPTEN)
  # Draco js decoder.
  add_compiler_flag_if_supported("-s ALLOW_MEMORY_GROWTH=1")
  add_compiler_flag_if_supported("--memory-init-file 0")
  add_compiler_flag_if_supported("-fno-omit-frame-pointer")

  if (CMAKE_BUILD_TYPE STREQUAL "")
    # Force -O3 when no build type is specified.
    add_compiler_flag_if_supported("-O3")
  endif ()

  set(draco_js_idl "${draco_root}/javascript/emscripten/draco_web.idl")

  # Generate ${draco_build_dir}/glue.cpp at cmake generation time so it can be
  # added to targets without cmake reporting errors.
  execute_process(COMMAND ${PYTHON_EXECUTABLE}
                  $ENV{EMSCRIPTEN}/tools/webidl_binder.py ${draco_js_idl}
                  ${draco_build_dir}/glue
                  OUTPUT_FILE ${draco_build_dir}/glue.cpp)

  # Add a custom rule depending on the IDL to regenerate
  # ${draco_build_dir}/glue.cpp as needed.
  add_custom_command(OUTPUT ${draco_build_dir}/glue.cpp
                     COMMAND ${PYTHON_EXECUTABLE}
                     $ENV{EMSCRIPTEN}/tools/webidl_binder.py ${draco_js_idl}
                     ${draco_build_dir}/glue
                     DEPENDS ${draco_js_idl}
                     COMMENT "Generating ${draco_build_dir}/glue.cpp."
                     WORKING_DIRECTORY ${draco_build_dir}
                     VERBATIM)

  # Add path to glue.cpp to draco include paths.
  include_directories("${draco_build_dir}")

  add_executable(draco_decoder
                 ${draco_compression_attributes_decoder_sources}
                 ${draco_compression_decode_sources}
                 ${draco_compression_mesh_decoder_sources}
                 ${draco_compression_point_cloud_decoder_sources}
                 ${draco_core_sources}
                 ${draco_io_sources}
                 ${draco_mesh_sources}
                 ${draco_point_cloud_sources}
                 ${draco_points_decoder_sources}
                 ${draco_js_sources}
                 ${draco_version_sources})

  # Make $draco_js_sources source files depend on glue.cpp.
  set_property(SOURCE ${draco_js_sources} APPEND PROPERTY OBJECT_DEPENDS
               ${draco_build_dir}/glue.cpp)
  em_link_post_js(draco_decoder "${draco_build_dir}/glue.js")
else ()
  # Standard Draco libs, encoder and decoder.
  # Object collections that mirror the Draco directory structure.
  add_library(draco_compression_attributes_decoder OBJECT
              ${draco_compression_attributes_decoder_sources})
  add_library(draco_compression_attributes_encoder OBJECT
              ${draco_compression_attributes_encoder_sources})
  add_library(draco_compression_attributes_pred_schemes OBJECT
              ${draco_compression_attributes_pred_schemes_sources})
  add_library(draco_compression_config OBJECT
              ${draco_compression_config_sources})
  add_library(draco_compression_decode OBJECT
              ${draco_compression_decode_sources})
  add_library(draco_compression_encode OBJECT
              ${draco_compression_encode_sources})
  add_library(draco_compression_mesh_decoder OBJECT
              ${draco_compression_mesh_decoder_sources})
  add_library(draco_compression_mesh_encoder OBJECT
              ${draco_compression_mesh_encoder_sources})
  add_library(draco_compression_point_cloud_decoder OBJECT
              ${draco_compression_point_cloud_decoder_sources})
  add_library(draco_compression_point_cloud_encoder OBJECT
              ${draco_compression_point_cloud_encoder_sources})
  add_library(draco_core OBJECT ${draco_core_sources})
  add_library(draco_io OBJECT ${draco_io_sources})
  add_library(draco_mesh OBJECT ${draco_mesh_sources})
  add_library(draco_point_cloud OBJECT ${draco_point_cloud_sources})
  add_library(draco_points_decoder OBJECT
              ${draco_points_common_sources}
              ${draco_points_decoder_sources})
  add_library(draco_points_encoder OBJECT
              ${draco_points_common_sources}
              ${draco_points_encoder_sources})

  # Library targets that consume the object collections.
  add_library(dracodec
              ${draco_version_sources}
              $<TARGET_OBJECTS:draco_compression_attributes_decoder>
              $<TARGET_OBJECTS:draco_compression_decode>
              $<TARGET_OBJECTS:draco_compression_mesh_decoder>
              $<TARGET_OBJECTS:draco_compression_point_cloud_decoder>
              $<TARGET_OBJECTS:draco_core>
              $<TARGET_OBJECTS:draco_io>
              $<TARGET_OBJECTS:draco_mesh>
              $<TARGET_OBJECTS:draco_point_cloud>
              $<TARGET_OBJECTS:draco_points_decoder>)
  add_library(draco
              ${draco_version_sources}
              $<TARGET_OBJECTS:draco_compression_attributes_decoder>
              $<TARGET_OBJECTS:draco_compression_attributes_encoder>
              $<TARGET_OBJECTS:draco_compression_attributes_pred_schemes>
              $<TARGET_OBJECTS:draco_compression_config>
              $<TARGET_OBJECTS:draco_compression_decode>
              $<TARGET_OBJECTS:draco_compression_encode>
              $<TARGET_OBJECTS:draco_compression_mesh_decoder>
              $<TARGET_OBJECTS:draco_compression_mesh_encoder>
              $<TARGET_OBJECTS:draco_compression_point_cloud_decoder>
              $<TARGET_OBJECTS:draco_compression_point_cloud_encoder>
              $<TARGET_OBJECTS:draco_core>
              $<TARGET_OBJECTS:draco_io>
              $<TARGET_OBJECTS:draco_mesh>
              $<TARGET_OBJECTS:draco_point_cloud>
              $<TARGET_OBJECTS:draco_points_decoder>
              $<TARGET_OBJECTS:draco_points_encoder>)

  # Draco app targets.
  add_executable(draco_decoder "${draco_root}/tools/draco_decoder.cc")
  target_link_libraries(draco_decoder PUBLIC dracodec)
  add_executable(draco_encoder
                 "${draco_root}/tools/draco_encoder.cc")
  target_link_libraries(draco_encoder PUBLIC draco)

  if (ENABLE_TESTS)
    add_executable(draco_tests ${draco_test_sources})
    include_directories("${draco_build_dir}"
                        "${GTEST_SOURCE_DIR}/googletest/include")
    target_link_libraries(draco_tests draco gtest)
  endif ()

  # Collect all of the header files in the tree, and add an install rule for
  # each.
  file(GLOB_RECURSE draco_headers RELATIVE ${draco_root} "*.h")
  foreach (filename ${draco_headers})
    get_filename_component(file_directory ${filename} DIRECTORY)
    install(FILES ${filename} DESTINATION
            "${CMAKE_INSTALL_PREFIX}/include/draco/${file_directory}")
  endforeach()

  # Add install rules for lib and executable targets.
  install(TARGETS dracodec draco DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
  install(TARGETS draco_decoder draco_encoder DESTINATION
          "${CMAKE_INSTALL_PREFIX}/bin")
endif ()
