if (NOT DRACO_CMAKE_UTIL_CMAKE_)
set(DRACO_CMAKE_UTIL_CMAKE_ 1)

function (create_dummy_source_file basename extension out_file_path)
  set(dummy_source_file "${draco_build_dir}/${basename}.${extension}")
   file(WRITE "${dummy_source_file}"
       "// Generated file. DO NOT EDIT!\n"
       "// ${target_name} needs a ${extension} file to force link language, \n"
       "// or to silence a harmless CMake warning: Ignore me.\n"
       "void ${target_name}_dummy_function(void) {}\n")
  set(${out_file_path} ${dummy_source_file} PARENT_SCOPE)
endfunction ()

function (add_dummy_source_file_to_target target_name extension)
  create_dummy_source_file("${target_name}" "${extension}" "dummy_source_file")
  target_sources(${target_name} PRIVATE ${dummy_source_file})
endfunction ()

function (extract_version_string version_file version_string_out_var)
  file(STRINGS "${version_file}" draco_version REGEX "kDracoVersion")
  list(GET draco_version 0 draco_version)
  string(REPLACE "static const char kDracoVersion[] = " "" draco_version
         "${draco_version}")
  string(REPLACE ";" "" draco_version "${draco_version}")
  string(REPLACE "\"" "" draco_version "${draco_version}")
  set("${version_string_out_var}" "${draco_version}" PARENT_SCOPE)
endfunction ()

endif()  # DRACO_CMAKE_UTIL_CMAKE_

