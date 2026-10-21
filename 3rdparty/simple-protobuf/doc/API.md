## API

All generated messages and enums use the API declared in [`../include/spb/json.hpp`](include/spb/json.hpp) and [`../include/spb/pb.hpp`](include/spb/pb.hpp).

```CPP
//- Serialize message via writer. All other `serialize` overloads are wrappers around this one.
//- example: `auto serialized_size = spb::pb::serialize( message, my_writer );`
auto serialize( const auto & message, spb::io::writer on_write ) -> size_t;

//- Return the size in bytes of the serialized message.
//- example: `auto serialized_size = spb::pb::serialize_size( message );`
auto serialize_size( const auto & message ) -> size_t;

//- Serialize message into a resizable container such as std::string or std::vector.
//- example: `auto serialized_size = spb::pb::serialize( message, my_string );`
template < typename Message, spb::resizable_container Container >
auto serialize( const Message & message, Container & result ) -> size_t;

//- Serialize message and return a resizable container such as std::string or std::vector.
//- example: `auto my_string = spb::pb::serialize< std::string >( message );`
template < spb::resizable_container Container = std::string, typename Message >
auto serialize( const Message & message ) -> Container;
```

```CPP
//- Deserialize message from a reader. All other `deserialize` overloads are wrappers around this one.
//- example: `spb::pb::deserialize( message, my_reader );`
void deserialize( auto & message, spb::io::reader on_read );

//- Deserialize message from a container such as std::string or std::vector.
//- example: `spb::pb::deserialize( message, my_string );`
template < typename Message, spb::size_container Container >
void deserialize( Message & message, const Container & protobuf );

//- Return deserialized message from a container such as std::string or std::vector.
//- example: `auto message = spb::pb::deserialize< Message >( my_string );`
template < typename Message, spb::size_container Container >
auto deserialize( const Container & protobuf ) -> Message;

//- Return deserialized message from a reader.
//- example: `auto message = spb::pb::deserialize< Message >( my_reader );`
template < typename Message >
auto deserialize( spb::io::reader reader ) -> Message;
```

The API is namespaced under `spb::json::` for JSON and `spb::pb::` for protobuf.
Template concepts [`spb::size_container`](../include/spb/concepts.h) and [`spb::resizable_container`](../include/spb/concepts.h) are defined in [`include/spb/concepts.h`](../include/spb/concepts.h).
`spb::io::reader` and `spb::io::writer` are user-supplied IO callback types defined in [`include/spb/io/io.hpp`](../include/spb/io/io.hpp).