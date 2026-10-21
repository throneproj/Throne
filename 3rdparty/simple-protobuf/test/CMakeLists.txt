include(cmake/doctest.cmake)

include_directories(${CMAKE_SOURCE_DIR}/test/include)

FILE(GLOB protos ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)

SET(PB_PACKAGE "")
configure_file(proto/scalar/scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/scalar.proto @ONLY)
configure_file(proto/person/person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/person.proto @ONLY)
configure_file(proto/reserved/reserved.proto.in ${CMAKE_CURRENT_BINARY_DIR}/reserved.proto @ONLY)
configure_file(proto/name/name.proto.in ${CMAKE_CURRENT_BINARY_DIR}/name.proto @ONLY)

if(SPB_PROTO_BUILD_ETL_TESTS)
  configure_file(proto/etl/etl-scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/etl-scalar.proto @ONLY)
  configure_file(proto/etl/etl-person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/etl-person.proto @ONLY)
endif()

SET(PB_PACKAGE ".gpb")
configure_file(proto/scalar/scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-scalar.proto @ONLY)
configure_file(proto/person/person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-person.proto @ONLY)
configure_file(proto/reserved/reserved.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-reserved.proto @ONLY)
configure_file(proto/name/name.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-name.proto @ONLY)
if(SPB_PROTO_BUILD_ETL_TESTS)
  configure_file(proto/etl/etl-scalar.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-scalar.proto @ONLY)
  configure_file(proto/etl/etl-person.proto.in ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-person.proto @ONLY)
endif()

