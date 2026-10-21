#include <cstdint>
#include <cstdlib>
#include <proto/message.pb.h>
#include <proto/proto3.pb.h>
#include <scalar.pb.h>

template <typename T> static void json_decode(const uint8_t *Data, size_t Size)
{
    try
    {
        T obj{};
        spb::json::deserialize(obj, Data, Size);
    }
    catch (...)
    {
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    json_decode<tutorial::Person>(Data, Size);
    json_decode<proto3_unittest::TestAllTypes>(Data, Size);
    json_decode<proto3_unittest::TestEmptyMessage>(Data, Size);

    return 0;
}
