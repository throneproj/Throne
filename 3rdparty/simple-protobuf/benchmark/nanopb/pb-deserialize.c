#include "common.h"
#include <pb_decode.h>

int main(int argc, char *argv[])
{
    pb_istream_t istream = pb_istream_from_buffer((uint8_t *)argv[0], argc);
    AddressBook decoded  = AddressBook_init_zero;
    bool res             = pb_decode(&istream, AddressBook_fields, &decoded);
    return res;
}
