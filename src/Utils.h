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
     * @param value The integer-like value to convert.
     * @param[out] result The array of bytes to store the result in.
     *
     * The result must be stored in an array of bytes with the *exact* required size.
     */
    template <size_t N, typename T>
    inline void toNetworkByteOrder(T value, uint8_t (&result)[N])
    {
        static_assert(N == sizeof(T), "Array size N must match the size of type T.");
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // If the host is already big-endian, we can just memcpy the value.
        memcpy(result, &value, N);
#else
        // The host is little endian, so we need to manually convert to big-endian.
        for (size_t i = 0; i < N; ++i)
        {
            // Shift right by (N - 1 - i) * 8 bits, then mask the lowest byte.
            result[i] = static_cast<uint8_t>((value >> ((N - 1 - i) * 8)) & 0xFF);
        }
#endif
    }

    /**
     * @brief Overload to convert a 32-bit float to network byte order.
     *
     * @param value The float value to convert.
     * @param[out] result The array of bytes to store the result in.
     */
    inline void toNetworkByteOrder(float value, uint8_t (&result)[4])
    {
        uint32_t int_value;
        memcpy(&int_value, &value, sizeof(float));
        toNetworkByteOrder<4>(int_value, result);
    }

    /**
     * @brief Overload to convert a 64-bit double to network byte order.
     *
     * @param value The double value to convert.
     * @param[out] result The array of bytes to store the result in.
     */
    inline void toNetworkByteOrder(double value, uint8_t (&result)[8])
    {
        uint64_t int_value;
        memcpy(&int_value, &value, sizeof(double));
        toNetworkByteOrder<8>(int_value, result);
    }

    // ========================================================================
    // UNSIGNED INTEGER READERS
    // ========================================================================

    // NOTE: The host byte order (big/little endian) does not matter, as the bit shift operator
    // will adjust accordingly.
    // See https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html

    /**
     * @brief Reads a 16-bit unsigned integer from a Big-Endian byte stream.
     */
    inline constexpr uint16_t read_uint16(const uint8_t *bytes)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                                     static_cast<uint16_t>(bytes[1]));
    }

    /**
     * @brief Reads a 32-bit unsigned integer from a Big-Endian byte stream.
     */
    inline constexpr uint32_t read_uint32(const uint8_t *bytes)
    {
        return (static_cast<uint32_t>(bytes[0]) << 24) |
               (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) |
               (static_cast<uint32_t>(bytes[3]));
    }

    /**
     * @brief Reads a 64-bit unsigned integer from a Big-Endian byte stream.
     */
    inline constexpr uint64_t read_uint64(const uint8_t *bytes)
    {
        return (static_cast<uint64_t>(bytes[0]) << 56) |
               (static_cast<uint64_t>(bytes[1]) << 48) |
               (static_cast<uint64_t>(bytes[2]) << 40) |
               (static_cast<uint64_t>(bytes[3]) << 32) |
               (static_cast<uint64_t>(bytes[4]) << 24) |
               (static_cast<uint64_t>(bytes[5]) << 16) |
               (static_cast<uint64_t>(bytes[6]) << 8) |
               (static_cast<uint64_t>(bytes[7]));
    }

    // ========================================================================
    // SIGNED INTEGER READERS
    // ========================================================================

    /**
     * @brief Reads a 16-bit signed integer from a Big-Endian byte stream.
     */
    inline constexpr int16_t read_int16(const uint8_t *bytes)
    {
        // Read as unsigned to avoid bitshift UB, then cast to signed.
        return static_cast<int16_t>(read_uint16(bytes));
    }

    /**
     * @brief Reads a 32-bit signed integer from a Big-Endian byte stream.
     */
    inline constexpr int32_t read_int32(const uint8_t *bytes)
    {
        return static_cast<int32_t>(read_uint32(bytes));
    }

    /**
     * @brief Reads a 64-bit signed integer from a Big-Endian byte stream.
     */
    inline constexpr int64_t read_int64(const uint8_t *bytes)
    {
        return static_cast<int64_t>(read_uint64(bytes));
    }

    // ========================================================================
    // FLOATING POINT READERS
    // ========================================================================

    /**
     * @brief Reads a 32-bit float from a Big-Endian byte stream.
     */
    inline float read_float(const uint8_t *bytes)
    {
        // Reuse the unsigned reader to safely reconstruct the bits
        uint32_t value = read_uint32(bytes);
        float host_float;

        // Safely copy the reconstructed bits into the float
        memcpy(&host_float, &value, sizeof(float));
        return host_float;
    }

    /**
     * @brief Reads a 64-bit double from a Big-Endian byte stream.
     */
    inline double read_double(const uint8_t *bytes)
    {
        // Reuse the unsigned reader to safely reconstruct the bits
        uint64_t value = read_uint64(bytes);
        double host_double;

        // Safely copy the reconstructed bits into the double
        memcpy(&host_double, &value, sizeof(double));
        return host_double;
    }
}

#endif