#include "Arduino.h"
#include "ProsecCoAP.h"
#include "utility/helpers.h"

uint16_t CoapGetNextMessageId()
{
    // Message ID is a simple sequential identifier.
    // However, to avoid collisions after a reset, start with a random value.
    static uint16_t id = (uint16_t)random(0, 0xFFFF);
    return id++;
}

void CoapGenerateRandomToken(uint8_t *buffer, size_t length)
{

    // Clamp length to maximum allowed token length to respect protocol specifications.
    length = length > COAP_MAX_TOKEN_LENGTH ? COAP_MAX_TOKEN_LENGTH : length;

    while (length > 0)
    {
        // Taking the full range of random values, including negative ones, and casting to uint32_t.
        // NOTE: It is more efficient to generate 4 bytes at a time.
        // The endianness is irrelevant for a random value.
        uint32_t r = random(0xF0000000, 0x0FFFFFFF);
        size_t chunkSize = length > 4 ? 4 : length;
        memcpy(buffer, &r, chunkSize);
        buffer += chunkSize;
        length -= chunkSize; // Length will become 0 on last loop.
    }
}
