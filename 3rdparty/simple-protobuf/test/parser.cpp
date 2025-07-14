#include "ast/ast.h"
#include "io/file.h"
#include <cstdlib>
#include <dumper/dumper.h>
#include <filesystem>
#include <parser/parser.h>
#include <sstream>
#include <string>
#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace
{
using namespace std::literals;

struct proto_file_test
{
    const std::string_view file_content;
    size_t error_line = 0;
};

void test_proto_file( const proto_file_test & test,
                      const std::string & test_file                         = "tmp_test.proto",
                      std::span< const std::filesystem::path > import_paths = { } )
{
    REQUIRE_NOTHROW( save_file( test_file, test.file_content ) );
    try
    {
        auto file   = parse_proto_file( test_file, import_paths );
        auto stream = std::stringstream( );
        dump_cpp_header( file, stream );
        dump_cpp( file, "header.pb.h", stream );
        REQUIRE( test.error_line == 0 );
    }
    catch( const std::exception & ex )
    {
        auto message = std::string_view( ex.what( ) );
        REQUIRE( message.find( test_file + ":" ) != message.npos );
        message.remove_prefix( message.find( test_file + ":" ) + 1 + test_file.size( ) );
        auto line = std::strtoul( message.data( ), nullptr, 10 );
        REQUIRE( test.error_line == line );
    }
    REQUIRE_NOTHROW( std::filesystem::remove( test_file ) );
}

void test_files( std::span< const proto_file_test > files,
                 const std::string & test_file                         = "tmp_test.proto",
                 std::span< const std::filesystem::path > import_paths = { } )
{
    for( const auto & file : files )
    {
        test_proto_file( file, test_file, import_paths );
    }
}
}// namespace

