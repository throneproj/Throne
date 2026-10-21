#include "common.h"
#include <string>

int main()
{
    std::string buffer;
    const auto book = init_message();
    const auto size = spb::json::serialize(book, buffer);
    return size > 0 ? 0 : 1;
}
