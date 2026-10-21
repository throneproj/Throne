#define ANKERL_NANOBENCH_IMPLEMENT
#include "../nanobench.h"
extern "C"
{
#include "common.h"
}
#include <pb_decode.h>
#include <pb_encode.h>
#include <vector>

int main()
{
    std::vector<uint8_t> buffer;
    buffer.resize(1024);

    AddressBook book;
    init_message(&book);

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "nanopb-pb-serialize",
        [&]
        {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
            auto res            = pb_encode(&stream, AddressBook_fields, &book);
            ankerl::nanobench::doNotOptimizeAway(res);
        });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "nanopb-pb-init-serialize",
        [&buffer]
        {
            AddressBook book;
            init_message(&book);
            pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
            auto res            = pb_encode(&stream, AddressBook_fields, &book);
            ankerl::nanobench::doNotOptimizeAway(res);
        });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "nanopb-pb-deserialize",
        [&]
        {
            pb_istream_t istream = pb_istream_from_buffer(buffer.data(), buffer.size());
            AddressBook decoded  = AddressBook_init_zero;
            auto res             = pb_decode(&istream, AddressBook_fields, &decoded);
            ankerl::nanobench::doNotOptimizeAway(res);
        });
}
