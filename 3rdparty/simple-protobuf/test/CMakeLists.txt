include(cmake/doctest.cmake)

FILE(GLOB protos ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)

SET(PB_PACKAGE "")
configure_file(proto/scalar/scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/scalar.proto @ONLY)
configure_file(proto/person/person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/person.proto @ONLY)
configure_file(proto/name/name.proto.in ${CMAKE_CURRENT_BINARY_DIR}/name.proto @ONLY)

if(SPB_PROTO_BUILD_ETL_TESTS)
  configure_file(proto/etl/etl-scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/etl-scalar.proto @ONLY)
  configure_file(proto/etl/etl-person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/etl-person.proto @ONLY)
endif()

SET(PB_PACKAGE ".gpb")
configure_file(proto/scalar/scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-scalar.proto @ONLY)
configure_file(proto/person/person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-person.proto @ONLY)
configure_file(proto/name/name.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-name.proto @ONLY)
if(SPB_PROTO_BUILD_ETL_TESTS)
  configure_file(proto/etl/etl-scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-scalar.proto @ONLY)
  configure_file(proto/etl/etl-person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-person.proto @ONLY)
endif()

spb_protobuf_generate(SPB_PROTO_SRCS SPB_PROTO_HDRS ${protos})
spb_protobuf_generate(SPB_PROTO_PERSON_SRC SPB_PROTO_PERSON_HDR ${CMAKE_CURRENT_BINARY_DIR}/person.proto)
spb_protobuf_generate(SPB_PROTO_NAME_SRC SPB_PROTO_NAME_HDR ${CMAKE_CURRENT_BINARY_DIR}/name.proto)
spb_protobuf_generate(SPB_PROTO_SCALAR_SRC SPB_PROTO_SCALAR_HDR ${CMAKE_CURRENT_BINARY_DIR}/scalar.proto)
if(SPB_PROTO_BUILD_ETL_TESTS)
  spb_protobuf_generate(SPB_PROTO_ETL_SCALAR_SRC SPB_PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/etl-scalar.proto)
  spb_protobuf_generate(SPB_PROTO_ETL_PERSON_SRC SPB_PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/etl-person.proto)
endif()

add_custom_target(unit_tests)

add_executable(base64-test base64.cpp)
spb_set_compile_options(base64-test)
spb_disable_warnings(base64-test)
target_link_libraries(base64-test PUBLIC spb-proto)
add_dependencies(unit_tests base64-test)
doctest_discover_tests(base64-test)

add_executable(json-test json.cpp ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC})
spb_set_compile_options(json-test)
spb_disable_warnings(json-test)
target_link_libraries(json-test PUBLIC spb-proto)
add_dependencies(unit_tests json-test)
doctest_discover_tests(json-test)

add_executable(pb-test pb.cpp ${SPB_PROTO_SRCS} ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC})
spb_set_compile_options(pb-test)
spb_disable_warnings(pb-test)
target_link_libraries(pb-test PUBLIC spb-proto)
add_dependencies(unit_tests pb-test)
doctest_discover_tests(pb-test)

if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT IPO_ENABLED)
endif()

add_executable(compiled-protos protos.cpp ${SPB_PROTO_SRCS} ${SPB_PROTO_HDRS})
spb_set_compile_options(compiled-protos)
spb_disable_warnings(compiled-protos)
target_link_libraries(compiled-protos PUBLIC spb-proto)

