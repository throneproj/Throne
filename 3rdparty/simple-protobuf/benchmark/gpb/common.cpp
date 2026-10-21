#include "common.h"

void init_message(AddressBook &book)
{
    {
        auto *person = book.add_people();
        person->set_name("john");
        person->set_id(587965);
        person->set_email("john@carter.com");
        {
            auto *phone = person->add_phones();
            phone->set_number("777888999");
            phone->set_type(Person_PhoneType::Person_PhoneType_PHONE_TYPE_MOBILE);
        }
        {
            auto *phone = person->add_phones();
            phone->set_number("456895251");
            phone->set_type(Person_PhoneType::Person_PhoneType_PHONE_TYPE_HOME);
        }
    }
    {
        auto *person = book.add_people();
        person->set_name("hanz");
        person->set_id(89);
        person->set_email("hanz@obergartenmeister.at");
        {
            auto *phone = person->add_phones();
            phone->set_number("5689571");
            phone->set_type(Person_PhoneType::Person_PhoneType_PHONE_TYPE_MOBILE);
        }
        {
            auto *phone = person->add_phones();
            phone->set_number("6589652");
            phone->set_type(Person_PhoneType::Person_PhoneType_PHONE_TYPE_HOME);
        }
    }
}
