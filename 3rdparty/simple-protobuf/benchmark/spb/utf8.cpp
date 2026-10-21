#define ANKERL_NANOBENCH_IMPLEMENT
#include "../nanobench.h"
#include <spb/utf8.h>
#include <string>

int main()
{
    std::string buffer(1024, 'a');

    ankerl::nanobench::Bench().minEpochIterations(100000).run(
        "utf8-is_valid",
        [&]
        {
            const auto result = spb::detail::utf8::is_valid(buffer);
            ankerl::nanobench::doNotOptimizeAway(result);
        });
}
