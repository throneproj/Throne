# This source code is based on /usr/share/cmake/Modules/FindProtobuf.cmake
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

include(CMakeParseArguments)

if(SPB_PROTO_USE_CLANG_FORMAT)
    find_program(CLANG_FORMAT clang-format)
endif()

function(spb_protobuf_generate)
  set(_options APPEND_PATH DESCRIPTORS)
  set(_singleargs LANGUAGE OUT_VAR EXPORT_MACRO PROTOC_OUT_DIR PLUGIN PLUGIN_OPTIONS PROTOC_EXE)
  if(COMMAND target_sources)
    list(APPEND _singleargs TARGET)
  endif()
  set(_multiargs PROTOS IMPORT_DIRS GENERATE_EXTENSIONS PROTOC_OPTIONS DEPENDENCIES)

  cmake_parse_arguments(spb "${_options}" "${_singleargs}" "${_multiargs}" "${ARGN}")

  if(NOT spb_PROTOS AND NOT spb_TARGET)
    message(SEND_ERROR "Error: spb_protobuf_generate called without any targets or source files")
    return()
  endif()

  if(NOT spb_OUT_VAR AND NOT spb_TARGET)
    message(SEND_ERROR "Error: spb_protobuf_generate called without a target or output variable")
    return()
  endif()

  if(NOT spb_LANGUAGE)
    set(spb_LANGUAGE cpp)
  endif()
  string(TOLOWER ${spb_LANGUAGE} spb_LANGUAGE)    
  if(NOT ${spb_LANGUAGE} STREQUAL cpp)
    message(SEND_ERROR "Error: spb_protobuf_generate supports only cpp LANGUAGE")
  endif()
    
  if(NOT spb_PROTOC_OUT_DIR)
    set(spb_PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  set(spb_GENERATE_EXTENSIONS .pb.h .pb.cc)
  
  if(spb_TARGET)
    get_target_property(_source_list ${spb_TARGET} SOURCES)
    foreach(_file ${_source_list})
      if(_file MATCHES "proto$")
        list(APPEND spb_PROTOS ${_file})
      endif()
    endforeach()
  endif()

  if(NOT spb_PROTOS)
    message(SEND_ERROR "Error: spb_protobuf_generate could not find any .proto files")
    return()
  endif()

  if(spb_APPEND_PATH)
    # Create an include path for each file specified
    foreach(_file ${spb_PROTOS})
      get_filename_component(_abs_file ${_file} ABSOLUTE)
      get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
      list(FIND _protobuf_include_path ${_abs_dir} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _protobuf_include_path -I ${_abs_dir})
      endif()
    endforeach()
  endif()

  foreach(DIR ${spb_IMPORT_DIRS})
    get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
    list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
    if(${_contains_already} EQUAL -1)
      list(APPEND _protobuf_include_path -I ${ABS_PATH})
    endif()
  endforeach()

  if(NOT spb_APPEND_PATH)
    list(APPEND _protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(_generated_srcs_all)
  foreach(_proto ${spb_PROTOS})
    get_filename_component(_abs_file ${_proto} ABSOLUTE)
    get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
    get_filename_component(_basename ${_proto} NAME_WLE)

    set(_rel_dir)
    set(_possible_rel_dir)
    if (NOT spb_APPEND_PATH)
      foreach(DIR ${_protobuf_include_path})
        if(NOT DIR STREQUAL "-I")
          string(FIND "${_abs_dir}" "${DIR}/" _pos)
          if(_pos EQUAL 0)
            string(LENGTH "${DIR}/" _len)
            string(LENGTH "${_abs_dir}/" _abs_len)
            string(SUBSTRING "${_abs_dir}" ${_len} ${_abs_len} _rel_dir)
            break()
          endif()
        endif()
      endforeach()
      set(_possible_rel_dir ${_rel_dir}/)
    endif()

    set(_generated_srcs)
    set(_generated_h "${spb_PROTOC_OUT_DIR}/${_possible_rel_dir}${_basename}.pb.cc")
    set(_generated_cc "${spb_PROTOC_OUT_DIR}/${_possible_rel_dir}${_basename}.pb.h")

    list(APPEND _generated_srcs "${_generated_h}")
    list(APPEND _generated_srcs "${_generated_cc}")
        
    list(APPEND _generated_srcs_all ${_generated_srcs})

    set(_comment "Running ${spb_LANGUAGE} protocol buffer compiler on ${_proto}")
    if(spb_PROTOC_OPTIONS)
      set(_comment "${_comment}, protoc-options: ${spb_PROTOC_OPTIONS}")
    endif()

    if(SPB_PROTO_USE_CLANG_FORMAT AND CLANG_FORMAT)        
      add_custom_command(
        OUTPUT ${_generated_srcs}
        COMMAND $<TARGET_FILE:spb-protoc> ARGS ${spb_PROTOC_OPTIONS} --cpp_out ${spb_PROTOC_OUT_DIR} ${_protobuf_include_path} ${_abs_file}
        COMMAND ${CLANG_FORMAT} ARGS -i "${_generated_h}"
        COMMAND ${CLANG_FORMAT} ARGS -i "${_generated_cc}"
        DEPENDS ${_abs_file} spb-protoc ${spb_DEPENDENCIES}
        COMMENT ${_comment}
        VERBATIM )
    else()
      add_custom_command(
        OUTPUT ${_generated_srcs}
        COMMAND $<TARGET_FILE:spb-protoc> ARGS ${spb_PROTOC_OPTIONS} --cpp_out ${spb_PROTOC_OUT_DIR} ${_protobuf_include_path} ${_abs_file}
        DEPENDS ${_abs_file} spb-protoc ${spb_DEPENDENCIES}
        COMMENT ${_comment}
        VERBATIM )
    endif()
  endforeach()

  set_source_files_properties(${_generated_srcs_all} PROPERTIES GENERATED TRUE)
  if(spb_OUT_VAR)
    set(${spb_OUT_VAR} ${_generated_srcs_all} PARENT_SCOPE)
  endif()
  if(spb_TARGET)
    target_sources(${spb_TARGET} PRIVATE ${_generated_srcs_all})
    target_link_libraries(${spb_TARGET} PUBLIC spb-proto)
    target_include_directories(${spb_TARGET} PRIVATE "$<BUILD_INTERFACE:${spb_PROTOC_OUT_DIR}>")
  endif()
endfunction()

function(spb_protobuf_generate_cpp SRCS HDRS)
  cmake_parse_arguments(spb_cpp "" "EXPORT_MACRO;DESCRIPTORS" "" ${ARGN})

  set(_proto_files "${spb_cpp_UNPARSED_ARGUMENTS}")
  if(NOT _proto_files)
    message(SEND_ERROR "Error: spb_protobuf_generate_cpp() called without any proto files")
    return()
  endif()
          
  set(_outvar)
  spb_protobuf_generate(OUT_VAR _outvar PROTOS ${_proto_files})

  set(${SRCS})
  set(${HDRS})
  
  foreach(_file ${_outvar})
    if(_file MATCHES "cc$")
      list(APPEND ${SRCS} ${_file})
    else()
      list(APPEND ${HDRS} ${_file})
    endif()
  endforeach()

  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction(spb_protobuf_generate_cpp)
