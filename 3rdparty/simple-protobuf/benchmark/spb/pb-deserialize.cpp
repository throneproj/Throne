#include "common.h"
#include <string>

int main()
{
    std::string buffer(127, '-');
    const auto book = spb::pb::deserialize<AddressBook>(buffer);
    return book.people.empty() ? 0 : 1;
}
