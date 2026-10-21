# simple-protobuf

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C)](https://cmake.org/)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tonda-kriz/simple-protobuf)
[![Linux-build](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-linux-tests.yml/badge.svg)](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-linux-tests.yml)
[![Windows-build](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-windows-tests.yml/badge.svg)](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-windows-tests.yml)
[![Mac-build](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-macos-tests.yml/badge.svg)](https://github.com/tonda-kriz/simple-protobuf/actions/workflows/ci-macos-tests.yml)
[![Library-coverage](https://tonda-kriz.github.io/simple-protobuf/library/coverage.svg)](https://tonda-kriz.github.io/simple-protobuf/library)

**A lightweight C++20 library for Protocol Buffers and JSON serialization.**
 - **Simple native C++ structs** with zero boilerplate
 - **Fast** as [Google Protocol Buffers](https://github.com/protocolbuffers/protobuf), **tiny** as [nanopb](https://github.com/nanopb/nanopb)
 - **Fully self-contained** — No `protoc`, no Google toolchain, no external dependencies

### Usage

1. Write standard `.proto` files (proto2 or proto3).
2. Run the included **`sprotoc`** tool to generate clean C++ source files (`.pb.h` + `.pb.cc`).
3. Serialize to **Protobuf** (wire-compatible) or **JSON** (Google-compatible) with one function call.

### Example

```proto
// proto/person.proto, hand written
package PhoneBook;

message Person {
  optional string name = 1;
  optional int32 id = 2;  // Unique ID number for this person.
  optional string email = 3;

  enum PhoneType {
    MOBILE = 0;
    HOME = 1;
    WORK = 2;
  }

  message PhoneNumber {
    required string number = 1; // phone number is always required
    optional PhoneType type = 2;
  }

  // all registered phones
  repeated PhoneNumber phones = 4;
}
```

```bash
# CMakeLists.txt, hand written
add_subdirectory(external/simple-protobuf) # or FetchContent
add_executable(myapp main.cpp proto/person.proto)
spb_protobuf_generate(TARGET myapp)
```

```CPP
// proto/person.pb.h, generated
namespace PhoneBook
{
struct Person {
    enum class PhoneType : int32_t {
        MOBILE = 0,
        HOME   = 1,
        WORK   = 2,
    };
    struct PhoneNumber {
        // phone number is always required
        std::string number;
        std::optional<PhoneType> type;
    };
    std::optional<std::string> name;
    // Unique ID number for this person.
    std::optional<int32_t> id;
    std::optional<std::string> email;
    // all registered phones
    std::vector<PhoneNumber> phones;
};
} // namespace PhoneBook
```

```CPP
// main.cpp, hand written
#include <iostream>
#include <proto/person.pb.h>   // <- generated

int main() {
    const auto john = phonebook::Person{
        .name   = "John Doe",
        .id     = 1234,
        .email  = "john@example.com",
        .phones = {{.number = "123456789", .type = phonebook::Person::PhoneType::MOBILE}}
    };

    // JSON round-trip
    const auto json = spb::json::serialize<std::string>(john);
    std::cout << "JSON:\n" << json << "\n\n";

    // Protobuf binary round-trip
    const auto pb_bytes = spb::pb::serialize<std::vector<std::byte>>(john);

    const auto decoded_json = spb::json::deserialize<phonebook::Person>(json);
    const auto decoded_pb = spb::pb::deserialize<phonebook::Person>(pb_bytes);

    // All equal: john == decoded_json == decoded_pb
}
```

## Features

* Fully self-contained, no `protoc`, no `Google libs`, no `external` dependencies - instead it uses its own proto-compiler called `sprotoc`.
* Full `proto2`/`proto3` support (no editions)
* Generates clean, modern C++ with `std::optional`, `std::vector`, and `enum class`.
* Protobuf wire format is 100% compatible with Google libraries, Python, Go, Java...
* JSON serialization is compatible with Google's JSON mapping
* Embedded-friendly: the library itself performs `zero heap allocations` - only user data (dynamic strings/vectors) may allocate, which can be eliminated with for [example](example/generated/etl.pb.h) [ETL](https://github.com/ETLCPP/etl) or `static strings/vectors` (std::array<...>)
* Highly configurable via [options](doc/options.md)
  * `max_count` for `repeated fields` and `max_size` for `bytes`/`string`
  * Supports user-defined types (including bitfields and embedded containers), see [spb_options.proto](example/proto/spb_options.proto) with generated [spb_options.pb.h](example/generated/spb_options.pb.h)

## Dependencies

* C++ compiler with C++20 support
* CMake (for build integration)
* Standard C++ library
* Optional: clang-format for code formatting

## Type mapping

| Proto | CPP type | Notes |
|------------|----------|--------------|
| `bool`     | `bool` |  |
| `float`/`double`   | `float`/`double` | |
| `int32`/`sint32`/`sfixed32` | `int32_t` |  |
| `fixed32`/`uint32` | `uint32_t` | |
| `int64`/`sint64`/`sfixed64` | `int64_t` | |
| `fixed64`/`uint64` | `uint64_t` | |
| `string`   | `std::string` | UTF-8 (Validated during de/serialize) |
| `bytes`    | `std::vector<std::byte>` | [Base64](https://en.wikipedia.org/wiki/Base64) in JSON |
| `message`  | `struct` | |
| `enum`     | `enum class` | |
| `repeated` | `std::vector<MessageT>` | |
| `optional` | `std::optional<MessageT>` | Or `std::unique_ptr<MessageT>` for cycle dependencies|
| `map`      | `std::map<KeyT, ValueT>` | |
| `oneof`    | `std::variant<std::monostate, ...>` | |

**All types** can be user-specified, see [options](doc/options.md).

## Examples

See the [example](example/) directory.

## Doc

* [API](doc/API.md)
* [Options](doc/options.md)
* [deepwiki](https://deepwiki.com/tonda-kriz/simple-protobuf)

## Performance

_Fast as [Google Protocol Buffers](https://github.com/protocolbuffers/protobuf), tiny as [nanopb](https://github.com/nanopb/nanopb)_

Measured on `Linux/i7-8650U CPU @ 1.90GHz` with GCC 16.1.1 `-flto -O2` using [nanobench](https://github.com/martinus/nanobench).

* Protobuf serialization/deserialization: **As fast as** [Google Protocol Buffers](https://github.com/protocolbuffers/protobuf).
* JSON serialization/deserialization: **~8x faster** than [Google Protocol Buffers](https://github.com/protocolbuffers/protobuf).
* Binary size (stripped executables): **As tiny as** [nanopb](https://github.com/nanopb/nanopb), which makes it ideal for Embedded systems.

![Speed benchmark](benchmark/img/speed-benchmark.png)
![Size benchmark](benchmark/img/file-size-benchmark.png)
See the [benchmark](benchmark/) directory for more details.

## Missing features

* gRPC is not implemented

## Status

* [x] Make it work
* [x] Make it right
* [x] Make it fast
