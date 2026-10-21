#include <cstdint>
#include <cstdlib>

#include <parser/parser.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    auto file = proto_file{
        .content = std::string(reinterpret_cast<const char *>(Data), Size),
    };

    fs::path root;
    parsed_files files;
    parsing_ctx ctx{.base_dir = root, .already_parsed = files};
    try
    {
        parse_proto_file_content(file, ctx);
    }
    catch (...)
    {
    }
    return 0;
}
