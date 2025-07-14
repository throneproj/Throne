#include <cstdint>
#include <cstdlib>
#include <string_view>

#include <parser/parser.h>

extern "C" int LLVMFuzzerTestOneInput( const uint8_t * Data, size_t Size )
{
    auto file = proto_file{
        .content = std::string( reinterpret_cast< const char * >( Data ), Size ),
    };
    try
    {
        parse_proto_file_content( file );
    }
    catch( ... )
    {
    }
    return 0;
}