/**
 * @file helpers.h
 *
 * @brief This header file contains utility helper functions.
 *
 * This software is released under the MIT License.
 * Copyright (c) 2026 Pasquale Lafiosca
 */
#ifndef HELPERS_H_INCLUDED
#define HELPERS_H_INCLUDED

// Require ProsecCoAP.h to be included first.
#ifndef __PROSECCOAP_H__
#error "ProsecCoAP.h must be included before helpers.h"
#endif

namespace helpers
{
    /**
     * @brief Encodes a value into 0, 1, 2, or 3 bytes for CoAP option representation.
     *
     * @param value The integer value to encode. Note that only the lower 24 bits are considered.
     * @param out A pointer to a buffer where the encoded bytes will be stored.
     * @return The number of bytes used for the encoding (0 to 3).
     * */
    uint8_t encodeUintOption(uint32_t value, uint8_t out[3]);

    /**
     * @brief Compares two URI paths for equality.
     *
     * @param a First URI path.
     * @param b Second URI path.
     *
     * @return true if both paths are equal, false otherwise.
     *         If *either* path is NULL, returns false.
     */
    bool pathEquals(const char *a, const char *b);

    /**
     * @brief Compares two tokens for equality.
     *
     * @param a First token.
     * @param aLength Length of the first token.
     * @param b Second token.
     * @param bLength Length of the second token.
     *
     * @return true if both tokens are equal in length and content, false otherwise.
     *         If lengths differ, returns false. If both lengths are zero, returns true.
     *         If *either* token is NULL (and length > 0), returns false.
     */
    bool tokenEquals(const uint8_t *a, uint8_t aLength, const uint8_t *b, uint8_t bLength);
} // namespace detail

#endif // HELPERS_H_INCLUDED