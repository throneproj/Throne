#include "common.h"
#include <string>

int main()
{
    std::string buffer;
    AddressBook book;
    init_message(book);
    const auto res = book.SerializeToString(&buffer);
    return res ? 0 : 1;
}