TEST_CASE( "protoc-parser" )
{
    SUBCASE( "api" )
    {
        REQUIRE_THROWS( parse_proto_file( "not_found.proto", { { "../", ".", "/home/" } } ) );
        REQUIRE( cpp_file_name_from_proto( "messages.proto", ".cpp" ) == "messages.cpp" );
    }
    SUBCASE( "import" )
    {
        REQUIRE_NOTHROW( std::filesystem::create_directories( "level1/level2/level3" ) );

        REQUIRE_NOTHROW( save_file( "empty.proto", "" ) );
        REQUIRE_NOTHROW( save_file( "level1/empty1.proto", "import \"level2/empty2.proto\";" ) );
        REQUIRE_NOTHROW(
            save_file( "level1/level2/empty2.proto", "import \"level3/empty3.proto\";" ) );
        REQUIRE_NOTHROW( save_file( "level1/level2/level3/empty3.proto", "" ) );
        REQUIRE_NOTHROW( save_file( "level1.proto", "import \"level1/empty1.proto\";" ) );
        REQUIRE_NOTHROW( save_file( "level2.proto", "import \"level1/level2/empty2.proto\";" ) );
        REQUIRE_NOTHROW(
            save_file( "level3.proto", "import \"level1/level2/level3/empty3.proto\";" ) );
        REQUIRE_NOTHROW( save_file( "level2-from-1.proto", "import \"level2/empty2.proto\";" ) );

        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            import "empty.proto";
            )",
              0 },
            { R"(package UnitTest;
            import "level1.proto";
            import "level2.proto";
            import "level3.proto";
            )",
              0 },
            { R"(package UnitTest;
            import "empty_not_found.proto";
            )",
              2 },
            { R"(package UnitTest;
            import "level2-from-1.proto";
            )",
              2 },
            { R"(package UnitTest;
            importX "message.proto";
            )",
              2 },
        };
        test_files( tests );
        constexpr proto_file_test tests2[] = {
            { R"(package UnitTest;
                import "level2-from-1.proto";
            )",
              0 },
        };

        test_files( tests2, "tmp_test.proto", { { "level2", "level1" } } );
        test_files( tests2, "tmp_test.proto", { { std::filesystem::current_path( ) / "level1" } } );
        test_files( tests2, ( std::filesystem::current_path( ) / "tmp_test.proto" ).string( ),
                    { { "level2", "level1" } } );

        REQUIRE_NOTHROW( std::filesystem::remove( "empty.proto" ) );
        REQUIRE_NOTHROW( std::filesystem::remove( "level1.proto" ) );
        REQUIRE_NOTHROW( std::filesystem::remove( "level2.proto" ) );
        REQUIRE_NOTHROW( std::filesystem::remove( "level3.proto" ) );
        REQUIRE_NOTHROW( std::filesystem::remove_all( "level1" ) );
    }
    SUBCASE( "comment" )
    {
        constexpr proto_file_test tests[] = { { "//"sv, 1 },
                                              { "/*"sv, 1 },
                                              { "/-"sv, 1 },
                                              { R"(package UnitTest;
            // comment
            )",
                                                0 },
                                              { "/**/"sv, 0 },
                                              { R"(package UnitTest;
            /* comment
            */;
            )",
                                                0 },
                                              { R"(syntax = "proto3";
                message Message
                {
                    required uint32 value = 1; // comment
                })"sv,
                                                0 },
                                              { R"(syntax = "proto3";
                message Message
                {
                    // comment
                    required uint32 value = 1; 
                })"sv,
                                                0 },
                                              { R"(syntax = "proto3";
                message Message
                {
                    // comment
                    required uint32 value = 1; // comment 
                })"sv,
                                                0 } };
        test_files( tests );
    }
    SUBCASE( "syntax" )
    {
        constexpr proto_file_test tests[] = { { ""sv, 0 },
                                              { "X"sv, 1 },
                                              { R"(synta = "proto2";)"sv, 1 },
                                              { R"(syntax = "proto1;")"sv, 1 },
                                              { R"(syntax = "proto2")"sv, 1 },
                                              { R"(syntax = "proto2";)"sv, 0 },
                                              { R"(syntax = "proto3";;)"sv, 0 },
                                              { R"(syntax = "proto3")"sv, 1 },
                                              { R"(syntax = "proto4;")"sv, 1 },
                                              { R"(syntax = "proto2";
                message Message
                {
                    required uint32 value = 1;
                })"sv,
                                                0 },
                                              { R"(syntax = "proto3";
                message Message
                {
                    required uint32 value = 1;
                })"sv,
                                                0 } };
        test_files( tests );
    }
    SUBCASE( "scalar" )
    {
        constexpr proto_file_test tests[] = {
            { R"(message Message{
                bool b = 1;
                float f = 2;
                double d = 3;
                int32 i32 = 4;
                sint32 si32 = 5;
                uint32 u32 = 6;
                int64 i64 = 7;
                sint64 si64 = 8;
                uint64 u64 = 9;
                fixed32 f32 = 10;
                sfixed32 sf32 = 11;
                fixed64 f64 = 12;
                sfixed64 sf64 = 13;
                string s = 14;
                bytes by = 15;
                })"sv,
              0 },
            { R"(message Message{
                repeated bool b = 1;
                repeated float f = 2;
                repeated double d = 3;
                repeated int32 i32 = 4;
                repeated sint32 si32 = 5;
                repeated uint32 u32 = 6;
                repeated int64 i64 = 7;
                repeated sint64 si64 = 8;
                repeated uint64 u64 = 9;
                repeated fixed32 f32 = 10;
                repeated sfixed32 sf32 = 11;
                repeated fixed64 f64 = 12;
                repeated sfixed64 sf64 = 13;
                repeated string s = 14;
                repeated bytes by = 15;
                })"sv,
              0 },
            { R"(message Message{
                repeated bool b = 1 [packed = true];
                repeated float f = 2 [packed = true];
                repeated double d = 3 [packed = true];
                repeated int32 i32 = 4 [packed = true];
                repeated sint32 si32 = 5 [packed = true];
                repeated uint32 u32 = 6 [packed = true];
                repeated int64 i64 = 7 [packed = true];
                repeated sint64 si64 = 8 [packed = true];
                repeated uint64 u64 = 9 [packed = true];
                repeated fixed32 f32 = 10 [packed = true];
                repeated sfixed32 sf32 = 11 [packed = true];
                repeated fixed64 f64 = 12 [packed = true];
                repeated sfixed64 sf64 = 13 [packed = true];
                repeated string s = 14 [packed = true];
                repeated bytes by = 15 [packed = true];
                })"sv,
              0 },
            { R"(message Message{
                bool b = 1 [default = true];
                float f = 2 [default = 0.4];
                double d = 3 [default = 0.];
                int32 i32 = 4 [default = -2];
                sint32 si32 = 5 [default = -3];
                uint32 u32 = 6 [default = 5];
                int64 i64 = 7 [default = 7];
                sint64 si64 = 8 [default = 8];
                uint64 u64 = 9 [default = 6];
                fixed32 f32 = 10 [default = 3];
                sfixed32 sf32 = 11 [default = 6];
                fixed64 f64 = 12 [default = 3];
                sfixed64 sf64 = 13 [default = 4];
                string s = 14 [default = "true"];
                bytes by = 15;
                })"sv,
              0 },
            { R"(message Message{
                    bool b = X;
                })"sv,
              2 },
        };
        test_files( tests );
    }
    SUBCASE( "option" )
    {
        constexpr proto_file_test tests[] = { { R"(package UnitTest;
            option cc_enable_arenas true;
            )",
                                                2 },
                                              { R"(package UnitTest;
            option cc_enable_arenas = true;
            )",
                                                0 },
                                              { R"(syntax = "proto3";
                message Message
                {
                    required uint32 value = 1 [default = 2, deprecated = true];
                })"sv,
                                                0 } };
        test_files( tests );
    }
    SUBCASE( "extensions" )
    {
        SUBCASE( "enum type" )
        {
            constexpr proto_file_test tests[] = { { R"(syntax = "proto2";
                //[[enum.type = "float"]]
                enum Enum {
                    value = 1;
                })"sv,
                                                    2 }

            };
            test_files( tests );
        }
        SUBCASE( "bitfield" )
        {
            constexpr proto_file_test tests[] = {
                { R"(syntax = "proto2";
                message ReqUint8_1{
                    //[[ field.type = "uint8:1" ]]
                    required uint32 value = 1;
                    //[[ field.type = "int8:1" ]]
                    required sint32 value2 = 2;
                    //[[ field.type = "int8:1" ]]
                    required sint64 value3 = 3;
                    //[[ field.type = "uint8:1" ]]
                    required fixed32 value4 = 4;
                    //[[ field.type = "int8:1" ]]
                    required sfixed32 value5 = 5;
                    //[[ field.type = "int8:1" ]]
                    required sfixed64 value6 = 6;
                    //[[ field.type = "uint8:1" ]]
                    required fixed64 value7 = 7;
                })"sv,
                  0 },
                { R"(syntax = "proto2";
                message OptUint8_1{
                    //[[ field.type = "uint8:1" ]]
                    optional uint32 value = 1;
                })",
                  3 },
                { R"(syntax = "proto2";
                message OptUint8_1{
                    //[[ field.type = "uint8:1" ]]
                    required bool value = 1;
                })",
                  3 },
                { R"(syntax = "proto2";
                message OptUint8_1{
                    //[[ field.type = "uint8:1" ]]
                    required float value = 1;
                })",
                  3 },
                { R"(syntax = "proto2";
                message OptUint8_1{
                    //[[ field.type = "uint8:1" ]]
                    required double value = 1;
                })",
                  3 },

            };
            test_files( tests );
        }
        SUBCASE( "small int" )
        {
            constexpr proto_file_test tests[] = {
                { R"(package UnitTest;
                message A {
                    // [[ field.type = "uint8" ]]
                    optional uint32 u32 = 1;
                })",
                  0 },
                { R"(package UnitTest;
                message A {
                    // [[ field.type = "uint16" ]]
                    optional uint32 u32 = 1;
                })",
                  0 },
                { R"(package UnitTest;
                message A {
                    // [[ field.type = "uint32" ]]
                    optional uint32 u32 = 1;
                })",
                  0 },
                { R"(package UnitTest;
                message A {
                    // [[ field.type = "uint64" ]]
                    optional uint32 u32 = 1;
                })",
                  3 },
                { R"(syntax = "proto2";
                message ReqUint8_1{
                    //[[ field.type = "int8" ]]
                    required uint32 value = 1;
                })"sv,
                  3 },
            };
            test_files( tests );
        }
    }
    SUBCASE( "dependency" )
    {
        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            message A {
                optional A a = 1;
            })",
              0 },
            { R"(package UnitTest;
            message A {
                repeated A a = 1;
            })",
              0 },
            { R"(package UnitTest;
            message A {
                required A a = 1;
            })",
              3 },
            { R"(package UnitTest;
            message A {
                required B b = 1;
            }
            message B {
                required A a = 1;
            })",
              2 },
            { R"(package UnitTest;
            message A {
                message B {
                    required A a = 1;
                }
                required B b = 1;
            })",
              4 },
            { R"(package UnitTest;
            option cc_enable_arenas = true;
            message A {
                message B {
                    repeated A a = 1;
                    required bool b = 2 [ default = true ];
                    map<int32, int32> map_field = 3;
                }
                required B b = 1;
            })",
              0 },
            { R"(package UnitTest.dependency;
            message A {
                message B { optional C c = 1; }
                message C { optional int32 b = 1; }
                message D { optional A a = 1; }
                message E {
                    repeated F f = 1;
                    optional int32 b = 2 [default = -2];
                }
                message F {
                    repeated E e = 1;
                    optional int32 c = 2;
                }
                message G {
                    optional H h = 1;
                    optional int32 b = 2;
                }
                message H {
                    optional G g = 1;
                    optional int32 c = 2 [default = 2];
                }
                optional int32 a = 1;
            })",
              0 },
            { R"(package UnitTest;
            message A {
                oneof oneof_field {
                    uint32 oneof_uint32 = 1;
                    string oneof_string = 2;
                    bytes oneof_bytes = 3;
                    A a = 4;
                }
            })",
              2 },
        };
        test_files( tests );
    }
    SUBCASE( "oneof" )
    {
        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            message A {
                oneof oneof_field {
                    uint32 oneof_uint32 = 1;
                    string oneof_string = 2;
                    bytes oneof_bytes = 3;
                }
            })",
              0 },

        };
        test_files( tests );
    }
    SUBCASE( "reserved" )
    {
        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            message A {
                reserved 4;
                reserved 5,6,7;
                reserved 8 to 10;
                reserved 10 to max;
                reserved "BB";
            })",
              0 },
        };
        test_files( tests );
    }
    SUBCASE( "enum" )
    {
        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            message A {
                enum PhoneType {
                    MOBILE = 0;
                    HOME = 1;
                    WORK = 2;
                }
            })",
              0 },

        };
        test_files( tests );
    }
    SUBCASE( "map" )
    {
        constexpr proto_file_test tests[] = {
            { R"(package UnitTest;
            message A {
                map<int32, int32> m_int32 = 1;
                map<int64, int64> m_int64 = 2;
                map<uint32, uint32> m_uint32 = 3;
                map<uint64, uint64> m_uint64 = 4;
                map<sint32, sint32> m_sint32 = 5;
                map<sint64, sint64> m_sint64 = 6;
                map<fixed32, fixed32> m_fixed32 = 7;
                map<fixed64, fixed64> m_fixed64 = 8;
                map<sfixed32, sfixed32> m_sfixed32 = 9;
                map<sfixed64, sfixed64> m_sfixed64 = 10;
                map<bool, bool> m_bool = 11;
                map<string, bool> m_string = 12;
            })",
              0 },
            { R"(package UnitTest;
            message A {
                map<float, int32> m_int32 = 1;
            })",
              3 },
            { R"(package UnitTest;
            message A {
                map<double, int32> m_int32 = 1;
            })",
              3 },
            { R"(package UnitTest;
            message A {
                map<bytes, int32> m_int32 = 1;
            })",
              3 },
            { R"(package UnitTest;
            message A {
                map<X, int32> m_int32 = 1;
            })",
              3 },

        };
        test_files( tests );
    }
}
