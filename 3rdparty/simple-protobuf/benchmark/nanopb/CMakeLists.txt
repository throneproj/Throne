NANOPB_GENERATE_CPP(PROTO_BOOK_SRC PROTO_BOOK_HDR ${CMAKE_CURRENT_LIST_DIR}/../proto/addressbook.proto)
add_library(nanopb-common STATIC common.c ${PROTO_BOOK_SRC})
target_include_directories(nanopb-common PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(nanopb-common PUBLIC nanopb)
spb_set_compile_options(nanopb-common)

add_executable(nanopb-benchmark benchmark.cpp)
target_link_libraries(nanopb-benchmark PUBLIC nanopb-common)
spb_set_compile_options(nanopb-benchmark)

add_executable(nanopb-pb-serialize pb-serialize.c)
target_link_libraries(nanopb-pb-serialize PUBLIC nanopb-common)
spb_set_compile_options(nanopb-pb-serialize)

add_executable(nanopb-pb-deserialize pb-deserialize.c)
target_link_libraries(nanopb-pb-deserialize PUBLIC nanopb-common)
spb_set_compile_options(nanopb-pb-deserialize)
