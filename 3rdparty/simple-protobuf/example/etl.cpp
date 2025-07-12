#include "spb/pb.hpp"
#include <etl.pb.h>
#include <iostream>

auto create_command( uint8_t arg ) -> ETL::Example::Command
{
    return {
        .command_id = uint8_t( ETL::Example::Command::CMD::CMD_READ ),
        .arg        = arg,
        .in_flag    = 1,
        .out_flag   = 0,
    };
}

auto main( int, char *[] ) -> int
{
    std::cout << "sizeof(Command): " << sizeof( ETL::Example::Command ) << std::endl;
    std::cout << "sizeof(DeviceStatus): " << sizeof( ETL::Example::DeviceStatus ) << std::endl;
    std::cout << "sizeof(CommandQueue): " << sizeof( ETL::Example::CommandQueue ) << std::endl;

    return 0;
}