add_library(spb-generated STATIC
  ${protos}
  ${CMAKE_CURRENT_BINARY_DIR}/person.proto
  ${CMAKE_CURRENT_BINARY_DIR}/reserved.proto
  ${CMAKE_CURRENT_BINARY_DIR}/name.proto
  ${CMAKE_CURRENT_BINARY_DIR}/scalar.proto
)
spb_set_compile_options(spb-generated)
spb_disable_warnings(spb-generated)
spb_protobuf_generate(LANGUAGE cpp TARGET spb-generated IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../include/spb/proto")

if(SPB_PROTO_BUILD_ETL_TESTS)
  spb_protobuf_generate_cpp(SPB_PROTO_ETL_SCALAR_SRC SPB_PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/etl-scalar.proto)
  spb_protobuf_generate_cpp(SPB_PROTO_ETL_PERSON_SRC SPB_PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/etl-person.proto)
endif()

add_custom_target(unit_tests)

add_executable(base64-test base64.cpp)
spb_set_compile_options(base64-test)
spb_disable_warnings(base64-test)
target_link_libraries(base64-test PUBLIC spb-proto)
add_dependencies(unit_tests base64-test)
doctest_discover_tests(base64-test)

add_executable(json-test json.cpp)
spb_set_compile_options(json-test)
spb_disable_warnings(json-test)
target_link_libraries(json-test PRIVATE spb-generated)
add_dependencies(unit_tests json-test)
doctest_discover_tests(json-test)

add_executable(pb-test pb.cpp)
spb_set_compile_options(pb-test)
spb_disable_warnings(pb-test)
target_link_libraries(pb-test PRIVATE spb-generated)
add_dependencies(unit_tests pb-test)
doctest_discover_tests(pb-test)

if(SPB_PROTO_BUILD_COMPATIBILITY_TESTS)
  PROTOBUF_GENERATE_CPP(PROTO_PERSON_SRC PROTO_PERSON_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-person.proto)
  if(Protobuf_VERSION VERSION_GREATER_EQUAL "30")
    PROTOBUF_GENERATE_CPP(PROTO_RESERVED_SRC PROTO_PERSON_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-reserved.proto)
  endif()
  PROTOBUF_GENERATE_CPP(PROTO_NAME_SRC PROTO_NAME_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-name.proto)
  PROTOBUF_GENERATE_CPP(PROTO_SPB_SRC PROTO_SPB_HDR ${CMAKE_SOURCE_DIR}/include/spb/proto/spb.proto)
  PROTOBUF_GENERATE_CPP(PROTO_SCALAR_SRC PROTO_SCALAR_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-scalar.proto)
  if(SPB_PROTO_BUILD_ETL_TESTS)
    PROTOBUF_GENERATE_CPP(PROTO_ETL_SCALAR_SRC PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-scalar.proto)
    PROTOBUF_GENERATE_CPP(PROTO_ETL_PERSON_SRC PROTO_ETL_HDR ${CMAKE_CURRENT_BINARY_DIR}/gpb-etl-person.proto)
  endif()

if(Protobuf_VERSION VERSION_GREATER_EQUAL "30")
  add_library(gpb-proto STATIC ${PROTO_SPB_SRC} ${PROTO_ETL_PERSON_SRC} ${PROTO_ETL_SCALAR_SRC} ${PROTO_SCALAR_SRC} ${PROTO_PERSON_SRC} ${PROTO_NAME_SRC} ${PROTO_RESERVED_SRC})
else()
  add_library(gpb-proto STATIC ${PROTO_SPB_SRC} ${PROTO_ETL_PERSON_SRC} ${PROTO_ETL_SCALAR_SRC} ${PROTO_SCALAR_SRC} ${PROTO_PERSON_SRC} ${PROTO_NAME_SRC})
endif()

  target_link_libraries(gpb-proto PUBLIC protobuf::libprotobuf)
  target_compile_features(gpb-proto INTERFACE cxx_std_20)

  add_executable(gpb-compatibility-test gpb-compatibility.cpp ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC})
  spb_set_compile_options(gpb-compatibility-test)
  target_link_libraries(gpb-compatibility-test PUBLIC spb-proto gpb-proto spb-generated)
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
    spb_set_compile_options(etl-compatibility-test)
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
      ../../src/spb-proto-compiler/ast/ast-options.cpp
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
    ../../src/spb-proto-compiler/dumper/pb/dumper.cpp
    ../../src/spb-proto-compiler/dumper/json/dumper.cpp
    ../../src/spb-proto-compiler/dumper/dumper.cpp
    ../../src/spb-proto-compiler/dumper/header.cpp
    ../../src/spb-proto-compiler/io/file.cpp
    ../../src/spb-proto-compiler/ast/ast.cpp
    ../../src/spb-proto-compiler/ast/ast-types.cpp
    ../../src/spb-proto-compiler/ast/ast-options.cpp
    ../../src/spb-proto-compiler/ast/ast-messages-order.cpp
  )
  target_include_directories(spb-protoc-fuzzer PUBLIC ${CMAKE_SOURCE_DIR}/src/spb-proto-compiler)
  target_link_libraries(spb-protoc-fuzzer PUBLIC spb-proto)
  spb_disable_warnings(spb-protoc-fuzzer)

  add_executable(spb-pb-fuzzer fuzzer/spb-pb-fuzzer.cpp
    ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC} ${SPB_PROTO_SRCS}
  )
  target_link_libraries(spb-pb-fuzzer PUBLIC spb-proto)
  target_link_libraries(spb-pb-fuzzer PRIVATE spb-generated)
  target_include_directories(spb-pb-fuzzer PUBLIC ${CMAKE_BINARY_DIR}/test/)
  spb_disable_warnings(spb-pb-fuzzer)

  add_executable(spb-json-fuzzer fuzzer/spb-json-fuzzer.cpp
    ${SPB_PROTO_PERSON_SRC} ${SPB_PROTO_NAME_SRC} ${SPB_PROTO_SCALAR_SRC} ${SPB_PROTO_SRCS}
  )
  target_link_libraries(spb-json-fuzzer PUBLIC spb-proto)
  target_link_libraries(spb-json-fuzzer PRIVATE spb-generated)
  target_include_directories(spb-json-fuzzer PUBLIC ${CMAKE_BINARY_DIR}/test/)
  spb_disable_warnings(spb-json-fuzzer)
else()
  add_subdirectory(custom-output-dir)
endif()
