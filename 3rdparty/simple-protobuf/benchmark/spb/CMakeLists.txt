add_library(spb-common STATIC common.cpp ../proto/addressbook.proto)
spb_protobuf_generate(TARGET spb-common)
spb_set_compile_options(spb-common)

add_executable(spb-benchmark benchmark.cpp)
target_link_libraries(spb-benchmark PUBLIC spb-common)
spb_set_compile_options(spb-benchmark)

add_executable(spb-pb-serialize pb-serialize.cpp)
target_link_libraries(spb-pb-serialize PUBLIC spb-common)
spb_set_compile_options(spb-pb-serialize)

add_executable(spb-pb-deserialize pb-deserialize.cpp)
target_link_libraries(spb-pb-deserialize PUBLIC spb-common)
spb_set_compile_options(spb-pb-deserialize)

add_executable(spb-json-serialize json-serialize.cpp)
target_link_libraries(spb-json-serialize PUBLIC spb-common)
spb_set_compile_options(spb-json-serialize)

add_executable(spb-json-deserialize json-deserialize.cpp)
target_link_libraries(spb-json-deserialize PUBLIC spb-common)
spb_set_compile_options(spb-json-deserialize)

#add_executable(spb-utf8 utf8.cpp)
#target_link_libraries(spb-utf8 PUBLIC spb::proto)
#spb_set_compile_options(spb-utf8)
