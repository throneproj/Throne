#include <cstdint>
#include <cstdlib>
#include <proto/message.pb.h>
#include <proto/proto3.pb.h>
#include <scalar.pb.h>
#include <string_view>

template <typename T> static void pb_decode(const uint8_t *Data, size_t Size)
{
    try
    {
        T obj{};
        spb::pb::deserialize(obj, Data, Size);
    }
    catch (...)
    {
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pb_decode<tutorial::Person>(Data, Size);
    pb_decode<proto3_unittest::TestAllTypes>(Data, Size);
    pb_decode<proto3_unittest::TestEmptyMessage>(Data, Size);

    return 0;
}
