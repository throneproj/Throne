PROTOBUF_GENERATE_CPP(PROTO_BOOK_SRC PROTO_BOOK_HDR ${CMAKE_CURRENT_LIST_DIR}/../proto/addressbook.proto)
add_library(gpb-common STATIC common.cpp ${PROTO_BOOK_SRC})
target_link_libraries(gpb-common PUBLIC protobuf::libprotobuf)
spb_set_compile_options(gpb-common)

PROTOBUF_GENERATE_CPP(PROTO_BOOK_LITE_SRC PROTO_BOOK_LITE_HDR ${CMAKE_CURRENT_LIST_DIR}/../proto/addressbook-lite.proto)
add_library(gpb-lite-common STATIC common.cpp ${PROTO_BOOK_LITE_SRC})
target_compile_definitions(gpb-lite-common PUBLIC GPB_LITE=1)
target_link_libraries(gpb-lite-common PUBLIC protobuf::libprotobuf-lite)
spb_set_compile_options(gpb-lite-common)

add_executable(gpb-benchmark benchmark.cpp)
spb_set_compile_options(gpb-benchmark)
target_link_libraries(gpb-benchmark PUBLIC gpb-common)

add_executable(gpb-lite-benchmark benchmark.cpp)
spb_set_compile_options(gpb-lite-benchmark)
target_link_libraries(gpb-lite-benchmark PUBLIC gpb-lite-common)

add_executable(gpb-pb-serialize pb-serialize.cpp)
spb_set_compile_options(gpb-pb-serialize)
target_link_libraries(gpb-pb-serialize PUBLIC gpb-common)

add_executable(gpb-lite-pb-serialize pb-serialize.cpp)
spb_set_compile_options(gpb-lite-pb-serialize)
target_link_libraries(gpb-lite-pb-serialize PUBLIC gpb-lite-common)

add_executable(gpb-pb-deserialize pb-deserialize.cpp)
spb_set_compile_options(gpb-pb-deserialize)
target_link_libraries(gpb-pb-deserialize PUBLIC gpb-common)

add_executable(gpb-lite-pb-deserialize pb-deserialize.cpp)
spb_set_compile_options(gpb-lite-pb-deserialize)
target_link_libraries(gpb-lite-pb-deserialize PUBLIC gpb-lite-common)

add_executable(gpb-json-serialize json-serialize.cpp)
target_link_libraries(gpb-json-serialize PUBLIC gpb-common)
spb_set_compile_options(gpb-json-serialize)

add_executable(gpb-json-deserialize json-deserialize.cpp)
target_link_libraries(gpb-json-deserialize PUBLIC gpb-common)
spb_set_compile_options(gpb-json-deserialize)
