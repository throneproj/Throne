/***************************************************************************\
* Name        : buffered reader                                             *
* Description : buffer between io::reader and detail::istream              *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once
#include "io.hpp"
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <sys/types.h>

namespace spb::io
{
class buffered_reader
{
  public:
    static constexpr size_t BUFFER_SIZE = 256;

  private:
    using buffer_index_type = uint16_t;
    std::array<char, BUFFER_SIZE> buffer;
    io::reader on_read;
    buffer_index_type begin_index = 0;
    buffer_index_type end_index = 0;
    bool eof_reached = false;

    auto bytes_in_buffer() const noexcept -> size_t
    {
        return end_index - begin_index;
    }

    auto space_left_in_buffer() const noexcept -> size_t
    {
        return buffer.size() - end_index;
    }

    void shift_data_to_start() noexcept
    {
        if (begin_index == 0)
            return;

        memmove(buffer.data(), buffer.data() + begin_index, bytes_in_buffer());
        end_index -= begin_index;
        begin_index = 0;
    }

    void read_buffer()
    {
        shift_data_to_start();

        while (bytes_in_buffer() < buffer.size() && !eof_reached)
        {
            auto bytes_in =
                static_cast<buffer_index_type>(on_read(&buffer[end_index], space_left_in_buffer()));
            eof_reached |= bytes_in == 0;
            end_index += bytes_in;
        }
    }

  public:
    explicit buffered_reader(io::reader reader) : on_read(reader)
    {
    }

    [[nodiscard]] auto view(size_t minimal_size) -> std::string_view
    {
        minimal_size = std::max<size_t>(minimal_size, 1U);
        if (bytes_in_buffer() < minimal_size)
            read_buffer();

        return std::string_view(&buffer[begin_index], bytes_in_buffer());
    }

    void skip(size_t size) noexcept
    {
        assert(size <= bytes_in_buffer());
        begin_index += static_cast<buffer_index_type>(size);
    }
};

} // namespace spb::io
