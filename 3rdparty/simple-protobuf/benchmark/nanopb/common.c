#include "common.h"
#include "addressbook.pb.h"

void init_message(AddressBook *book)
{
    book->people_count = 2;
    {
        Person *person = &book->people[0];
        strncpy(person->name, "john", sizeof(person->name));
        person->id = 587965;
        strncpy(person->email, "john@carter.com", sizeof(person->email));

        person->phones_count = 2;
        strncpy(person->phones[0].number, "777888999", sizeof(person->phones[0].number));
        person->phones[0].type = Person_PhoneType_PHONE_TYPE_MOBILE;

        strncpy(person->phones[1].number, "456895251", sizeof(person->phones[1].number));
        person->phones[0].type = Person_PhoneType_PHONE_TYPE_HOME;
    }
    {
        Person *person = &book->people[1];
        strncpy(person->name, "hanz", sizeof(person->name));
        person->id = 89;
        strncpy(person->email, "hanz@obergartenmeister.at", sizeof(person->email));

        person->phones_count = 2;
        strncpy(person->phones[0].number, "5689571", sizeof(person->phones[0].number));
        person->phones[0].type = Person_PhoneType_PHONE_TYPE_MOBILE;

        strncpy(person->phones[1].number, "6589652", sizeof(person->phones[1].number));
        person->phones[1].type = Person_PhoneType_PHONE_TYPE_HOME;
    }
}
