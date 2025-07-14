//- this example is based on https://protobuf.dev/getting-started/cpptutorial/

#include <addressbook.pb.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace
{

auto load_file( const std::filesystem::path & file_path ) -> std::string
{
    if( !std::filesystem::exists( file_path ) )
    {
        return "{}";
    }
    const auto file_size = std::filesystem::file_size( file_path );
    auto file_content    = std::string( file_size, '\0' );

    if( auto * p_file = fopen( file_path.string( ).c_str( ), "rb" ); p_file )
    {
        const auto read = fread( file_content.data( ), 1, file_content.size( ), p_file );
        fclose( p_file );
        file_content.resize( read );
        return file_content;
    }

    perror( file_path.string( ).c_str( ) );
    throw std::system_error( std::make_error_code( std::errc( errno ) ) );
}

void save_file( const std::filesystem::path & file_path, std::string_view file_content )
{
    if( auto * p_file = fopen( file_path.string( ).c_str( ), "wb" ); p_file )
    {
        const auto written = fwrite( file_content.data( ), 1, file_content.size( ), p_file );
        fclose( p_file );
        if( written == file_content.size( ) )
        {
            return;
        }
    }
    perror( file_path.string( ).c_str( ) );
    throw std::system_error( std::make_error_code( std::errc( errno ) ) );
}
// This function fills in a Person message based on user input.
void PromptForAddress( tutorial::Person & person )
{
    std::cout << "Enter person ID number: ";
    int id;
    std::cin >> id;
    person.id = id;
    std::cin.ignore( 256, '\n' );

    std::cout << "Enter name: ";
    getline( std::cin, person.name.emplace( ) );

    std::cout << "Enter email address (blank for none): ";
    std::string email;
    getline( std::cin, email );
    if( !email.empty( ) )
    {
        person.email = std::move( email );
    }

    while( true )
    {
        std::cout << "Enter a phone number (or leave blank to finish): ";
        std::string number;
        getline( std::cin, number );
        if( number.empty( ) )
        {
            break;
        }

        auto & phone_number = person.phones.emplace_back( );
        phone_number.number = number;

        std::cout << "Is this a mobile, home, or work phone? ";
        std::string type;
        getline( std::cin, type );
        if( type == "mobile" )
        {
            phone_number.type = tutorial::Person::PhoneType::PHONE_TYPE_MOBILE;
        }
        else if( type == "home" )
        {
            phone_number.type = tutorial::Person::PhoneType::PHONE_TYPE_HOME;
        }
        else if( type == "work" )
        {
            phone_number.type = tutorial::Person::PhoneType::PHONE_TYPE_WORK;
        }
        else
        {
            std::cout << "Unknown phone type.  Using default." << std::endl;
        }
    }
}
}// namespace

// Main function:  Reads the entire address book from a file,
//   adds one person based on user input, then writes it back out to the same
//   file.
auto main( int argc, char * argv[] ) -> int
{
    if( argc != 2 )
    {
        std::cerr << "Usage:  " << argv[ 0 ] << " ADDRESS_BOOK_FILE" << std::endl;
        return -1;
    }

    try
    {
        auto address_book =
            spb::json::deserialize< tutorial::AddressBook >( load_file( argv[ 1 ] ) );

        // Add an address.
        PromptForAddress( address_book.people.emplace_back( ) );

        // Write the new address book back to disk.
        save_file( argv[ 1 ], spb::json::serialize( address_book ) );
    }
    catch( const std::exception & e )
    {
        std::cerr << "Exception: " << e.what( ) << std::endl;
        return 1;
    }

    return 0;
}