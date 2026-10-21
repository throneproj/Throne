#pragma once

#ifdef GPB_LITE
#include "addressbook-lite.pb.h"
#else
#include "addressbook.pb.h"
#endif

void init_message(AddressBook &book);
