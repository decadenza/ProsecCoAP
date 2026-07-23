#ifndef TESTS_MOCKS_ARDUINO_H_
#define TESTS_MOCKS_ARDUINO_H_

#include <stdint.h>
#include <stddef.h>
#include <string>

class String
{
private:
    std::string _value;

public:
    String() = default;
    String(const char *value) : _value(value == nullptr ? "" : value) {}

    String &operator=(const char *value)
    {
        _value = (value == nullptr ? "" : value);
        return *this;
    }

    String &operator+=(char c)
    {
        _value.push_back(c);
        return *this;
    }

    String &operator+=(const char *value)
    {
        if (value != nullptr)
        {
            _value += value;
        }
        return *this;
    }

    bool reserve(size_t size)
    {
        _value.reserve(size);
        return true;
    }

    size_t length() const
    {
        return _value.length();
    }

    const char *c_str() const
    {
        return _value.c_str();
    }
};

class IPAddress
{
private:
    uint8_t _bytes[4];

public:
    IPAddress() : _bytes{0, 0, 0, 0} {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : _bytes{a, b, c, d} {}

    uint8_t operator[](int index) const
    {
        return _bytes[index];
    }

    uint8_t &operator[](int index)
    {
        return _bytes[index];
    }

    bool operator==(const IPAddress &other) const
    {
        return _bytes[0] == other._bytes[0] &&
               _bytes[1] == other._bytes[1] &&
               _bytes[2] == other._bytes[2] &&
               _bytes[3] == other._bytes[3];
    }

    bool operator!=(const IPAddress &other) const
    {
        return !(*this == other);
    }
};

inline unsigned long millis()
{
    static unsigned long now = 0;
    now += 100;
    return now;
}

inline long random(long min, long max)
{
    if (max <= min)
    {
        return min;
    }
    return min;
}

#endif
