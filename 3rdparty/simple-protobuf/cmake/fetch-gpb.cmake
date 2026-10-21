set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_BUILD_PROTOC_BINARIES ON)
set(protobuf_INSTALL OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_WITH_ZLIB OFF)

Include(FetchContent)

FetchContent_Declare(
  protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG v35.0
  )
FetchContent_MakeAvailable(protobuf)

function(PROTOBUF_GENERATE_CPP SRC_VAR HDR_VAR PROTO_FILE)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(OUTPUT_CC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
  set(OUTPUT_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")

  add_custom_command(
    OUTPUT ${OUTPUT_CC} ${OUTPUT_H}
    COMMAND protobuf::protoc
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I${CMAKE_SOURCE_DIR}/benchmark/gpb/../proto -I${CMAKE_CURRENT_BINARY_DIR} -I${CMAKE_SOURCE_DIR}/include/spb/proto -I${CMAKE_BINARY_DIR}/_deps/protobuf-src/src ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating Protobuf files for ${PROTO_FILE} as ${OUTPUT_CC} and ${OUTPUT_H}"
    VERBATIM
  )

  set(${SRC_VAR} ${OUTPUT_CC} PARENT_SCOPE)
  set(${HDR_VAR} ${OUTPUT_H} PARENT_SCOPE)
endfunction(PROTOBUF_GENERATE_CPP)
