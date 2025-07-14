spb_protobuf_generate(PROTO_SRCS PROTO_HDRS proto/addressbook.proto)
spb_protobuf_generate(PROTO_ETL_SRCS PROTO_ETL_HDRS proto/etl.proto)

add_executable(addressbook addressbook.cpp ${PROTO_SRCS})
target_link_libraries(addressbook PUBLIC spb-proto)

Include(FetchContent)

FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl
  GIT_TAG        "20.39.4"
)

FetchContent_MakeAvailable(etl)

add_executable(etl-example etl.cpp ${PROTO_ETL_SRCS})
target_link_libraries(etl-example PUBLIC spb-proto etl::etl)
spb_set_compile_options(etl-example)
