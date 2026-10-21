#include "common.h"

auto init_message() -> AddressBook
{
    return AddressBook{.people = {
                           {.name = "john",
                            .id = 587965,
                            .email = "john@carter.com",
                            .phones = {{.number = "777888999", .type = Person::PhoneType::PHONE_TYPE_MOBILE},
                                       {.number = "456895251", .type = Person::PhoneType::PHONE_TYPE_HOME}}},
                           {.name = "hanz",
                            .id = 89,
                            .email = "hanz@obergartenmeister.at",
                            .phones = {{.number = "5689571", .type = Person::PhoneType::PHONE_TYPE_MOBILE},
                                       {.number = "6589652", .type = Person::PhoneType::PHONE_TYPE_HOME}}},
                       }};
}
