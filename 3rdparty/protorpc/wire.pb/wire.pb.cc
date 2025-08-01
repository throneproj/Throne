#include "wire.pb.h"
#include <spb/json.hpp>
#include <system_error>
#include <type_traits>

namespace spb::json
{
namespace detail
{
void serialize_value( detail::ostream & stream, const ::protorpc::wire::Const & value )
{
	switch( value )
	{
	case ::protorpc::wire::Const::ZERO:
		return stream.serialize( "ZERO"sv);
	case ::protorpc::wire::Const::MAX_REQUEST_HEADER_LEN:
		return stream.serialize( "MAX_REQUEST_HEADER_LEN"sv);
	default:
		throw std::system_error( std::make_error_code( std::errc::invalid_argument ) );
	}
}

void deserialize_value( detail::istream & stream, ::protorpc::wire::Const & value )
{
	auto enum_value = stream.deserialize_string_or_int( 4, 22 );
	std::visit( detail::overloaded{
		[&]( std::string_view enum_str )
		{
			const auto enum_hash = djb2_hash( enum_str );
			switch( enum_hash )
			{
			case detail::djb2_hash( "ZERO"sv ):
				if( enum_str == "ZERO"sv ){
					value = ::protorpc::wire::Const::ZERO;
					return ;				}
				break ;
			case detail::djb2_hash( "MAX_REQUEST_HEADER_LEN"sv ):
				if( enum_str == "MAX_REQUEST_HEADER_LEN"sv ){
					value = ::protorpc::wire::Const::MAX_REQUEST_HEADER_LEN;
					return ;				}
				break ;
			}
			throw std::system_error( std::make_error_code( std::errc::invalid_argument ) );
		},
		[&]( int32_t enum_int )
		{
			switch( ::protorpc::wire::Const( enum_int ) )
			{
			case ::protorpc::wire::Const::ZERO:
			case ::protorpc::wire::Const::MAX_REQUEST_HEADER_LEN:
				value = ::protorpc::wire::Const( enum_int );
				return ;
			}
			throw std::system_error( std::make_error_code( std::errc::invalid_argument ) );
		}
	}, enum_value );
}

} // namespace detail
namespace detail
{
void serialize_value( detail::ostream & stream, const ::protorpc::wire::RequestHeader & value )
{
	stream.serialize( "id"sv, value.id );
	stream.serialize( "method"sv, value.method );
	stream.serialize( "raw_request_len"sv, value.raw_request_len );
}
void deserialize_value( detail::istream & stream, ::protorpc::wire::RequestHeader & value )
{
	auto key = stream.deserialize_key( 2, 15 );
	switch( djb2_hash( key ) )
	{
		case detail::djb2_hash( "id"sv ):
			if( key == "id"sv )
			{
				return stream.deserialize( value.id );
			}
				break;
		case detail::djb2_hash( "method"sv ):
			if( key == "method"sv )
			{
				return stream.deserialize( value.method );
			}
				break;
		case detail::djb2_hash( "raw_request_len"sv ):
			if( key == "raw_request_len"sv )
			{
				return stream.deserialize( value.raw_request_len );
			}
				break;
		case detail::djb2_hash( "rawRequestLen"sv ):
			if( key == "rawRequestLen"sv )
			{
				return stream.deserialize( value.raw_request_len );
			}
			break;
	}
	return stream.skip_value( );
}
} // namespace detail
namespace detail
{
void serialize_value( detail::ostream & stream, const ::protorpc::wire::ResponseHeader & value )
{
	stream.serialize( "id"sv, value.id );
	stream.serialize( "error"sv, value.error );
	stream.serialize( "raw_response_len"sv, value.raw_response_len );
}
void deserialize_value( detail::istream & stream, ::protorpc::wire::ResponseHeader & value )
{
	auto key = stream.deserialize_key( 2, 16 );
	switch( djb2_hash( key ) )
	{
		case detail::djb2_hash( "id"sv ):
			if( key == "id"sv )
			{
				return stream.deserialize( value.id );
			}
				break;
		case detail::djb2_hash( "error"sv ):
			if( key == "error"sv )
			{
				return stream.deserialize( value.error );
			}
				break;
		case detail::djb2_hash( "rawResponseLen"sv ):
			if( key == "rawResponseLen"sv )
			{
				return stream.deserialize( value.raw_response_len );
			}
				break;
		case detail::djb2_hash( "raw_response_len"sv ):
			if( key == "raw_response_len"sv )
			{
				return stream.deserialize( value.raw_response_len );
			}
			break;
	}
	return stream.skip_value( );
}
} // namespace detail
} // namespace spb::json
#include "wire.pb.h"
#include <spb/pb.hpp>
#include <type_traits>

namespace spb::pb
{
namespace detail
{
void serialize( detail::ostream & stream, const ::protorpc::wire::RequestHeader & value )
{
	stream.serialize_as<scalar_encoder::varint>( 1, value.id );
	stream.serialize( 2, value.method );
	stream.serialize_as<scalar_encoder::varint>( 3, value.raw_request_len );
}
void deserialize_value( detail::istream & stream, ::protorpc::wire::RequestHeader & value, uint32_t tag )
{
	switch( field_from_tag( tag ) )
	{
		case 1:
			return stream.deserialize_as<scalar_encoder::varint>( value.id, tag );
		case 2:
			return stream.deserialize( value.method, tag );
		case 3:
			return stream.deserialize_as<scalar_encoder::varint>( value.raw_request_len, tag );
		default:
			return stream.skip( tag );	
}
}

} // namespace detail
namespace detail
{
void serialize( detail::ostream & stream, const ::protorpc::wire::ResponseHeader & value )
{
	stream.serialize_as<scalar_encoder::varint>( 1, value.id );
	stream.serialize( 2, value.error );
	stream.serialize_as<scalar_encoder::varint>( 3, value.raw_response_len );
}
void deserialize_value( detail::istream & stream, ::protorpc::wire::ResponseHeader & value, uint32_t tag )
{
	switch( field_from_tag( tag ) )
	{
		case 1:
			return stream.deserialize_as<scalar_encoder::varint>( value.id, tag );
		case 2:
			return stream.deserialize( value.error, tag );
		case 3:
			return stream.deserialize_as<scalar_encoder::varint>( value.raw_response_len, tag );
		default:
			return stream.skip( tag );	
}
}

} // namespace detail
} // namespace spb::pb
