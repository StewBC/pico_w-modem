/*
 Print.h - Base class that provides print() and println()
 Copyright (c) 2008 David A. Mellis.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, Write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef Print_h
#define Print_h

#include <stdint.h>
#include <stddef.h>

#include "WString.h"
#include "Printable.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print
{
private:
    int write_error;
    size_t printNumber(unsigned long, uint8_t);
    size_t printFloat(double, uint8_t);

protected:
    void setWriteError(int err = 1)
    {
        write_error = err;
    }

public:
    Print() : write_error(0)
    {
    }

    int getWriteError()
    {
        return write_error;
    }
    void clearWriteError()
    {
        setWriteError(0);
    }

    virtual size_t Write(uint8_t) = 0;
    size_t Write(const char *str)
    {
        if (str == NULL)
            return 0;
        return Write((const uint8_t *)str, strlen_P(str));
    }
    virtual size_t Write(const uint8_t *buffer, size_t size);
    size_t Write(const char *buffer, size_t size)
    {
        return Write((const uint8_t *)buffer, size);
    }
    // These handle ambiguity for Write(0) case, because (0) can be a pointer or an integer
    inline size_t Write(short t) { return Write((uint8_t)t); }
    inline size_t Write(unsigned short t) { return Write((uint8_t)t); }
    inline size_t Write(int t) { return Write((uint8_t)t); }
    inline size_t Write(unsigned int t) { return Write((uint8_t)t); }
    inline size_t Write(long t) { return Write((uint8_t)t); }
    inline size_t Write(unsigned long t) { return Write((uint8_t)t); }
    // Enable Write(char) to fall through to Write(uint8_t)
    inline size_t Write(char c) { return Write((uint8_t)c); }
    inline size_t Write(int8_t c) { return Write((uint8_t)c); }

    size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
    // size_t printf_P(PGM_P format, ...) __attribute__((format(printf, 2, 3)));
    size_t print(const __FlashStringHelper *);
    size_t print(const String &);
    size_t print(const char[]);
    size_t print(char);
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const Printable &);

    size_t println(const __FlashStringHelper *);
    size_t println(const String &s);
    size_t println(const char[]);
    size_t println(char);
    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const Printable &);
    size_t println(void);

    virtual void flush()
    { /* Empty implementation for backward compatibility */
    }
};

#endif
