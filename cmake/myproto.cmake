add_subdirectory(3rdparty/simple-protobuf)

spb_protobuf_generate(PROTO_SRCS PROTO_HDRS core/server/gen/libcore.proto)

add_library(myproto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(myproto
        PUBLIC
        spb-proto
        )
