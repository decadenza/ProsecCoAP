/**
 * @file Detail.h
 *
 * @brief This header file contains detail functions, not meant for public use.
 *
 * This software is released under the MIT License.
 * Copyright (c) 2026 Pasquale Lafiosca
 */
#ifndef DETAIL_H_INCLUDED
#define DETAIL_H_INCLUDED

// Include common types.
#include "../Types.h"

namespace Coap
{

    /**
     * @brief Internal details of the library. Not for public use.
     */
    namespace Detail
    {
        /**
         * @brief Get a random timeout value for retransmissions.
         *
         * Returns a random timeout value between COAP_ACK_MIN_TIMEOUT_MS and COAP_ACK_MAX_TIMEOUT_MS
         * as per RFC 7252, Section 4.8.
         *
         * @return A random timeout value in milliseconds.
         */
        unsigned long getRandomTimeout();

        /**
         * @brief Return the minimum number of bytes needed to represent the given value.
         *
         * Note that 0 is represented with 0 bytes, as per CoAP option encoding rules.
         * @see https://datatracker.ietf.org/doc/html/rfc7252#section-3.2
         *
         * @param value The value to evaluate.
         * @return The minimum number of bytes required to represent the value.
         */
        size_t getMinOptionBytes(uint32_t value);

        /**
         * @brief Wrapper around low level function to send raw UDP data.
         *
         * @param udp The UDP instance to use for sending data.
         * @param data Pointer to the data to send.
         * @param length Length of the data in bytes.
         * @param ip Destination IP address.
         * @param port Destination port number.
         * @return An error code indicating success or failure.
         */
        ErrorCode sendUdp(UDP *udp, const uint8_t *data, size_t length, IPAddress ip, uint16_t port);

        /**
         * @brief Internal URI registry for mapping paths to callbacks.
         *
         * This class is used internally to manage the mapping between
         * URI paths and their associated callback functions.
         *
         * The registry is set at setup time and is not meant to be modified at runtime.
         */
        class UriRegistry
        {
        private:
            /**
             * @brief Array of pointers to constant URI paths.
             *
             * @note These are just pointers; the actual null terminated strings must
             * exist elsewhere, normally as a constant.
             */
            const char *_path[COAP_MAX_CALLBACKS];
            /**
             * @brief Array of callback functions associated to each path.
             */
            Callback _callback[COAP_MAX_CALLBACKS];
            /**
             * @brief Counter of registered URIs.
             *
             * @note This counter saves us from iterating through the entire arrays when searching for a path,
             * as well as indicating the next available slot in the arrays.
             */
            size_t _count;

        public:
            UriRegistry() : _count(0) {}
            /**
             * @brief Add a new URI path and its associated callback.
             *
             * @param path The URI path to serve with **no leading slash**,
             *             **no trailing slash** and no other special characters.
             *             If the path already exists, the callback is updated.
             *             Paths are *case-sensitive.
             *             Examples of valid paths are:
             *                 - `test`
             *                 - `sensors/temp`
             *
             *
             * @param callback The callback function associated with the path.
             * @return An error code indicating success or failure.
             *         It returns @ref ErrorCode::OK on success.
             *         It returns @ref ErrorCode::NOT_SUPPORTED if the registry is full.
             *         Increase @ref COAP_MAX_CALLBACKS to allow more callbacks.
             *
             * @warning The path string pointer must remain valid for the entire lifetime
             *          of the registry. It is recommended to use constant strings.
             */
            ErrorCode add(const char *path, Callback callback);
            /**
             * @brief Find the callback for the given URI path.
             *
             * It looks for an exact match of the path, case sensitive.
             *
             * @param path The URI path to search for. Note that a path == "" is valid.
             * @param[out] callback Output parameter to store the found callback.
             * @return @ref ErrorCode::OK if found, @ref ErrorCode::NOT_FOUND if not found.
             *         It may return other error codes.
             *
             * @note No delete function is provided, as the registry is meant to be set
             *       at setup time and not modified at runtime.
             */
            ErrorCode find(const char *path, Callback &callback) const;
        };

    }
}
#endif // DETAIL_H_INCLUDED