/**
 * @file Utils.h
 * @brief Utility functions for the ProsecCoAP library.
 * */
#ifndef __PROSECCOAP_UTILS_H__
#define __PROSECCOAP_UTILS_H__

#include <Arduino.h>
#include <stdint.h>

/**
 * @namespace Coap::Utils
 * @brief Utilities.
 *
 * This namespace contains public utility functions and classes.
 */
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

    /**
     * @brief Custom implementation of ntohs (Network to Host Short) for 16-bit integers.
     */
    inline constexpr uint16_t ntohs(uint16_t net_short)
    {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // If the native platform is already Big-Endian, do nothing
        return net_short;
#else
        // On Little-Endian, swap the 2 bytes
        return static_cast<uint16_t>((((net_short) & 0xFF00u) >> 8) |
                                     (((net_short) & 0x00FFu) << 8));
#endif
    }

    /**
     * @brief Custom implementation of ntohl (Network to Host Long) for 32-bit integers.
     */
    inline constexpr uint32_t ntohl(uint32_t net_long)
    {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // If the native platform is already Big-Endian, do nothing
        return net_long;
#else
        // On Little-Endian, swap the 4 bytes
        return (((net_long & 0xFF000000ul) >> 24) |
                ((net_long & 0x00FF0000ul) >> 8) |
                ((net_long & 0x0000FF00ul) << 8) |
                ((net_long & 0x000000FFul) << 24));
#endif
    }

    /**
     * @brief Custom implementation of ntohll (Network to Host Long Long) for 64-bit integers.
     */
    inline constexpr uint64_t ntohll(uint64_t net_longlong)
    {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // If the native platform is already Big-Endian, do nothing
        return net_longlong;
#else
        // On Little-Endian, swap the 8 bytes
        return (((net_longlong & 0xFF00000000000000ull) >> 56) |
                ((net_longlong & 0x00FF000000000000ull) >> 40) |
                ((net_longlong & 0x0000FF0000000000ull) >> 24) |
                ((net_longlong & 0x000000FF00000000ull) >> 8) |
                ((net_longlong & 0x00000000FF000000ull) << 8) |
                ((net_longlong & 0x0000000000FF0000ull) << 24) |
                ((net_longlong & 0x000000000000FF00ull) << 40) |
                ((net_longlong & 0x00000000000000FFull) << 56));
#endif
    }
}

#endif