/***************************************************************************\
* Name        : C++ ostream with indentation                                *
* Description : Insert space indentation after new line. '{', '}' alter indentation *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <ostream>

#include <ostream>
#include <streambuf>
#include <string>

class indented_streambuf : public std::streambuf
{
  private:
    std::streambuf *dest;
    int indent_level = 0;
    int last_char    = 0;
    std::string indent;
    static constexpr int indent_spaces = 4;

  public:
    explicit indented_streambuf(std::streambuf *dest_buf) : dest(dest_buf)
    {
    }

    void set_indent(int level)
    {
        if (level >= 0)
        {
            indent.assign(level * indent_spaces, ' ');
            indent_level = level;
        }
    }

  protected:
    virtual int_type overflow(int_type ch) override
    {
        if (ch == traits_type::eof())
            return traits_type::eof();

        if (ch == '}')
            set_indent(indent_level - 1);

        if (last_char == '\n' && ch != '\n')
        {
            if (dest->sputn(indent.data(), indent.size()) != std::streamsize(indent.size()))
                return traits_type::eof();
        }
        if (ch == '{')
            set_indent(indent_level + 1);

        last_char = ch;
        return dest->sputc(ch);
    }

    virtual std::streamsize xsputn(const char *s, std::streamsize n) override
    {
        std::streamsize written = 0;

        for (std::streamsize i = 0; i < n; ++i)
        {
            if (overflow(s[i]) == traits_type::eof())
                break;
            ++written;
        }

        return written;
    }
};

class indented_ostream : public std::ostream
{
  private:
    indented_streambuf buffer;

  public:
    explicit indented_ostream(std::ostream &os) : std::ostream(&buffer), buffer(os.rdbuf())
    {
    }
};
