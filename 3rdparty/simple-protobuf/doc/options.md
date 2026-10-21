# Options

TLDR: See [example/spb_options.proto](../example/proto/spb_options.proto) and [generated/spb_options.pb.h](../example/generated/spb_options.pb.h).

[options](../include/spb/proto/spb.proto) can be specified in a few different ways, and all are compatible with GPB.
1. standard GPB [options](https://protobuf.dev/programming-guides/proto2/#options)
2. in comments, so they are ignored by the GPB `protoc`. These options use C++ [attribute](https://en.cppreference.com/w/cpp/language/attributes) syntax.

**Notes:**
- Unknown options are ignored.
- `descriptor.proto` is not required because options are [hardcoded](../src/spb-proto-compiler/ast/ast-options.cpp) in the compiler.

## GPB options

Only the following GPB options are supported, other options are ignored:
- `default` -> [default field value](https://protobuf.com/docs/language-spec#pseudo-options)
- `packed` -> [repeated field encoding](https://protobuf.com/docs/language-spec#repeated-field-encoding)
- `deprecated` -> mark field as `[[deprecated]]`
- `json_name` -> [JSON name of the field](https://protobuf.com/docs/language-spec#pseudo-options)

## int types

You may also use 8-bit and 16-bit integer types (`int8`, `uint8`, `int16`, `uint16`) for both integer fields and enums.

**Warning:** to remain compatible with GPB, always use matching sign. For example, `int32` and `int8` are valid together, but `int32` and `uint8` are not.

```proto
//[[ (spb_opt).type = "int16" ]]
[ (spb_opt).type = "int16" ];
```

### bit fields

You may use bit fields such as `int8:1` or `uint64:2` for [ints](#int-types).

*Remember: always use types with matching sign.*

```proto
//[[ (spb_opt).type = "uint32:2" ]]
[ (spb_opt).type = "uint32:2" ];
```

## enum types

You can specify the storage type for an enum: `int8`, `uint8`, `int16`, `uint16`, or `int32`.

```proto
//[[ (spb_enumopt).type = "int16" ]]
option (spb_enumopt).type = "int16";

//[[ (spb_msgopt).enum = "int16" ]]
option (spb_msgopt).enum = "int16";

//[[ (spb_fileopt).enum = "int16" ]]
option (spb_fileopt).enum = "int16";
```

## container types

You can use custom container types for `optional`, `repeated`, `string`, and `bytes` fields.
Containers must satisfy a [concept](../include/spb/concepts.h).

| container  | [concept](../include/spb/concepts.h)     | notes       |
|------------|------------------------------|-------------|
| `optional` | `proto_label_optional`       | [`optional`](https://protobuf.dev/programming-guides/proto2/#field-labels) field label |
| `repeated` | `proto_label_repeated`       | [`repeated`](https://protobuf.dev/programming-guides/proto2/#field-labels) field label |
| `map`      | `proto_map`                  | [`map`](https://protobuf.dev/programming-guides/proto2/#maps) field type |
| `string`   | `proto_field_string`         | fixed-size [`string`](https://protobuf.dev/programming-guides/proto2/#scalar) field type |
| `string`   | `proto_field_string_resizable` | resizable [`string`](https://protobuf.dev/programming-guides/proto2/#scalar) field type |
| `bytes`    | `proto_field_bytes`          | fixed-size [`bytes`](https://protobuf.dev/programming-guides/proto2/#scalar) field type |
| `bytes`    | `proto_field_bytes_resizable` | resizable [`bytes`](https://protobuf.dev/programming-guides/proto2/#scalar) field type |

```proto
//[[ (spb_opt).optional = "std::optional<$>" ]]
[ (spb_opt).optional = "std::optional<$>" ];

//[[ (spb_msgopt).optional = "std::optional<$>" ]]
option (spb_msgopt).optional = "std::optional<$>";

//[[ (spb_fileopt).optional = "std::optional<$>" ]]
option (spb_fileopt).optional = "std::optional<$>";

//[[ (spb_opt).repeated = "std::vector<$>" ]]
[ (spb_opt).repeated = "std::vector<$>" ];

//[[ (spb_msgopt).repeated = "std::vector<$>" ]]
option (spb_msgopt).repeated = "std::vector<$>";

//[[ (spb_fileopt).repeated = "std::vector<$>" ]]
option (spb_fileopt).repeated = "std::vector<$>";

//[[ (spb_opt).map = "std::map<$, @>" ]]
[ (spb_opt).map = "std::map<$, @>" ];

//[[ (spb_msgopt).map = "std::map<$, @>" ]]
option (spb_msgopt).map = "std::map<$, @>";

//[[ (spb_fileopt).map = "std::map<$, @>" ]]
option (spb_fileopt).map = "std::map<$, @>";

//[[ (spb_opt).string = "std::string" ]]
[ (spb_opt).string = "std::string" ];

//[[ (spb_msgopt).string = "std::string" ]]
option (spb_msgopt).string = "std::string";

//[[ (spb_fileopt).string = "std::string" ]]
option (spb_fileopt).string = "std::string";

//[[ (spb_opt).bytes = "std::vector<$>" ]]
[ (spb_opt).bytes = "std::vector<$>" ];

//[[ (spb_msgopt).bytes = "std::vector<$>" ]]
option (spb_msgopt).bytes = "std::vector<$>";

//[[ (spb_fileopt).bytes = "std::vector<$>" ]]
option (spb_fileopt).bytes = "std::vector<$>";
```

`$` is replaced with the container's `value_type`.

- For `string`, `$` is replaced by `char`.
- For `bytes`, `$` is replaced by `std::byte`.
- For `map`, `$` is replaced by `key_type`, `@` is replaced by `mapped_type`.
- For `repeated` and `optional`, `$` is replaced by the field type specified in the `.proto` file.

## fixed size repeated fields

You can use a **fixed-size** container for repeated fields with simple element types (integers or floating point).
The field must be `repeated` and `packed` (packed is the default in proto3, for proto2 use `[packed = true]`). The container must satisfy concept [proto_label_repeated_fixed_size](../include/spb/concepts.h).

**Warning:** deserialization throws an exception if the repeated field length differs from the expected size.

```proto
//[[ (spb_opt).repeated = "std::fixed_size_simd<$,4>" ]]
[ (spb_opt).repeated = "std::array<$,32>" ];

//[[ (spb_msgopt).repeated = "std::fixed_size_simd<$,4>" ]]
option (spb_msgopt).repeated = "std::array<$,32>";

//[[ (spb_fileopt).repeated = "std::fixed_size_simd<$,4>" ]]
option (spb_fileopt).repeated = "std::array<$,32>";
```

## fixed size bytes and string

You can use **fixed-size** containers for `bytes` and `string` fields. They must satisfy concept [proto_field_bytes](../include/spb/concepts.h) or [proto_field_string](../include/spb/concepts.h).

**Warning:** deserialization throws an exception if the field size differs from the expected size.

```proto
//[[ (spb_opt).string = "std::array<$,4>" ]]
[ (spb_opt).string = "std::array<$,4>" ];

//[[ (spb_msgopt).string = "std::array<$,4>" ]]
option (spb_msgopt).string = "std::array<$,4>";

//[[ (spb_fileopt).string = "std::array<$,4>" ]]
option (spb_fileopt).string = "std::array<$,4>";

//[[ (spb_opt).bytes = "std::array<$,32>" ]]
[ (spb_opt).bytes = "std::array<$,32>" ];

//[[ (spb_msgopt).bytes = "std::array<$,32>" ]]
option (spb_msgopt).bytes = "std::array<$,32>";

//[[ (spb_fileopt).bytes = "std::array<uint8_t,32>" ]]
option (spb_fileopt).bytes = "std::array<uint8_t,32>";
```

## maximum size for bytes and string

You can set a maximum size in bytes for `bytes` or `string` fields (excluding the `\0` terminator).
Serialization or deserialization throws an exception if `real_size > max_size`.
This option is copied from [nanopb](https://github.com/nanopb/nanopb/blob/master/generator/proto/nanopb.proto), so the original name is also supported.

```proto
//[[ (spb_opt).max_size = 8]]
[ (spb_opt).max_size = 8]

//[[ (nanopb).max_size = 8]]
[ (nanopb).max_size = 8]

//[[ (nanopb).max_length = 7]]
[ (nanopb).max_length = 8]

//[[ (spb_msgopt).max_size = 8]]
option (spb_msgopt).max_size = 8;

//[[ (nanopb_msgopt).max_size = 8]]
option (nanopb_msgopt).max_size = 8;

//[[ (nanopb_msgopt).max_length = 7]]
option (nanopb_msgopt).max_length = 7;

//[[ (spb_fileopt).max_size = 8]]
option (spb_fileopt).max_size = 8;

//[[ (nanopb_fileopt).max_size = 8]]
option (nanopb_fileopt).max_size = 8;

//[[ (nanopb_fileopt).max_length = 7]]
option (nanopb_fileopt).max_length = 7;
```

## maximum length for repeated label

You can set a maximum number of elements for a repeated field.
Serialization or deserialization throws an exception if `real_count > max_count`.
This option is also copied from [nanopb](https://github.com/nanopb/nanopb/blob/master/generator/proto/nanopb.proto), so the original name is also supported.

```proto
//[[ (spb_opt).max_count = 8]]
[ (spb_opt).max_count = 8]

//[[ (nanopb).max_count = 8]]
[ (nanopb).max_count = 8]

//[[ (spb_msgopt).max_count = 8]]
option (spb_msgopt).max_count = 8;

//[[ (nanopb_msgopt).max_count = 8]]
option (nanopb_msgopt).max_count = 8;

//[[ (spb_fileopt).max_count = 8]]
option (spb_fileopt).max_count = 8;

//[[ (nanopb_fileopt).max_count = 8]]
option (nanopb_fileopt).max_count = 8;
```
