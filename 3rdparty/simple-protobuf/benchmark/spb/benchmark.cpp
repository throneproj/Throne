#define ANKERL_NANOBENCH_IMPLEMENT
#include "../nanobench.h"
#include "common.h"
#include <string>

int main()
{
    std::string buffer;
    buffer.reserve(1024);

    const auto book = init_message();

    ankerl::nanobench::Bench().minEpochIterations(100000).run("spb-pb-serialize",
                                                              [&]
                                                              {
                                                                  auto size =
                                                                      spb::pb::serialize(book, buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(size);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run("spb-pb-init-serialize",
                                                              [&buffer]
                                                              {
                                                                  const auto book = init_message();
                                                                  auto size =
                                                                      spb::pb::serialize(book, buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(size);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "spb-pb-deserialize",
        [&]
        {
            const auto book = spb::pb::deserialize<AddressBook>(buffer);
            ankerl::nanobench::doNotOptimizeAway(book);
        });

    ankerl::nanobench::Bench().minEpochIterations(100000).run("spb-json-serialize",
                                                              [&]
                                                              {
                                                                  auto size =
                                                                      spb::json::serialize(book, buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(size);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run("spb-json-init-serialize",
                                                              [&buffer]
                                                              {
                                                                  const auto book = init_message();
                                                                  auto size =
                                                                      spb::json::serialize(book, buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(size);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "spb-json-deserialize",
        [&]
        {
            const auto book = spb::json::deserialize<AddressBook>(buffer);
            ankerl::nanobench::doNotOptimizeAway(book);
        });
}
