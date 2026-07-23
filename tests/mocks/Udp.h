#ifndef TESTS_MOCKS_UDP_H_
#define TESTS_MOCKS_UDP_H_

#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

class UDP
{
public:
    virtual ~UDP() {}

    virtual int begin(uint16_t)
    {
        return 1;
    }

    virtual int parsePacket()
    {
        return 0;
    }

    virtual int read(unsigned char *, size_t)
    {
        return 0;
    }

    virtual int beginPacket(IPAddress, uint16_t)
    {
        return 1;
    }

    virtual size_t write(const uint8_t *, size_t size)
    {
        return size;
    }

    virtual int endPacket()
    {
        return 1;
    }

    virtual IPAddress remoteIP()
    {
        return IPAddress();
    }

    virtual uint16_t remotePort()
    {
        return 0;
    }
};

#endif
