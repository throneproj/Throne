#include <cstdint>
#include <cstdlib>
#include <map.pb.h>
#include <person.pb.h>
#include <proto3.pb.h>
#include <scalar.pb.h>
#include <string_view>

template < typename T >
static void pb_decode( const uint8_t * Data, size_t Size )
{
    try
    {
        auto result = spb::pb::deserialize< T >( { reinterpret_cast< const char * >( Data ), Size } );
    }
    catch( ... )
    {
    }
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t * Data, size_t Size )
{
    pb_decode< PhoneBook::Person >( Data, Size );
    pb_decode< proto3_unittest::TestAllTypes >( Data, Size );
    pb_decode< proto3_unittest::TestEmptyMessage >( Data, Size );

    return 0;
}