Include(FetchContent)

FetchContent_Declare(
    nanopb
    GIT_REPOSITORY https://github.com/nanopb/nanopb.git
    GIT_TAG 0.4.9.1
    GIT_SHALLOW TRUE
  )
FetchContent_MakeAvailable(nanopb)

add_library(nanopb STATIC
    ${nanopb_SOURCE_DIR}/pb_common.c
    ${nanopb_SOURCE_DIR}/pb_decode.c
    ${nanopb_SOURCE_DIR}/pb_encode.c
)
target_include_directories(nanopb PUBLIC "${nanopb_SOURCE_DIR}")
spb_set_compile_options(nanopb)

function(NANOPB_GENERATE_CPP SRC_VAR HDR_VAR PROTO_FILE)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(OUTPUT_C "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.c")
  set(OUTPUT_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")

  add_custom_command(
    OUTPUT ${OUTPUT_C} ${OUTPUT_H}
    COMMAND python3
    ARGS ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../build/_deps/nanopb-src/generator/nanopb_generator.py --output-dir=${CMAKE_CURRENT_BINARY_DIR}/ -I${CMAKE_SOURCE_DIR}/benchmark/nanopb/../proto -I${CMAKE_CURRENT_BINARY_DIR} ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating NanopbProtobuf files for ${PROTO_FILE} as ${OUTPUT_C} and ${OUTPUT_H}"
    VERBATIM
  )

  set(${SRC_VAR} ${OUTPUT_C} PARENT_SCOPE)
  set(${HDR_VAR} ${OUTPUT_H} PARENT_SCOPE)
endfunction(NANOPB_GENERATE_CPP)
