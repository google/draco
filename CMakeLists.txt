cmake_minimum_required(VERSION 3.2)
project(draco C CXX)

set(draco_root "${CMAKE_CURRENT_SOURCE_DIR}")
set(draco_src_root "${draco_root}/src/draco")
set(draco_build_dir "${CMAKE_BINARY_DIR}")

if ("${draco_root}" STREQUAL "${draco_build_dir}")
  message(FATAL_ERROR
          "Building from within the Draco source tree is not supported.\n"
          "Hint: mkdir -p draco_build && cd draco_build\n"
          "Run cmake from there.")
endif ()

include(CMakePackageConfigHelpers)
include("${draco_root}/cmake/compiler_flags.cmake")
include("${draco_root}/cmake/sanitizers.cmake")
include("${draco_root}/cmake/util.cmake")

# Draco requires C++11 support.
require_cxx_flag_nomsvc("-std=c++11" YES)

option(ENABLE_CCACHE "Enable ccache support." OFF)
option(ENABLE_DISTCC "Enable distcc support." OFF)
option(ENABLE_EXTRA_SPEED "" OFF)
option(ENABLE_EXTRA_WARNINGS "" OFF)
option(ENABLE_GOMA "Enable goma support." OFF)
option(ENABLE_MESH_COMPRESSION "" ON)
option(ENABLE_POINT_CLOUD_COMPRESSION "" ON)
option(ENABLE_PREDICTIVE_EDGEBREAKER "" ON)
option(ENABLE_STANDARD_EDGEBREAKER "" ON)
option(ENABLE_BACKWARDS_COMPATIBILITY "" ON)
option(ENABLE_DECODER_ATTRIBUTE_DEDUPLICATION "" OFF)
option(ENABLE_TESTS "Enables tests." OFF)
option(ENABLE_WASM "" OFF)
option(ENABLE_WERROR "" OFF)
option(ENABLE_WEXTRA "" OFF)
option(IGNORE_EMPTY_BUILD_TYPE "" OFF)
option(BUILD_UNITY_PLUGIN "Build plugin library for Unity" OFF)
option(BUILD_FOR_GLTF "" OFF)

if (BUILD_FOR_GLTF)
# Override settings when building for GLTF.
add_cxx_preproc_definition("DRACO_MESH_COMPRESSION_SUPPORTED")
add_cxx_preproc_definition("DRACO_STANDARD_EDGEBREAKER_SUPPORTED")
else ()
  if (ENABLE_POINT_CLOUD_COMPRESSION)
    add_cxx_preproc_definition("DRACO_POINT_CLOUD_COMPRESSION_SUPPORTED")
  endif ()
  if (ENABLE_MESH_COMPRESSION)
    add_cxx_preproc_definition("DRACO_MESH_COMPRESSION_SUPPORTED")

    if (ENABLE_STANDARD_EDGEBREAKER)
      add_cxx_preproc_definition("DRACO_STANDARD_EDGEBREAKER_SUPPORTED")
    endif ()
    if (ENABLE_PREDICTIVE_EDGEBREAKER)
      add_cxx_preproc_definition("DRACO_PREDICTIVE_EDGEBREAKER_SUPPORTED")
    endif ()
  endif ()

  if (ENABLE_BACKWARDS_COMPATIBILITY)
    add_cxx_preproc_definition("DRACO_BACKWARDS_COMPATIBILITY_SUPPORTED")
  endif ()
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
if (ENABLE_CCACHE)
  set_compiler_launcher(ENABLE_CCACHE ccache)
endif ()
if (ENABLE_DISTCC)
  set_compiler_launcher(ENABLE_DISTCC distcc)
endif ()
if (ENABLE_GOMA)
  set_compiler_launcher(ENABLE_GOMA gomacc)
endif ()
if (BUILD_UNITY_PLUGIN)
  set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared library for unity plugin.")
  add_cxx_preproc_definition("BUILD_UNITY_PLUGIN")
endif ()

if(ENABLE_EXTRA_SPEED)
  if(MSVC)
    # Maximum optimization in Release mode.
    add_compile_options("$<$<CONFIG:RELEASE>:/Ox>")
  else()
    add_compiler_flag_if_supported("-O3")
  endif()
endif()

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
      if (APPLE AND BUILD_SHARED_LIBS)
        # Silence an RPATH warning emitted during processing of Googletest
        # cmake files.
        set(CMAKE_MACOSX_RPATH 1)
      endif ()

      set(DRACO_TEST_DATA_DIR "${draco_root}/testdata")
      configure_file("${draco_root}/cmake/draco_test_config.h.cmake"
                     "${draco_build_dir}/testing/draco_test_config.h")
      add_subdirectory("${GTEST_SOURCE_DIR}" "${GTEST_BUILD_DIR}"
                       EXCLUDE_FROM_ALL)
    endif ()

    include_directories("${GTEST_SOURCE_DIR}")
  endif ()
endif ()

# Draco source file listing variables.
set(draco_attributes_sources
    "${draco_src_root}/attributes/attribute_octahedron_transform.cc"
    "${draco_src_root}/attributes/attribute_octahedron_transform.h"
    "${draco_src_root}/attributes/attribute_quantization_transform.cc"
    "${draco_src_root}/attributes/attribute_quantization_transform.h"
    "${draco_src_root}/attributes/attribute_transform.cc"
    "${draco_src_root}/attributes/attribute_transform.h"
    "${draco_src_root}/attributes/attribute_transform_data.h"
    "${draco_src_root}/attributes/attribute_transform_type.h"
    "${draco_src_root}/attributes/geometry_attribute.cc"
    "${draco_src_root}/attributes/geometry_attribute.h"
    "${draco_src_root}/attributes/geometry_indices.h"
    "${draco_src_root}/attributes/point_attribute.cc"
    "${draco_src_root}/attributes/point_attribute.h")

