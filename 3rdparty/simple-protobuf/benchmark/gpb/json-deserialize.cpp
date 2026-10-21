#include "common.h"
#include "google/protobuf/util/json_util.h"
#include <string>

int main()
{
    std::string buffer(127, 'g');
    AddressBook addressbook;
    const auto res = google::protobuf::util::JsonStringToMessage(buffer, &addressbook);
    return res.ok() ? 0 : 1;
}
