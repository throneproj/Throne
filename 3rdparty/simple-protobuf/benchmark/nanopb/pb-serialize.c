#include "common.h"
#include <pb_encode.h>
#include <stdint.h>

int main()
{
    uint8_t buffer[1024];
    AddressBook book;
    init_message(&book);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool res            = pb_encode(&stream, AddressBook_fields, &book);
    return res;
}