if(SPB_PROTO_BUILD_COMPATIBILITY_TESTS)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "/etc/arch-release")
    set(ABSL_PROPAGATE_CXX_STD On)
    set(ABSL_ENABLE_INSTALL Off)
    set(BUILD_SHARED_LIBS Off)
  
    set(protobuf_INSTALL Off)
    set(protobuf_BUILD_LIBPROTOC Off)
    set(protobuf_BUILD_TESTS Off)
    set(protobuf_BUILD_LIBUPB Off)
    set(protobuf_BUILD_SHARED_LIBS Off)
    set(protobuf_BUILD_LIBUPB Off)
    set(protobuf_ABSL_PROVIDER "module")
  
    FetchContent_Declare(
      protobuf
      GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
      GIT_TAG v30.0
    )
    FetchContent_MakeAvailable(protobuf)
  else()
    find_package(Protobuf REQUIRED)
  endif()

  function(PROTOBUF_GENERATE_CPP SRC_VAR HDR_VAR PROTO_FILE)
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(OUTPUT_CC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    set(OUTPUT_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
    
    add_custom_command(
        OUTPUT ${OUTPUT_CC} ${OUTPUT_H}
        COMMAND protobuf::protoc
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I${CMAKE_CURRENT_BINARY_DIR} ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating Protobuf files for ${PROTO_FILE} as ${OUTPUT_CC} and ${OUTPUT_H}"
        VERBATIM
    )
    
    set(${SRC_VAR} ${OUTPUT_CC} PARENT_SCOPE)
    set(${HDR_VAR} ${OUTPUT_H} PARENT_SCOPE)
  endfunction()

  PROTOBUF_GENERATE_CPP(PROTO_PERSON_SRC PROTO_PERSON_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-person.proto)
  PROTOBUF_GENERATE_CPP(PROTO_NAME_SRC PROTO_NAME_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-name.proto)
  PROTOBUF_GENERATE_CPP(PROTO_SCALAR_SRC PROTO_SCALAR_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-scalar.proto)
  if(SPB_PROTO_BUILD_ETL_TESTS)
    PROTOBUF_GENERATE_CPP(PROTO_ETL_SCALAR_SRC PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-scalar.proto)
    PROTOBUF_GENERATE_CPP(PROTO_ETL_PERSON_SRC PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-person.proto)
  endif()

  add_library(gpb-proto STATIC ${PROTO_ETL_PERSON_SRC} ${PROTO_ETL_SCALAR_SRC} ${PROTO_SCALAR_SRC} ${PROTO_PERSON_SRC} ${PROTO_NAME_SRC})
  target_link_libraries(gpb-proto PUBLIC protobuf::libprotobuf)
  target_compile_features(gpb-proto INTERFACE cxx_std_20)

  add_executable(gpb-compatibility-test gpb-compatibility.cpp ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC})
  spb_set_compile_options(gpb-compatibility-test)
  target_link_libraries(gpb-compatibility-test PUBLIC spb-proto gpb-proto)
  add_dependencies(unit_tests gpb-compatibility-test)
  doctest_discover_tests(gpb-compatibility-test)

  if(SPB_PROTO_BUILD_ETL_TESTS)
    Include(FetchContent)

    FetchContent_Declare(
      etl
      GIT_REPOSITORY https://github.com/ETLCPP/etl
      GIT_TAG        "20.39.4"
    )

    FetchContent_MakeAvailable(etl)

    add_executable(etl-compatibility-test etl-compatibility.cpp ${SPB_PROTO_ETL_SCALAR_SRC} ${SPB_PROTO_ETL_PERSON_SRC})
    spb_enable_warnings(etl-compatibility-test)
    target_link_libraries(etl-compatibility-test PUBLIC spb-proto gpb-proto etl::etl)
    add_dependencies(unit_tests etl-compatibility-test)
    doctest_discover_tests(etl-compatibility-test)  
  endif()
endif()

if(SPB_PROTO_BUILD_COMPILER_TESTS)
  add_executable(spb-protoc-test parser.cpp 
      ../../src/spb-proto-compiler/parser/parser.cpp 
      ../../src/spb-proto-compiler/dumper/pb/dumper.cpp
      ../../src/spb-proto-compiler/dumper/json/dumper.cpp
      ../../src/spb-proto-compiler/dumper/dumper.cpp
      ../../src/spb-proto-compiler/dumper/header.cpp
      ../../src/spb-proto-compiler/io/file.cpp
      ../../src/spb-proto-compiler/ast/ast.cpp
      ../../src/spb-proto-compiler/ast/ast-types.cpp
      ../../src/spb-proto-compiler/ast/ast-messages-order.cpp
      )
  target_include_directories(spb-protoc-test PUBLIC ${CMAKE_SOURCE_DIR}/src/spb-proto-compiler)
  target_link_libraries(spb-protoc-test PUBLIC spb-proto)
  add_dependencies(unit_tests spb-protoc-test)
  doctest_discover_tests(spb-protoc-test)
endif()

if(SPB_PROTO_BUILD_FUZZER_TESTS)
  if(SPB_PROTO_USE_ADDRESS_SANITIZER)
    add_compile_options("-fsanitize=fuzzer,address" -O1 -g)
    add_link_options("-fsanitize=fuzzer,address")
elseif(SPB_PROTO_USE_UB_SANITIZER)
    message("-- Enable undefined behavior sanitizer" -O1 -g)
    add_compile_options("-fsanitize=fuzzer,undefined")
    add_link_options("-fsanitize=fuzzer,undefined")
endif()

  add_executable(spb-protoc-fuzzer fuzzer/spb-protoc-fuzzer.cpp 
    ../../src/spb-proto-compiler/parser/parser.cpp 
    ../../src/spb-proto-compiler/io/file.cpp
    ../../src/spb-proto-compiler/ast/ast.cpp
    )
  target_include_directories(spb-protoc-fuzzer PUBLIC ${CMAKE_SOURCE_DIR}/src/spb-proto-compiler)
  target_link_libraries(spb-protoc-fuzzer PUBLIC spb-proto)
  spb_disable_warnings(spb-protoc-fuzzer)

  add_executable(spb-pb-fuzzer fuzzer/spb-pb-fuzzer.cpp 
    ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC} ${SPB_PROTO_SRCS}
  )
  target_link_libraries(spb-pb-fuzzer PUBLIC spb-proto)
  spb_disable_warnings(spb-pb-fuzzer)

  add_executable(spb-json-fuzzer fuzzer/spb-json-fuzzer.cpp 
    ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC} ${SPB_PROTO_SRCS}
  )
  target_link_libraries(spb-json-fuzzer PUBLIC spb-proto)
  spb_disable_warnings(spb-json-fuzzer)
endif()


