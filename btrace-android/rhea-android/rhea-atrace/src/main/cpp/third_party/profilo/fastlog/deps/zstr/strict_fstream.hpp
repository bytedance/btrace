#ifndef __STRICT_FSTREAM_HPP
#define __STRICT_FSTREAM_HPP

#include <cassert>
#include <fstream>
#include <cstring>
#include <string>

/**
 * This namespace defines wrappers for std::ifstream, std::ofstream, and
 * std::fstream objects. The wrappers perform the following steps:
 * - check the open modes make sense
 * - check that the call to open() is successful
 * - (for input streams) check that the opened file is peek-able
 * - turn on the badbit in the exception mask
 */
namespace strict_fstream
{

/// Exception class thrown by failed operations.
class Exception
    : public std::exception
{
public:
    Exception(const std::string& msg) : _msg(msg) {}
    const char * what() const noexcept { return _msg.c_str(); }
private:
    std::string _msg;
}; // class Exception

namespace detail
{

struct static_method_holder
{
    static std::string mode_to_string(std::ios_base::openmode mode)
    {
        static const int n_modes = 6;
        static const std::ios_base::openmode mode_val_v[n_modes] =
            {
                std::ios_base::in,
                std::ios_base::out,
                std::ios_base::app,
                std::ios_base::ate,
                std::ios_base::trunc,
                std::ios_base::binary
            };

        static const char * mode_name_v[n_modes] =
            {
                "in",
                "out",
                "app",
                "ate",
                "trunc",
                "binary"
            };
        std::string res;
        for (size_t i = 0; i < n_modes; ++i)
        {
            if (mode & mode_val_v[i])
            {
                res += (not res.empty()? "|" : "");
                res += mode_name_v[i];
            }
        }
        if (res.empty()) res = "none";
        return res;
    }
    static void check_mode(const std::string& filename, std::ios_base::openmode mode)
    {
        if ((mode & std::ios_base::trunc) and not (mode & std::ios_base::out))
        {
            throw Exception(std::string("strict_fstream: open('") + filename + "'): mode error: trunc and not out");
        }
        else if ((mode & std::ios_base::app) and not (mode & std::ios_base::out))
        {
            throw Exception(std::string("strict_fstream: open('") + filename + "'): mode error: app and not out");
        }
        else if ((mode & std::ios_base::trunc) and (mode & std::ios_base::app))
        {
            throw Exception(std::string("strict_fstream: open('") + filename + "'): mode error: trunc and app");
        }
     }
    static void check_open(std::ios * s_p, const std::string& filename, std::ios_base::openmode mode)
    {
        if (s_p->fail())
        {
            throw Exception(std::string("strict_fstream: open('")
                            + filename + "'," + mode_to_string(mode) + "): open failed: "
                            + std::strerror(errno));
        }
    }
    static void check_peek(std::istream * is_p, const std::string& filename, std::ios_base::openmode mode)
    {
        bool peek_failed = true;
        try
        {
            is_p->peek();
            peek_failed = is_p->fail();
        }
        catch (std::ios_base::failure e) {}
        if (peek_failed)
        {
            throw Exception(std::string("strict_fstream: open('")
                            + filename + "'," + mode_to_string(mode) + "): peek failed: "
                            + std::strerror(errno));
        }
        is_p->clear();
    }
}; // struct static_method_holder

} // namespace detail

class ifstream
    : public std::ifstream
{
public:
    ifstream() = default;
    ifstream(const std::string& filename, std::ios_base::openmode mode = std::ios_base::in)
    {
        open(filename, mode);
    }
    void open(const std::string& filename, std::ios_base::openmode mode = std::ios_base::in)
    {
        mode |= std::ios_base::in;
        exceptions(std::ios_base::badbit);
        detail::static_method_holder::check_mode(filename, mode);
        std::ifstream::open(filename, mode);
        detail::static_method_holder::check_open(this, filename, mode);
        detail::static_method_holder::check_peek(this, filename, mode);
    }
}; // class ifstream

class ofstream
    : public std::ofstream
{
public:
    ofstream() = default;
    ofstream(const std::string& filename, std::ios_base::openmode mode = std::ios_base::out)
    {
        open(filename, mode);
    }
    void open(const std::string& filename, std::ios_base::openmode mode = std::ios_base::out)
    {
        mode |= std::ios_base::out;
        exceptions(std::ios_base::badbit);
        detail::static_method_holder::check_mode(filename, mode);
        std::ofstream::open(filename, mode);
        detail::static_method_holder::check_open(this, filename, mode);
    }
}; // class ofstream

class fstream
    : public std::fstream
{
public:
    fstream() = default;
    fstream(const std::string& filename, std::ios_base::openmode mode = std::ios_base::in)
    {
        open(filename, mode);
    }
    void open(const std::string& filename, std::ios_base::openmode mode = std::ios_base::in)
    {
        if (not (mode & std::ios_base::out)) mode |= std::ios_base::in;
        exceptions(std::ios_base::badbit);
        detail::static_method_holder::check_mode(filename, mode);
        std::fstream::open(filename, mode);
        detail::static_method_holder::check_open(this, filename, mode);
        detail::static_method_holder::check_peek(this, filename, mode);
    }
}; // class fstream

} // namespace strict_fstream

#endif
