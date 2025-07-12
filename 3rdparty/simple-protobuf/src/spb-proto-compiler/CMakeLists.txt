project(spb-proto-compiler VERSION 1.0.0 LANGUAGES CXX)

add_executable(spb-protoc main.cpp parser/parser.cpp ast/ast-types.cpp 
  ast/ast-messages-order.cpp ast/ast.cpp io/file.cpp dumper/header.cpp 
  dumper/pb/dumper.cpp dumper/json/dumper.cpp dumper/dumper.cpp)
  
target_include_directories(spb-protoc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(spb-protoc PUBLIC spb-proto)
spb_set_compile_options(spb-protoc)
target_compile_features(spb-protoc PUBLIC cxx_std_20)
