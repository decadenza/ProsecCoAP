/**
 * @file Utils.h
 * @brief Utility functions for the ProsecCoAP library.
 * */
#ifndef __PROSECCOAP_UTILS_H__
#define __PROSECCOAP_UTILS_H__

#include <string.h> // For memcpy.
#include <stdint.h>
#include <stddef.h> // For size_t.

namespace Coap
{
    /**
     * @brief Utilities.
     *
     * This namespace contains public utility functions and classes.
     */
    namespace Utils
    {
        /**
         * @brief Convert an integer-like value to network byte order (i.e. big-endian).
         *
         * @param value The integer-like value to convert.
         * @param[out] result The pointer to the byte buffer where the result will be stored.
         *
         * The destination buffer must have at least sizeof(T) bytes available.
         *
         * @note Both signed and unsigned integers are supported as long as the right-shift operator
         * performs arithmetic right shift, so that the result remains negative.
         */
        template <typename T>
        inline void toNetworkByteOrder(T value, uint8_t *result)
        {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            // If the host is already big-endian, we can just memcpy the value.
            memcpy(result, &value, sizeof(T));
#else
            // The host is little endian, so we need to manually convert to big-endian.
            for (size_t i = 0; i < sizeof(T); ++i)
            {
                // Shift right by (sizeof(T) - 1 - i) * 8 bits, then mask the lowest byte.
                result[i] = static_cast<uint8_t>((value >> ((sizeof(T) - 1 - i) * 8)) & 0xFF);
            }
#endif
        }

        /**
         * @brief Overload to convert a 32-bit float to network byte order.
         *
         * @param value The float value to convert.
         * @param[out] result The pointer to the byte buffer where the result will be stored (min 4 bytes).
         */
        inline void toNetworkByteOrder(float value, uint8_t *result)
        {
            uint32_t int_value;
            memcpy(&int_value, &value, sizeof(float));
            toNetworkByteOrder(int_value, result);
        }

        /**
         * @brief Overload to convert a 64-bit double to network byte order.
         *
         * @param value The double value to convert.
         * @param[out] result The pointer to the byte buffer where the result will be stored (min 8 bytes).
         */
        inline void toNetworkByteOrder(double value, uint8_t *result)
        {
            uint64_t int_value;
            memcpy(&int_value, &value, sizeof(double));
            toNetworkByteOrder(int_value, result);
        }

        // NOTE: The host byte order (big/little endian) does not matter, as the bit shift operator
        // will adjust accordingly.
        // See https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html

        /**
         * @brief Reads a 16-bit unsigned integer from a Big-Endian byte stream.
         *
         * Basic usage:
         * @code{.cpp}
         * uint8_t stream[2] = {0xAB, 0xCD};
         * uint16_t value = Coap::Utils::read_uint16(stream);
         * @endcode
         *
         * Usage with larger buffers:
         * @code{.cpp}
         * uint8_t stream[100] = {0}; // Large source data (must be populated).
         * uint16_t value = Coap::Utils::read_uint16(&stream[42]);
         * @endcode
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

        /**
         * @brief Reads a 16-bit signed integer from a Big-Endian byte stream.
         */
        inline constexpr int16_t read_int16(const uint8_t *bytes)
        {
            // Read as unsigned to avoid bitshift undefined behaviour, then cast to signed.
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

        /**
         * @brief Reads a 32-bit float from a Big-Endian byte stream.
         */
        inline float read_float(const uint8_t *bytes)
        {
            uint32_t value = read_uint32(bytes);
            float host_float;
            memcpy(&host_float, &value, sizeof(float));
            return host_float;
        }

        /**
         * @brief Reads a 64-bit double from a Big-Endian byte stream.
         *
         * @note Templated to defer the 64-bit architecture check until the function is actually called.
         */
        template <typename T = double>
        inline T read_double(const uint8_t *bytes)
        {
            static_assert(sizeof(T) == 8, "Cannot read a 64-bit network double on a 32-bit double architecture (like AVR).");
            uint64_t value = read_uint64(bytes);
            double host_double;
            memcpy(&host_double, &value, sizeof(double));
            return host_double;
        }
    }
}

#endif