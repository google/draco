# Finddraco
#
# Locates draco and sets the following variables:
#
# - DRACO_FOUND
# - DRACO_INCLUDE_DIR
# - DRACO_LIBARY_DIR
# - DRACO_LIBRARY
# - DRACO_VERSION
#
# DRACO_FOUND is set to YES only when all other variables are successfully
# configured.
include(GNUInstallDirs)

unset(DRACO_FOUND)
unset(DRACO_INCLUDE_DIR)
unset(DRACO_LIBRARY_DIR)
unset(DRACO_LIBRARY)
unset(DRACO_VERSION)

mark_as_advanced(DRACO_FOUND)
mark_as_advanced(DRACO_INCLUDE_DIR)
mark_as_advanced(DRACO_LIBRARY_DIR)
mark_as_advanced(DRACO_LIBRARY)
mark_as_advanced(DRACO_VERSION)

if(NOT DRACO_SEARCH_DIR)
  set(DRACO_SEARCH_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR})
endif()

set(draco_features_dir "${DRACO_SEARCH_DIR}/draco")

# Set DRACO_INCLUDE_DIR
find_path(
  DRACO_INCLUDE_DIR
  NAMES "draco_features.h"
  PATHS "${draco_features_dir}")

# The above returned "path/to/draco/", strip "draco" so that projects can
# include draco sources using draco[/subdir]/file.h, like the draco sources.
get_filename_component(DRACO_INCLUDE_DIR ${DRACO_INCLUDE_DIR} DIRECTORY)

# Extract the version string from draco_version.h.
if(DRACO_INCLUDE_DIR)
  set(draco_version_file
      "${DRACO_INCLUDE_DIR}/draco/core/draco_version.h")
  file(STRINGS "${draco_version_file}" draco_version REGEX "kDracoVersion")
  list(GET draco_version 0 draco_version)
  string(REPLACE "static const char kDracoVersion[] = " "" draco_version
                 "${draco_version}")
  string(REPLACE ";" "" draco_version "${draco_version}")
  string(REPLACE "\"" "" draco_version "${draco_version}")
  set(DRACO_VERSION ${draco_version})
endif()

# Find the library.
if(BUILD_SHARED_LIBS)
  find_library(DRACO_LIBRARY NAMES draco.dll libdraco.dylib libdraco.so)
else()
  find_library(DRACO_LIBRARY NAMES draco.lib libdraco.a)
endif()

# Store path to library.
get_filename_component(DRACO_LIBRARY_DIR ${DRACO_LIBRARY} DIRECTORY)

if(DRACO_INCLUDE_DIR
   AND DRACO_LIBRARY_DIR
   AND DRACO_LIBRARY
   AND DRACO_VERSION)
  set(DRACO_FOUND YES)
endif()