set(draco_compression_attributes_dec_sources
    "${draco_src_root}/compression/attributes/attributes_decoder.cc"
    "${draco_src_root}/compression/attributes/attributes_decoder.h"
    "${draco_src_root}/compression/attributes/kd_tree_attributes_decoder.cc"
    "${draco_src_root}/compression/attributes/kd_tree_attributes_decoder.h"
    "${draco_src_root}/compression/attributes/kd_tree_attributes_shared.h"
    "${draco_src_root}/compression/attributes/mesh_attribute_indices_encoding_data.h"
    "${draco_src_root}/compression/attributes/mesh_traversal_sequencer.h"
    "${draco_src_root}/compression/attributes/normal_compression_utils.h"
    "${draco_src_root}/compression/attributes/sequential_attribute_decoder.cc"
    "${draco_src_root}/compression/attributes/sequential_attribute_decoder.h"
    "${draco_src_root}/compression/attributes/sequential_attribute_decoders_controller.cc"
    "${draco_src_root}/compression/attributes/sequential_attribute_decoders_controller.h"
    "${draco_src_root}/compression/attributes/sequential_integer_attribute_decoder.cc"
    "${draco_src_root}/compression/attributes/sequential_integer_attribute_decoder.h"
    "${draco_src_root}/compression/attributes/sequential_normal_attribute_decoder.cc"
    "${draco_src_root}/compression/attributes/sequential_normal_attribute_decoder.h"
    "${draco_src_root}/compression/attributes/sequential_quantization_attribute_decoder.cc"
    "${draco_src_root}/compression/attributes/sequential_quantization_attribute_decoder.h")

set(draco_compression_attributes_enc_sources
    "${draco_src_root}/compression/attributes/attributes_encoder.cc"
    "${draco_src_root}/compression/attributes/attributes_encoder.h"
    "${draco_src_root}/compression/attributes/kd_tree_attributes_encoder.cc"
    "${draco_src_root}/compression/attributes/kd_tree_attributes_encoder.h"
    "${draco_src_root}/compression/attributes/linear_sequencer.h"
    "${draco_src_root}/compression/attributes/mesh_attribute_indices_encoding_observer.h"
    "${draco_src_root}/compression/attributes/points_sequencer.h"
    "${draco_src_root}/compression/attributes/sequential_attribute_encoder.cc"
    "${draco_src_root}/compression/attributes/sequential_attribute_encoder.h"
    "${draco_src_root}/compression/attributes/sequential_attribute_encoders_controller.cc"
    "${draco_src_root}/compression/attributes/sequential_attribute_encoders_controller.h"
    "${draco_src_root}/compression/attributes/sequential_integer_attribute_encoder.cc"
    "${draco_src_root}/compression/attributes/sequential_integer_attribute_encoder.h"
    "${draco_src_root}/compression/attributes/sequential_normal_attribute_encoder.cc"
    "${draco_src_root}/compression/attributes/sequential_normal_attribute_encoder.h"
    "${draco_src_root}/compression/attributes/sequential_quantization_attribute_encoder.cc"
    "${draco_src_root}/compression/attributes/sequential_quantization_attribute_encoder.h")

