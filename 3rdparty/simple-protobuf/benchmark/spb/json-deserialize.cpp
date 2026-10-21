#include "common.h"
#include <string>

int main()
{
    std::string buffer(127, '2');
    const auto book = spb::json::deserialize<AddressBook>(buffer);
    return book.people.empty() ? 0 : 1;
}
