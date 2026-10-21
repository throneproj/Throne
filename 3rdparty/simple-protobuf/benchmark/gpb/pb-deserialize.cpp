#include "common.h"
#include <string>

int main()
{
    std::string buffer(127, 'g');
    AddressBook addressbook;
    const auto res = addressbook.ParseFromString(buffer);
    return res ? 0 : 1;
}
