# FindDraco
#
# Locates Draco and sets the following variables:
#
# Drace_FOUND
# Draco_INCLUDE_DIRS
# Draco_LIBARY_DIRS
# Draco_LIBRARIES
# Draco_VERSION_STRING
#
# Draco_FOUND is set to YES only when all other variables are successfully
# configured.

unset(Draco_FOUND)
unset(Draco_INCLUDE_DIRS)
unset(Draco_LIBRARY_DIRS)
unset(Draco_LIBRARIES)
unset(Draco_VERSION_STRING)

mark_as_advanced(Draco_FOUND)
mark_as_advanced(Draco_INCLUDE_DIRS)
mark_as_advanced(Draco_LIBRARY_DIRS)
mark_as_advanced(Draco_LIBRARIES)
mark_as_advanced(Draco_VERSION_STRING)

set(Draco_version_file_no_prefix "draco/src/draco/core/draco_version.h")

# Set Draco_INCLUDE_DIRS
find_path(Draco_INCLUDE_DIRS NAMES "${Draco_version_file_no_prefix}")

#  Extract the version string from draco_version.h.
if (Draco_INCLUDE_DIRS)
  set(Draco_version_file
      "${Draco_INCLUDE_DIRS}/draco/src/draco/core/draco_version.h")
  file(STRINGS "${Draco_version_file}" draco_version
       REGEX "kDracoVersion")
  list(GET draco_version 0 draco_version)
  string(REPLACE "static const char kDracoVersion[] = " "" draco_version
         "${draco_version}")
  string(REPLACE ";" "" draco_version "${draco_version}")
  string(REPLACE "\"" "" draco_version "${draco_version}")
  set(Draco_VERSION_STRING ${draco_version})
endif ()

# Find the library.
if (BUILD_SHARED_LIBS)
  find_library(Draco_LIBRARIES NAMES draco.dll libdraco.dylib libdraco.so)
else ()
  find_library(Draco_LIBRARIES NAMES draco.lib libdraco.a)
endif ()

# Store path to library.
get_filename_component(Draco_LIBRARY_DIRS ${Draco_LIBRARIES} DIRECTORY)

if (Draco_INCLUDE_DIRS AND Draco_LIBRARY_DIRS AND Draco_LIBRARIES AND
    Draco_VERSION_STRING)
  set(Draco_FOUND YES)
endif ()
