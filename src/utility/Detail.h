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
         * @brief Internal URI registry for mapping paths to callbacks.
         *
         * This class is used internally to manage the mapping between
         * URI paths and their associated callback functions.
         */
        class UriRegistry
        {
        private:
            // Array of pointers to constant URI paths.
            // Note that these are just pointers; the actual null terminated strings must
            // exist elsewhere, normally as a constant.
            const char *_path[COAP_MAX_CALLBACKS];
            // Array of callback functions associated with each path.
            Callback _callback[COAP_MAX_CALLBACKS];
            // Counter of registered URIs.
            size_t _count;

        public:
            UriRegistry() : _count(0) {}
            /**
             * @brief Add a new URI path and its associated callback.
             * @param path The URI path to register with no leading slash and no other special characters.
             *             Examples are:
             *             - `test`
             *             - `sensors/temp`
             *             If the path already exists, the callback is updated.
             *             Paths are *case-sensitive*.
             * @warning A path that does not respect the format won't work.
             * @warning The path string must remain valid for the entire lifetime
             *          of the registry. It is recommended to use constant strings.
             * @param callback The callback function associated with the path.
             * @return An error code indicating success or failure.
             *         It returns @ref ErrorCode::None on success.
             *         It returns @ref ErrorCode::NotSupported if the registry is full.
             *         Increase @ref COAP_MAX_CALLBACKS to allow more callbacks.
             */
            ErrorCode add(const char *path, Callback callback);
            /**
             * @brief Find a callback for the given URI path.
             *
             * @param path The URI path to search for. Note that a path == "" is valid.
             * @param[out] callback Output parameter to store the found callback.
             * @return @ref ErrorCode::None if found, @ref ErrorCode::NotFound if not found.
             *         It may return other error codes.
             */
            ErrorCode find(const char *path, Callback &callback) const;
        };
    }
}
#endif // DETAIL_H_INCLUDED