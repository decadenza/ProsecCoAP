/**
 * @file Utils.h
 * @brief Utility functions for the ProsecCoAP library.
 * */
#include <Arduino.h>
#include <stdint.h>

namespace Coap::Utils
{
    /**
     * @brief Convert an integer-like value to network byte order (i.e. big-endian).
     *
     * The result must be stored in an array of bytes with the *exact* required size.
     */
    template <size_t N, typename T>
    void toNetworkByteOrder(T value, uint8_t (&result)[N])
    {
        for (size_t i = 0; i < N; ++i)
        {
            // Shift right by (N - 1 - i) * 8 bits, then mask the lowest byte
            result[i] = static_cast<uint8_t>((value >> ((N - 1 - i) * 8)) & 0xFF);
        }
    }
}