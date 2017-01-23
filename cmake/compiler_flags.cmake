cmake_minimum_required(VERSION 3.2)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Strings used to cache failed C/CXX flags.
set(DRACO_FAILED_C_FLAGS)
set(DRACO_FAILED_CXX_FLAGS)

# Checks C compiler for support of $c_flag. Adds $c_flag to $CMAKE_C_FLAGS when
# the compile test passes. Caches $c_flag in $DRACO_FAILED_C_FLAGS when the test
# fails.
function (add_c_flag_if_supported c_flag)
  unset(C_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_C_FLAGS}" "${c_flag}" C_FLAG_FOUND)
  unset(C_FLAG_FAILED CACHE)
  string(FIND "${DRACO_FAILED_C_FLAGS}" "${c_flag}" C_FLAG_FAILED)

  if (${C_FLAG_FOUND} EQUAL -1 AND ${C_FLAG_FAILED} EQUAL -1)
    unset(C_FLAG_SUPPORTED CACHE)
    message("Checking C compiler flag support for: " ${c_flag})
    check_c_compiler_flag("${c_flag}" C_FLAG_SUPPORTED)
    if (C_FLAG_SUPPORTED)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${c_flag}" CACHE STRING "" FORCE)
    else ()
      set(DRACO_FAILED_C_FLAGS "${DRACO_FAILED_C_FLAGS} ${c_flag}" CACHE STRING
          "" FORCE)
    endif ()
  endif ()
endfunction ()

# Checks C++ compiler for support of $cxx_flag. Adds $cxx_flag to
# $CMAKE_CXX_FLAGS when the compile test passes. Caches $c_flag in
# $DRACO_FAILED_CXX_FLAGS when the test fails.
function (add_cxx_flag_if_supported cxx_flag)
  unset(CXX_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FOUND)
  unset(CXX_FLAG_FAILED CACHE)
  string(FIND "${DRACO_FAILED_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FAILED)

  if (${CXX_FLAG_FOUND} EQUAL -1 AND ${CXX_FLAG_FAILED} EQUAL -1)
    unset(CXX_FLAG_SUPPORTED CACHE)
    message("Checking CXX compiler flag support for: " ${cxx_flag})
    check_cxx_compiler_flag("${cxx_flag}" CXX_FLAG_SUPPORTED)
    if (CXX_FLAG_SUPPORTED)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_flag}" CACHE STRING ""
          FORCE)
    else()
      set(DRACO_FAILED_CXX_FLAGS "${DRACO_FAILED_CXX_FLAGS} ${cxx_flag}" CACHE
          STRING "" FORCE)
    endif ()
  endif ()
endfunction ()

# Convenience method for adding a flag to both the C and C++ compiler command
# lines.
function (add_compiler_flag_if_supported flag)
  add_c_flag_if_supported(${flag})
  add_cxx_flag_if_supported(${flag})
endfunction ()

# Checks CXX compiler for support of $cxx_flag and terminates generation when
# support is not present.
function (require_cxx_flag cxx_flag)
  unset(CXX_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FOUND)

  if (${CXX_FLAG_FOUND} EQUAL -1)
    unset(DRACO_HAVE_CXX_FLAG CACHE)
    message("Checking CXX compiler flag support for: " ${cxx_flag})
    check_cxx_compiler_flag("${cxx_flag}" DRACO_HAVE_CXX_FLAG)
    if (NOT DRACO_HAVE_CXX_FLAG)
      message(FATAL_ERROR "Draco requires support for CXX flag: ${cxx_flag}.")
    endif ()
    set(CMAKE_CXX_FLAGS "${cxx_flag} ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
  endif ()
endfunction ()

# Checks only non-MSVC targets for support of $cxx_flag.
function (require_cxx_flag_nomsvc cxx_flag)
  if (NOT MSVC)
    require_cxx_flag(${cxx_flag})
  endif ()
endfunction ()

# Adds $preproc_def to CXX compiler command line (as -D$preproc_def) if not
# already present.
function (add_cxx_preproc_definition preproc_def)
  unset(PREPROC_DEF_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${preproc_def}" PREPROC_DEF_FOUND)

  if (${PREPROC_DEF_FOUND} EQUAL -1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${preproc_def}" CACHE STRING ""
        FORCE)
  endif ()
endfunction ()
