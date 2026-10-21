#include "common.h"
#include "google/protobuf/util/json_util.h"
#include <string>

int main()
{
    std::string buffer;
    AddressBook book;
    init_message(book);
    const auto res = google::protobuf::util::MessageToJsonString(book, &buffer);
    return res.ok() ? 0 : 1;
}