set(draco_compression_attributes_pred_schemes_dec_sources
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_constrained_multi_parallelogram_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_constrained_multi_parallelogram_shared.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_data.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_predictor_area.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_predictor_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_multi_parallelogram_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_shared.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_portable_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_portable_predictor.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_decoder_factory.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_decoder_interface.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_decoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_delta_decoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_factory.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_interface.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_canonicalized_decoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_canonicalized_transform_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_decoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_transform_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_wrap_decoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_wrap_transform_base.h")

set(draco_compression_attributes_pred_schemes_enc_sources
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_constrained_multi_parallelogram_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_constrained_multi_parallelogram_shared.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_data.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_predictor_area.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_geometric_normal_predictor_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_multi_parallelogram_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_shared.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_portable_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords_portable_predictor.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_delta_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.cc"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_encoder_interface.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_encoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_factory.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_interface.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_canonicalized_encoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_canonicalized_transform_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_encoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_transform_base.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_wrap_encoding_transform.h"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_wrap_transform_base.h")

set(draco_enc_config_sources
    "${draco_src_root}/compression/config/compression_shared.h"
    "${draco_src_root}/compression/config/draco_options.h"
    "${draco_src_root}/compression/config/encoder_options.h"
    "${draco_src_root}/compression/config/encoding_features.h")

set(draco_dec_config_sources
    "${draco_src_root}/compression/config/compression_shared.h"
    "${draco_src_root}/compression/config/decoder_options.h"
    "${draco_src_root}/compression/config/draco_options.h")

set(draco_compression_decode_sources
    "${draco_src_root}/compression/decode.cc"
    "${draco_src_root}/compression/decode.h")

set(draco_compression_encode_sources
    "${draco_src_root}/compression/encode.cc"
    "${draco_src_root}/compression/encode.h"
    "${draco_src_root}/compression/encode_base.h"
    "${draco_src_root}/compression/expert_encode.cc"
    "${draco_src_root}/compression/expert_encode.h")

set(draco_compression_mesh_dec_sources
    "${draco_src_root}/compression/mesh/mesh_decoder.cc"
    "${draco_src_root}/compression/mesh/mesh_decoder.h"
    "${draco_src_root}/compression/mesh/mesh_decoder_helpers.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_decoder.cc"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_decoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_decoder_impl.cc"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_decoder_impl.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_decoder_impl_interface.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_shared.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_decoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_predictive_decoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_valence_decoder.h"
    "${draco_src_root}/compression/mesh/mesh_sequential_decoder.cc"
    "${draco_src_root}/compression/mesh/mesh_sequential_decoder.h")

set(draco_compression_mesh_enc_sources
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoder.cc"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoder_impl.cc"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoder_impl.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoder_impl_interface.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_shared.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_encoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_predictive_encoder.h"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_traversal_valence_encoder.h"
    "${draco_src_root}/compression/mesh/mesh_encoder.cc"
    "${draco_src_root}/compression/mesh/mesh_encoder.h"
    "${draco_src_root}/compression/mesh/mesh_encoder_helpers.h"
    "${draco_src_root}/compression/mesh/mesh_sequential_encoder.cc"
    "${draco_src_root}/compression/mesh/mesh_sequential_encoder.h")

set(draco_compression_point_cloud_dec_sources
    "${draco_src_root}/compression/point_cloud/point_cloud_decoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_decoder.h"
    "${draco_src_root}/compression/point_cloud/point_cloud_kd_tree_decoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_kd_tree_decoder.h"
    "${draco_src_root}/compression/point_cloud/point_cloud_sequential_decoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_sequential_decoder.h")

set(draco_compression_point_cloud_enc_sources
    "${draco_src_root}/compression/point_cloud/point_cloud_encoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_encoder.h"
    "${draco_src_root}/compression/point_cloud/point_cloud_kd_tree_encoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_kd_tree_encoder.h"
    "${draco_src_root}/compression/point_cloud/point_cloud_sequential_encoder.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_sequential_encoder.h")

set(draco_core_sources
    "${draco_src_root}/core/ans.h"
    "${draco_src_root}/core/bit_utils.h"
    "${draco_src_root}/core/cycle_timer.cc"
    "${draco_src_root}/core/cycle_timer.h"
    "${draco_src_root}/core/data_buffer.cc"
    "${draco_src_root}/core/data_buffer.h"
    "${draco_src_root}/core/decoder_buffer.cc"
    "${draco_src_root}/core/decoder_buffer.h"
    "${draco_src_root}/core/divide.cc"
    "${draco_src_root}/core/divide.h"
    "${draco_src_root}/core/draco_index_type.h"
    "${draco_src_root}/core/draco_index_type_vector.h"
    "${draco_src_root}/core/draco_types.cc"
    "${draco_src_root}/core/draco_types.h"
    "${draco_src_root}/core/encoder_buffer.cc"
    "${draco_src_root}/core/encoder_buffer.h"
    "${draco_src_root}/core/hash_utils.cc"
    "${draco_src_root}/core/hash_utils.h"
    "${draco_src_root}/core/macros.h"
    "${draco_src_root}/core/math_utils.h"
    "${draco_src_root}/core/options.cc"
    "${draco_src_root}/core/options.h"
    "${draco_src_root}/core/quantization_utils.cc"
    "${draco_src_root}/core/quantization_utils.h"
    "${draco_src_root}/core/rans_symbol_coding.h"
    "${draco_src_root}/core/rans_symbol_decoder.h"
    "${draco_src_root}/core/rans_symbol_encoder.h"
    "${draco_src_root}/core/shannon_entropy.cc"
    "${draco_src_root}/core/shannon_entropy.h"
    "${draco_src_root}/core/status.h"
    "${draco_src_root}/core/statusor.h"
    "${draco_src_root}/core/symbol_coding_utils.cc"
    "${draco_src_root}/core/symbol_coding_utils.h"
    "${draco_src_root}/core/symbol_decoding.cc"
    "${draco_src_root}/core/symbol_decoding.h"
    "${draco_src_root}/core/symbol_encoding.cc"
    "${draco_src_root}/core/symbol_encoding.h"
    "${draco_src_root}/core/varint_decoding.h"
    "${draco_src_root}/core/varint_encoding.h"
    "${draco_src_root}/core/vector_d.h")

set(draco_core_bit_coders_sources
    "${draco_src_root}/core/bit_coders/adaptive_rans_bit_coding_shared.h"
    "${draco_src_root}/core/bit_coders/adaptive_rans_bit_decoder.cc"
    "${draco_src_root}/core/bit_coders/adaptive_rans_bit_decoder.h"
    "${draco_src_root}/core/bit_coders/adaptive_rans_bit_encoder.cc"
    "${draco_src_root}/core/bit_coders/adaptive_rans_bit_encoder.h"
    "${draco_src_root}/core/bit_coders/direct_bit_decoder.cc"
    "${draco_src_root}/core/bit_coders/direct_bit_decoder.h"
    "${draco_src_root}/core/bit_coders/direct_bit_encoder.cc"
    "${draco_src_root}/core/bit_coders/direct_bit_encoder.h"
    "${draco_src_root}/core/bit_coders/folded_integer_bit_decoder.h"
    "${draco_src_root}/core/bit_coders/folded_integer_bit_encoder.h"
    "${draco_src_root}/core/bit_coders/rans_bit_decoder.cc"
    "${draco_src_root}/core/bit_coders/rans_bit_decoder.h"
    "${draco_src_root}/core/bit_coders/rans_bit_encoder.cc"
    "${draco_src_root}/core/bit_coders/rans_bit_encoder.h"
    "${draco_src_root}/core/bit_coders/symbol_bit_decoder.cc"
    "${draco_src_root}/core/bit_coders/symbol_bit_decoder.h"
    "${draco_src_root}/core/bit_coders/symbol_bit_encoder.cc"
    "${draco_src_root}/core/bit_coders/symbol_bit_encoder.h")

set(draco_io_sources
    "${draco_src_root}/io/mesh_io.cc"
    "${draco_src_root}/io/mesh_io.h"
    "${draco_src_root}/io/obj_decoder.cc"
    "${draco_src_root}/io/obj_decoder.h"
    "${draco_src_root}/io/obj_encoder.cc"
    "${draco_src_root}/io/obj_encoder.h"
    "${draco_src_root}/io/parser_utils.cc"
    "${draco_src_root}/io/parser_utils.h"
    "${draco_src_root}/io/ply_decoder.cc"
    "${draco_src_root}/io/ply_decoder.h"
    "${draco_src_root}/io/ply_encoder.cc"
    "${draco_src_root}/io/ply_encoder.h"
    "${draco_src_root}/io/ply_property_reader.h"
    "${draco_src_root}/io/ply_property_writer.h"
    "${draco_src_root}/io/ply_reader.cc"
    "${draco_src_root}/io/ply_reader.h"
    "${draco_src_root}/io/point_cloud_io.cc"
    "${draco_src_root}/io/point_cloud_io.h")

set(draco_mesh_sources
    "${draco_src_root}/mesh/corner_table.cc"
    "${draco_src_root}/mesh/corner_table.h"
    "${draco_src_root}/mesh/corner_table_iterators.h"
    "${draco_src_root}/mesh/corner_table_traversal_processor.h"
    "${draco_src_root}/mesh/edgebreaker_observer.h"
    "${draco_src_root}/mesh/edgebreaker_traverser.h"
    "${draco_src_root}/mesh/mesh.cc"
    "${draco_src_root}/mesh/mesh.h"
    "${draco_src_root}/mesh/mesh_are_equivalent.cc"
    "${draco_src_root}/mesh/mesh_are_equivalent.h"
    "${draco_src_root}/mesh/mesh_attribute_corner_table.cc"
    "${draco_src_root}/mesh/mesh_attribute_corner_table.h"
    "${draco_src_root}/mesh/mesh_cleanup.cc"
    "${draco_src_root}/mesh/mesh_cleanup.h"
    "${draco_src_root}/mesh/mesh_misc_functions.cc"
    "${draco_src_root}/mesh/mesh_misc_functions.h"
    "${draco_src_root}/mesh/mesh_stripifier.cc"
    "${draco_src_root}/mesh/mesh_stripifier.h"
    "${draco_src_root}/mesh/triangle_soup_mesh_builder.cc"
    "${draco_src_root}/mesh/triangle_soup_mesh_builder.h")

set(draco_point_cloud_sources
    "${draco_src_root}/point_cloud/point_cloud.cc"
    "${draco_src_root}/point_cloud/point_cloud.h"
    "${draco_src_root}/point_cloud/point_cloud_builder.cc"
    "${draco_src_root}/point_cloud/point_cloud_builder.h")

set(draco_points_common_sources
    "${draco_src_root}/compression/point_cloud/algorithms/point_cloud_compression_method.h"
    "${draco_src_root}/compression/point_cloud/algorithms/point_cloud_types.h"
    "${draco_src_root}/compression/point_cloud/algorithms/quantize_points_3.h"
    "${draco_src_root}/compression/point_cloud/algorithms/queuing_policy.h")

set(draco_points_dec_sources
    "${draco_src_root}/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.cc"
    "${draco_src_root}/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.h"
    "${draco_src_root}/compression/point_cloud/algorithms/float_points_tree_decoder.cc"
    "${draco_src_root}/compression/point_cloud/algorithms/float_points_tree_decoder.h")

set(draco_points_enc_sources
    "${draco_src_root}/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_encoder.cc"
    "${draco_src_root}/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_encoder.h"
    "${draco_src_root}/compression/point_cloud/algorithms/float_points_tree_encoder.cc"
    "${draco_src_root}/compression/point_cloud/algorithms/float_points_tree_encoder.h")

set(draco_metadata_sources
    "${draco_src_root}/metadata/geometry_metadata.cc"
    "${draco_src_root}/metadata/geometry_metadata.h"
    "${draco_src_root}/metadata/metadata.cc"
    "${draco_src_root}/metadata/metadata.h")

set(draco_metadata_enc_sources
    "${draco_src_root}/metadata/metadata_encoder.cc"
    "${draco_src_root}/metadata/metadata_encoder.h")

set(draco_metadata_dec_sources
    "${draco_src_root}/metadata/metadata_decoder.cc"
    "${draco_src_root}/metadata/metadata_decoder.h")


set(draco_js_dec_sources
    "${draco_src_root}/javascript/emscripten/decoder_webidl_wrapper.cc"
    "${draco_src_root}/javascript/emscripten/draco_decoder_glue_wrapper.cc")

set(draco_js_enc_sources
    "${draco_src_root}/javascript/emscripten/draco_encoder_glue_wrapper.cc"
    "${draco_src_root}/javascript/emscripten/encoder_webidl_wrapper.cc")


set(draco_test_sources
    "${draco_src_root}/attributes/point_attribute_test.cc"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_canonicalized_transform_test.cc"
    "${draco_src_root}/compression/attributes/prediction_schemes/prediction_scheme_normal_octahedron_transform_test.cc"
    "${draco_src_root}/compression/attributes/sequential_integer_attribute_encoding_test.cc"
    "${draco_src_root}/compression/decode_test.cc"
    "${draco_src_root}/compression/encode_test.cc"
    "${draco_src_root}/compression/mesh/mesh_edgebreaker_encoding_test.cc"
    "${draco_src_root}/compression/mesh/mesh_encoder_test.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_kd_tree_encoding_test.cc"
    "${draco_src_root}/compression/point_cloud/point_cloud_sequential_encoding_test.cc"
    "${draco_src_root}/core/bit_coders/rans_coding_test.cc"
    "${draco_src_root}/core/buffer_bit_coding_test.cc"
    "${draco_src_root}/core/draco_test_base.h"
    "${draco_src_root}/core/draco_test_utils.cc"
    "${draco_src_root}/core/draco_test_utils.h"
    "${draco_src_root}/core/draco_tests.cc"
    "${draco_src_root}/core/math_utils_test.cc"
    "${draco_src_root}/core/quantization_utils_test.cc"
    "${draco_src_root}/core/status_test.cc"
    "${draco_src_root}/core/symbol_coding_test.cc"
    "${draco_src_root}/core/vector_d_test.cc"
    "${draco_src_root}/io/obj_decoder_test.cc"
    "${draco_src_root}/io/obj_encoder_test.cc"
    "${draco_src_root}/io/ply_decoder_test.cc"
    "${draco_src_root}/io/ply_reader_test.cc"
    "${draco_src_root}/io/point_cloud_io_test.cc"
    "${draco_src_root}/mesh/mesh_are_equivalent_test.cc"
    "${draco_src_root}/mesh/mesh_cleanup_test.cc"
    "${draco_src_root}/mesh/triangle_soup_mesh_builder_test.cc"
    "${draco_src_root}/metadata/metadata_encoder_test.cc"
    "${draco_src_root}/metadata/metadata_test.cc"
    "${draco_src_root}/point_cloud/point_cloud_builder_test.cc"
    "${draco_src_root}/point_cloud/point_cloud_test.cc")

set(draco_version_sources
    "${draco_build_dir}/draco_version.cc"
    "${draco_build_dir}/draco_version.h")

set(draco_unity_plug_sources
    "${draco_src_root}/unity/draco_unity_plugin.cc"
    "${draco_src_root}/unity/draco_unity_plugin.h")

include_directories("${draco_root}/src")

#
# Draco targets.
#
if (EMSCRIPTEN)
  # Draco js decoder.
  require_compiler_flag("-s ALLOW_MEMORY_GROWTH=1" YES)
  require_compiler_flag("-Wno-almost-asm" YES)
  require_compiler_flag("--memory-init-file 0" YES)
  require_compiler_flag("-fno-omit-frame-pointer" YES)
  require_compiler_flag("-s MODULARIZE=1" YES)
  if (ENABLE_EXTRA_SPEED)
    require_compiler_flag("--llvm-lto 1" YES)
  endif ()
  require_compiler_flag("-s NO_FILESYSTEM=1" YES)
  require_compiler_flag("-s EXPORTED_RUNTIME_METHODS=[]" YES)
  require_compiler_flag("-s PRECISE_F32=1" YES)
  if (ENABLE_WASM)
    require_compiler_flag("-s WASM=1" YES)
  else ()
    require_compiler_flag("-s ELIMINATE_DUPLICATE_FUNCTIONS=1" YES)
  endif ()

  if (CMAKE_BUILD_TYPE STREQUAL "")
    # Force -O3 when no build type is specified.
    add_compiler_flag_if_supported("-O3")
  endif ()

  set(draco_js_dec_idl
      "${draco_src_root}/javascript/emscripten/draco_web_decoder.idl")
  set(draco_js_enc_idl
      "${draco_src_root}/javascript/emscripten/draco_web_encoder.idl")

  # Generate ${draco_build_dir}/glue_decoder.cpp at cmake generation time so it
  # can be added to targets without cmake reporting errors.
  execute_process(COMMAND ${PYTHON_EXECUTABLE}
                  $ENV{EMSCRIPTEN}/tools/webidl_binder.py
                  ${draco_js_dec_idl}
                  ${draco_build_dir}/glue_decoder)

  execute_process(COMMAND ${PYTHON_EXECUTABLE}
                  $ENV{EMSCRIPTEN}/tools/webidl_binder.py
                  ${draco_js_enc_idl}
                  ${draco_build_dir}/glue_encoder)

  if (NOT EXISTS "${draco_build_dir}/glue_decoder.cpp" OR
      NOT EXISTS "${draco_build_dir}/glue_encoder.cpp")
    message(FATAL_ERROR "Glue generation failed.")
  endif ()


  # Add a custom rule depending on the IDL to regenerate
  # ${draco_build_dir}/glue_decoder.cpp as needed.
  add_custom_command(OUTPUT ${draco_build_dir}/glue_decoder.cpp
                     COMMAND ${PYTHON_EXECUTABLE}
                     $ENV{EMSCRIPTEN}/tools/webidl_binder.py
                     ${draco_js_dec_idl}
                     ${draco_build_dir}/glue_decoder
                     DEPENDS ${draco_js_dec_idl}
                     COMMENT "Generating ${draco_build_dir}/glue_decoder.cpp."
                     WORKING_DIRECTORY ${draco_build_dir}
                     VERBATIM)

  add_custom_command(OUTPUT ${draco_build_dir}/glue_encoder.cpp
                     COMMAND ${PYTHON_EXECUTABLE}
                     $ENV{EMSCRIPTEN}/tools/webidl_binder.py
                     ${draco_js_enc_idl}
                     ${draco_build_dir}/glue_encoder
                     DEPENDS ${draco_js_enc_idl}
                     COMMENT "Generating ${draco_build_dir}/glue_encoder.cpp."
                     WORKING_DIRECTORY ${draco_build_dir}
                     VERBATIM)


  # Add path to glue_decoder.cpp to draco include paths.
  include_directories("${draco_build_dir}")

  set(draco_decoder_src
      ${draco_attributes_sources}
      ${draco_compression_attributes_dec_sources}
      ${draco_compression_attributes_pred_schemes_dec_sources}
      ${draco_compression_decode_sources}
      ${draco_compression_mesh_dec_sources}
      ${draco_compression_point_cloud_dec_sources}
      ${draco_core_sources}
      ${draco_core_bit_coders_sources}
      ${draco_dec_config_sources}
      ${draco_io_sources}
      ${draco_mesh_sources}
      ${draco_metadata_dec_sources}
      ${draco_metadata_sources}
      ${draco_point_cloud_sources}
      ${draco_points_dec_sources}
      ${draco_version_sources})
  set(draco_encoder_src
      ${draco_attributes_sources}
      ${draco_compression_attributes_enc_sources}
      ${draco_compression_attributes_pred_schemes_enc_sources}
      ${draco_compression_encode_sources}
      ${draco_compression_mesh_enc_sources}
      ${draco_compression_point_cloud_enc_sources}
      ${draco_core_sources}
      ${draco_core_bit_coders_sources}
      ${draco_enc_config_sources}
      ${draco_io_sources}
      ${draco_mesh_sources}
      ${draco_metadata_enc_sources}
      ${draco_metadata_sources}
      ${draco_point_cloud_sources}
      ${draco_points_enc_sources}
      ${draco_version_sources})


  add_executable(draco_decoder
                 ${draco_js_dec_sources}
                 ${draco_decoder_src})
  append_link_flag_to_target(draco_decoder
                             "-s EXPORT_NAME=\"'DracoDecoderModule'\"")

  add_executable(draco_encoder
                 ${draco_js_enc_sources}
                 ${draco_encoder_src})
  append_link_flag_to_target(draco_encoder
                             "-s EXPORT_NAME=\"'DracoEncoderModule'\"")

  target_compile_definitions(draco_encoder PRIVATE
                             DRACO_ATTRIBUTE_DEDUPLICATION_SUPPORTED)
  if (ENABLE_DECODER_ATTRIBUTE_DEDUPLICATION)
    target_compile_definitions(draco_decoder PRIVATE
                               DRACO_ATTRIBUTE_DEDUPLICATION_SUPPORTED)
  endif ()

  # Make $draco_js_sources source files depend on glue_decoder.cpp.
  set_property(SOURCE ${draco_js_sources} APPEND PROPERTY OBJECT_DEPENDS
               ${draco_build_dir}/glue_decoder.cpp)
  em_link_pre_js(draco_decoder
                 "${draco_src_root}/javascript/emscripten/prepareCallbacks.js"
                 "${draco_src_root}/javascript/emscripten/version.js")
  em_link_post_js(draco_decoder
                  "${draco_build_dir}/glue_decoder.js"
                  "${draco_src_root}/javascript/emscripten/finalize.js")

  set_property(SOURCE ${draco_js_enc_sources} APPEND PROPERTY OBJECT_DEPENDS
               ${draco_build_dir}/glue_encoder.cpp)
  em_link_pre_js(draco_encoder
                 "${draco_src_root}/javascript/emscripten/prepareCallbacks.js"
                 "${draco_src_root}/javascript/emscripten/version.js")
  em_link_post_js(draco_encoder
                  "${draco_build_dir}/glue_encoder.js"
                  "${draco_src_root}/javascript/emscripten/finalize.js")

else ()
  # Standard Draco libs, encoder and decoder.
  # Object collections that mirror the Draco directory structure.
  add_library(draco_attributes OBJECT ${draco_attributes_sources})
  add_library(draco_compression_attributes_dec OBJECT
              ${draco_compression_attributes_dec_sources})
  add_library(draco_compression_attributes_enc OBJECT
              ${draco_compression_attributes_enc_sources})
  add_library(draco_compression_attributes_pred_schemes_dec OBJECT
              ${draco_compression_attributes_pred_schemes_dec_sources})
  add_library(draco_compression_attributes_pred_schemes_enc OBJECT
              ${draco_compression_attributes_pred_schemes_enc_sources})
  add_library(draco_enc_config OBJECT
              ${draco_enc_config_sources})
  add_library(draco_dec_config OBJECT
              ${draco_dec_config_sources})
  add_library(draco_compression_decode OBJECT
              ${draco_compression_decode_sources})
  add_library(draco_compression_encode OBJECT
              ${draco_compression_encode_sources})
  add_library(draco_compression_mesh_dec OBJECT
              ${draco_compression_mesh_dec_sources})
  add_library(draco_compression_mesh_enc OBJECT
              ${draco_compression_mesh_enc_sources})
  add_library(draco_compression_point_cloud_dec OBJECT
              ${draco_compression_point_cloud_dec_sources})
  add_library(draco_compression_point_cloud_enc OBJECT
              ${draco_compression_point_cloud_enc_sources})
  add_library(draco_core OBJECT ${draco_core_sources})
  add_library(draco_core_bit_coders OBJECT ${draco_core_bit_coders_sources})
  add_library(draco_io OBJECT ${draco_io_sources})
  add_library(draco_mesh OBJECT ${draco_mesh_sources})
  add_library(draco_metadata_dec OBJECT ${draco_metadata_dec_sources})
  add_library(draco_metadata_enc OBJECT ${draco_metadata_enc_sources})
  add_library(draco_metadata OBJECT ${draco_metadata_sources})
  add_library(draco_point_cloud OBJECT ${draco_point_cloud_sources})
  add_library(draco_points_dec OBJECT
              ${draco_points_common_sources}
              ${draco_points_dec_sources})
  add_library(draco_points_enc OBJECT
              ${draco_points_common_sources}
              ${draco_points_enc_sources})
  add_library(draco_unity_plugin OBJECT
              ${draco_unity_plug_sources})

  # Library targets that consume the object collections.
  add_library(dracodec
              ${draco_version_sources}
              $<TARGET_OBJECTS:draco_attributes>
              $<TARGET_OBJECTS:draco_compression_attributes_dec>
              $<TARGET_OBJECTS:draco_compression_decode>
              $<TARGET_OBJECTS:draco_compression_mesh_dec>
              $<TARGET_OBJECTS:draco_compression_point_cloud_dec>
              $<TARGET_OBJECTS:draco_core>
              $<TARGET_OBJECTS:draco_core_bit_coders>
              $<TARGET_OBJECTS:draco_dec_config>
              $<TARGET_OBJECTS:draco_io>
              $<TARGET_OBJECTS:draco_mesh>
              $<TARGET_OBJECTS:draco_metadata>
              $<TARGET_OBJECTS:draco_metadata_dec>
              $<TARGET_OBJECTS:draco_point_cloud>
              $<TARGET_OBJECTS:draco_points_dec>)
  add_library(dracoenc
              ${draco_version_sources}
              $<TARGET_OBJECTS:draco_attributes>
              $<TARGET_OBJECTS:draco_compression_attributes_enc>
              $<TARGET_OBJECTS:draco_compression_attributes_pred_schemes_enc>
              $<TARGET_OBJECTS:draco_compression_encode>
              $<TARGET_OBJECTS:draco_compression_mesh_enc>
              $<TARGET_OBJECTS:draco_compression_point_cloud_enc>
              $<TARGET_OBJECTS:draco_core>
              $<TARGET_OBJECTS:draco_core_bit_coders>
              $<TARGET_OBJECTS:draco_enc_config>
              $<TARGET_OBJECTS:draco_mesh>
              $<TARGET_OBJECTS:draco_metadata>
              $<TARGET_OBJECTS:draco_metadata_enc>
              $<TARGET_OBJECTS:draco_point_cloud>
              $<TARGET_OBJECTS:draco_points_enc>)
  add_library(draco
              ${draco_version_sources}
              $<TARGET_OBJECTS:draco_attributes>
              $<TARGET_OBJECTS:draco_compression_attributes_dec>
              $<TARGET_OBJECTS:draco_compression_attributes_enc>
              $<TARGET_OBJECTS:draco_compression_attributes_pred_schemes_dec>
              $<TARGET_OBJECTS:draco_compression_attributes_pred_schemes_enc>
              $<TARGET_OBJECTS:draco_enc_config>
              $<TARGET_OBJECTS:draco_dec_config>
              $<TARGET_OBJECTS:draco_compression_decode>
              $<TARGET_OBJECTS:draco_compression_encode>
              $<TARGET_OBJECTS:draco_compression_mesh_dec>
              $<TARGET_OBJECTS:draco_compression_mesh_enc>
              $<TARGET_OBJECTS:draco_compression_point_cloud_dec>
              $<TARGET_OBJECTS:draco_compression_point_cloud_enc>
              $<TARGET_OBJECTS:draco_core>
              $<TARGET_OBJECTS:draco_core_bit_coders>
              $<TARGET_OBJECTS:draco_io>
              $<TARGET_OBJECTS:draco_mesh>
              $<TARGET_OBJECTS:draco_metadata>
              $<TARGET_OBJECTS:draco_metadata_dec>
              $<TARGET_OBJECTS:draco_metadata_enc>
              $<TARGET_OBJECTS:draco_point_cloud>
              $<TARGET_OBJECTS:draco_points_dec>
              $<TARGET_OBJECTS:draco_points_enc>)
  if (BUILD_UNITY_PLUGIN)
    add_library(dracodec_unity
                MODULE
                ${draco_version_sources}
                $<TARGET_OBJECTS:draco_unity_plugin>
                $<TARGET_OBJECTS:draco_attributes>
                $<TARGET_OBJECTS:draco_compression_attributes_dec>
                $<TARGET_OBJECTS:draco_compression_decode>
                $<TARGET_OBJECTS:draco_compression_mesh_dec>
                $<TARGET_OBJECTS:draco_compression_point_cloud_dec>
                $<TARGET_OBJECTS:draco_core>
                $<TARGET_OBJECTS:draco_core_bit_coders>
                $<TARGET_OBJECTS:draco_dec_config>
                $<TARGET_OBJECTS:draco_io>
                $<TARGET_OBJECTS:draco_mesh>
                $<TARGET_OBJECTS:draco_metadata>
                $<TARGET_OBJECTS:draco_metadata_dec>
                $<TARGET_OBJECTS:draco_point_cloud>
                $<TARGET_OBJECTS:draco_points_dec>)
    # For Mac, we need to build a .bundle for plugin.
    if (APPLE)
      set_target_properties(dracodec_unity PROPERTIES BUNDLE true)
    endif ()
  endif ()
  set(draco_header_only_targets
      draco_compression_attributes_pred_schemes_dec
      draco_dec_config
      draco_enc_config)

  # For now, enable deduplication for both encoder and decoder.
  # TODO(ostava): Support for disabling attribute deduplication for the C++
  # decoder is planned in future releases.
  add_preproc_definition(DRACO_ATTRIBUTE_DEDUPLICATION_SUPPORTED)

  if (BUILD_SHARED_LIBS)
    set_target_properties(dracodec PROPERTIES SOVERSION 1)
    set_target_properties(dracoenc PROPERTIES SOVERSION 1)
    set_target_properties(draco PROPERTIES SOVERSION 1)
    if (BUILD_UNITY_PLUGIN)
      set_target_properties(dracodec_unity PROPERTIES SOVERSION 1)
    endif ()
  endif ()

  extract_version_string("${draco_src_root}/core/draco_version.h" draco_version)

  if (NOT MSVC)
    # Generate draco.pc.
    set(prefix "${CMAKE_INSTALL_PREFIX}")
    set(pkgconfig_file "${draco_build_dir}/draco.pc")
    file(WRITE "${pkgconfig_file}" "# draco pkg-config.\n")
    file(APPEND "${pkgconfig_file}" "prefix=${prefix}\n")
    file(APPEND "${pkgconfig_file}" "exec_prefix=${prefix}/bin\n")
    file(APPEND "${pkgconfig_file}" "libdir=${prefix}/lib\n")
    file(APPEND "${pkgconfig_file}" "includedir=${prefix}/include\n\n")
    file(APPEND "${pkgconfig_file}" "Name: Draco\n")
    file(APPEND "${pkgconfig_file}" "Description: 3D Compression.\n")
    file(APPEND "${pkgconfig_file}" "Version: ${draco_version}\n")
    file(APPEND "${pkgconfig_file}" "Requires:\n")
    file(APPEND "${pkgconfig_file}" "Conflicts:\n")
    file(APPEND "${pkgconfig_file}" "Libs: -L${prefix}/lib -ldraco\n")
    file(APPEND "${pkgconfig_file}" "Cflags: -I${prefix}/include -std=c++11\n")
  endif ()

  # Draco app targets.
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  SET(BUILD_SHARED_LIBRARIES OFF)
  SET(CMAKE_EXE_LINKER_FLAGS "-static")
  add_executable(draco_decoder "${draco_src_root}/tools/draco_decoder.cc")
  target_link_libraries(draco_decoder PRIVATE dracodec)
  add_executable(draco_encoder
                 "${draco_src_root}/tools/draco_encoder.cc")
  target_link_libraries(draco_encoder PRIVATE draco)

  if (ENABLE_TESTS)
    add_executable(draco_tests ${draco_test_sources})
    include_directories("${draco_build_dir}"
                        "${GTEST_SOURCE_DIR}/googletest/include")
    target_link_libraries(draco_tests draco gtest)
  endif ()

  # Collect all of the header files in the tree, and add an install rule for
  # each.
  file(GLOB_RECURSE draco_headers RELATIVE ${draco_root}/src/draco "*.h")
  foreach (filename ${draco_headers})
    get_filename_component(file_directory ${filename} DIRECTORY)
    install(FILES src/draco/${filename} DESTINATION
            "${CMAKE_INSTALL_PREFIX}/include/draco/${file_directory}")
  endforeach()

  if (NOT MSVC)
    # Install our pkg-config file.
    install(FILES "${draco_build_dir}/draco.pc"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
  endif ()

  # Add install rules for lib and executable targets.
  install(TARGETS dracodec dracoenc draco
          DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
  if (BUILD_UNITY_PLUGIN)
    install(TARGETS dracodec_unity
            DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
  endif ()
  install(TARGETS draco_decoder draco_encoder DESTINATION
          "${CMAKE_INSTALL_PREFIX}/bin")

  # Generate a CMake config file.
  set(draco_include_install_dir "include/draco")
  set(draco_lib_install_dir "lib")
  set(draco_script_install_dir
      "${CMAKE_INSTALL_PREFIX}/${draco_lib_install_dir}/draco/cmake")
  set(draco_config_in_file "${draco_root}/cmake/DracoConfig.cmake")
  set(draco_config_out_file "${draco_build_dir}/DracoConfig.cmake")

  configure_package_config_file(
      "${draco_config_in_file}"
      "${draco_config_out_file}"
      NO_CHECK_REQUIRED_COMPONENTS_MACRO
      INSTALL_DESTINATION "${draco_lib_install_dir}/draco/cmake"
      PATH_VARS draco_include_install_dir draco_lib_install_dir)
  write_basic_package_version_file(
      "${draco_build_dir}/DracoConfigVersion.cmake"
      VERSION ${draco_version}
      COMPATIBILITY SameMajorVersion)

  # Read in the config file and rewrite it with a version string included.
  file(STRINGS "${draco_config_out_file}" draco_config_strings)
  file(WRITE "${draco_config_out_file}"
       "set(draco_VERSION \"${draco_version}\")\n")
  foreach (config_string ${draco_config_strings})
    file(APPEND "${draco_config_out_file}" "${config_string}\n")
  endforeach ()

  # Add install rules for CMake helper scripts.
  set(draco_cmake_helper_scripts
      "${draco_root}/cmake/FindDraco.cmake"
      "${draco_build_dir}/DracoConfig.cmake"
      "${draco_build_dir}/DracoConfigVersion.cmake")
  install(FILES ${draco_cmake_helper_scripts}
          DESTINATION "${draco_script_install_dir}")

  # Some generators complain about unknown link language for header only
  # targets. Silence the harmless warnings/errors with some dummy source files.
  foreach (target ${draco_header_only_targets})
    add_dummy_source_file_to_target("${target}" "cc")
  endforeach ()
endif ()
