#define ANKERL_NANOBENCH_IMPLEMENT
#include "../nanobench.h"
#include "common.h"
#ifdef GPB_LITE
#define GPB_NAME "gpb-lite"
#else
#define GPB_NAME "gpb"
#include "google/protobuf/util/json_util.h"
#endif
#include <string>

int main()
{
    std::string buffer;
    buffer.reserve(1024);

    AddressBook book;
    init_message(book);

    ankerl::nanobench::Bench().minEpochIterations(100000).run(GPB_NAME "-pb-serialize",
                                                              [&]
                                                              {
                                                                  const auto res =
                                                                      book.SerializeToString(&buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(res);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(GPB_NAME "-pb-init-serialize",
                                                              [&buffer]
                                                              {
                                                                  AddressBook book;
                                                                  init_message(book);
                                                                  const auto res =
                                                                      book.SerializeToString(&buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(res);
                                                              });

    ankerl::nanobench::Bench().minEpochIterations(100000).run(GPB_NAME "-pb-deserialize",
                                                              [&buffer]
                                                              {
                                                                  AddressBook book;
                                                                  const auto res =
                                                                      book.ParseFromString(buffer);
                                                                  ankerl::nanobench::doNotOptimizeAway(res);
                                                              });
#ifndef GPB_LITE
    ankerl::nanobench::Bench().minEpochIterations(10000).run(
        GPB_NAME "-json-serialize",
        [&]
        {
            buffer.clear();
            const auto res = google::protobuf::util::MessageToJsonString(book, &buffer);
            ankerl::nanobench::doNotOptimizeAway(res);
        });

    ankerl::nanobench::Bench().minEpochIterations(10000).run(
        GPB_NAME "-json-init-serialize",
        [&buffer]
        {
            AddressBook book;
            init_message(book);
            buffer.clear();
            const auto res = google::protobuf::util::MessageToJsonString(book, &buffer);
            ankerl::nanobench::doNotOptimizeAway(res);
        });

    ankerl::nanobench::Bench().minEpochIterations(10000).run(
        GPB_NAME "-json-deserialize",
        [&]
        {
            AddressBook book;
            const auto res = google::protobuf::util::JsonStringToMessage(buffer, &book);
            ankerl::nanobench::doNotOptimizeAway(book);
        });
#endif
}
