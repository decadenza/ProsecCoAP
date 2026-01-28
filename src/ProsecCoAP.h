/*
CoAP library for Arduino with Observe functionality.

This software is released under the MIT License.
Copyright (c) 2014 Hirotaka Niisato
Copyright (c) 2026 Pasquale Lafiosca

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef __PROSECCOAP_H__
#define __PROSECCOAP_H__

#include "Udp.h"

// SECTION Constants.
/**
 * @defgroup ConfigurableConstants
 * @brief CoAP constants that can be overridden.
 *
 * These constants can be overridden by defining them before including this header.
 * Example:
 * @code{.cpp}
 * #define COAP_MAX_OBSERVERS 8
 * #include <ProsecCoAP.h>
 * @endcode
 *
 * Refer to each constant's documentation for details.
 *@{
 */
#ifndef COAP_MAX_MESSAGE_SIZE
/**
 * @brief Maximum size of a CoAP message in bytes.
 *
 * As per CoAP specification, the recommended message size should fit within a single
 * IP packet to avoid fragmentation. The recommended maximum size is therefore 1152 bytes.
 * The absolute minimum size for a UDP payload (and thus, of a message) is 40 bytes.
 *
 * This value applies to both incoming and outgoing messages.
 * Keep this value small enough to reduce memory usage. Exceeding it will
 * lead to errors when building outgoing messages or parsing incoming messages.
 *
 * See https://datatracker.ietf.org/doc/html/rfc7252#section-4.6
 *
 * # Further considerations on the message size
 * The message structure is:
 * | HEADER  | TOKEN | OPTIONS  | PAYLOAD  |
 * | :------ | :---- | :------- | :------- |
 * | 4 bytes | 0-8b  | variable | variable |
 *
 */
#define COAP_MAX_MESSAGE_SIZE 128U
#endif
#ifndef COAP_MAX_CALLBACK
/**
 * @brief Maximum number of callbacks that can be registered.
 */
#define COAP_MAX_CALLBACK 10U
#endif
#ifndef COAP_MAX_OBSERVERS
/**
 * @brief Maximum number of observers that can be registered at runtime.
 */
#define COAP_MAX_OBSERVERS 4U
#endif
#ifndef COAP_OBSERVER_LEASE_MS
#define COAP_OBSERVER_LEASE_MS 60000UL
#endif
#ifndef COAP_ACK_MIN_TIMEOUT_MS
/**
 * @brief Minimum ACK timeout in milliseconds. See RFC 7252, Section 4.8.
 * Default to 2 seconds (2000 ms).
 *
 * This value can be overridden.
 */
#define COAP_ACK_MIN_TIMEOUT_MS 2000UL
#endif
#ifndef COAP_ACK_RANDOM_FACTOR
/**
 * @brief ACK timeout random factor as per RFC 7252, Section 4.8.
 * Default to 1.5.
 * This value can be overridden.
 */
#define COAP_ACK_RANDOM_FACTOR 1.5f
#endif
/**
 * @brief The precomputed maximum ACK timeout derived from the minimum timeout and the random factor.
 */
#define COAP_ACK_MAX_TIMEOUT_MS (uint32_t)(COAP_ACK_MIN_TIMEOUT_MS * COAP_ACK_RANDOM_FACTOR)
#ifndef COAP_MAX_RETRANSMIT
/**
 * @brief The maximum number of retransmission attempts for confirmable messages.
 * Default to 4 as per RFC 7252, Section 4.8.
 * This value can be overridden.
 */
#define COAP_MAX_RETRANSMIT 4U
#endif
#ifndef COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE
/**
 * @brief The maximum number of confirmable messages that are tracked for retransmission.
 * This value can be overridden.
 */
#define COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE 4U
#endif
/** @} */ // End of "Configurable constants" group

/**
 * @defgroup NonConfigurableConstants
 * @brief CoAP constants that cannot be overridden.
 *
 * These are non-overridable constants. Either defined by the CoAP specification
 * or derived from it.
 *
 * Refer to each constant's documentation for details.
 *
 * @{
 */
/**
 * @brief The CoAP version.
 *
 * CoAP version as per RFC 7252.
 */
#define COAP_VERSION 0x01
/**
 * @brief The size of the CoAP header in bytes.
 */
#define COAP_HEADER_SIZE 4U
/**
 * @brief The payload marker byte.
 */
#define COAP_PAYLOAD_MARKER 0xFF
/**
 * @brief The default CoAP port number.
 *
 * As per RFC 7252, the default port for CoAP is 5683.
 * The the CoAP instance may define a different port.
 */
#define COAP_DEFAULT_PORT 5683
/**
 * @brief Helper to encode class and detail into a 8-bit response code as defined in RFC 7252.
 */
#define COAP_CODE_ENCODE(class, detail) ((class << 5) | (detail))
/**
 * @brief The maximum length of a CoAP token.
 *
 * See also https://datatracker.ietf.org/doc/html/rfc7252#section-5.3.1.
 */
#define COAP_MAX_TOKEN_LENGTH 8U
/** @} */ // End of "Non configurable constants" group

// !SECTION End of all constants.

/**
 * @namespace Coap
 * @brief Namespace for the library.
 *
 * All library enums, classes and functions are defined within this namespace.
 */
namespace Coap
{
    // SECTION Enums.
    /**
     * @brief The CoAP message type.
     *
     * It consists of 2 bits used to represent the message type in the CoAP header.
     * See https://datatracker.ietf.org/doc/html/rfc7252#section-3
     */
    enum class MessageType : uint8_t
    {
        /** Confirmable message */
        Con = 0,
        /** Non-confirmable message */
        NonCon = 1,
        /** Acknowledgement message */
        Ack = 2,
        /** Reset message */
        Reset = 3
    };

    /**
     * @brief The CoAP code.
     *
     * The code is defined in Section 3 of RFC 7252 as an 8-bit unsigned integer,
     * split into a 3-bit class (most significant bits) and a 5-bit detail (least significant bits).
     * The class 0 indicates a request, classes 2.xx, 4.xx, and 5.xx indicate responses.
     *
     * The list of allowed codes is specified in the CoAP MessageCode Registries
     * (https://datatracker.ietf.org/doc/html/rfc7252#section-12.1).
     */
    enum class MessageCode : uint8_t
    {
        /** Empty message */
        Empty = COAP_CODE_ENCODE(0, 0),

        // SECTION 0.xx Request MessageCodes
        Get = COAP_CODE_ENCODE(0, 1),
        Post = COAP_CODE_ENCODE(0, 2),
        Put = COAP_CODE_ENCODE(0, 3),
        Delete = COAP_CODE_ENCODE(0, 4),
        // !SECTION End of 0.xx Request MessageCodes

        // SECTION 2.xx Success response codes
        /** Like HTTP 201 "Created", only used in response to POST and PUT requests. */
        Created = COAP_CODE_ENCODE(2, 1),
        /** Like HTTP 204 "No Content", only used in response to DELETE or POST requests that cause the resource to cease being available. */
        Deleted = COAP_CODE_ENCODE(2, 2),
        /** Related to HTTP 304 "Not Modified", indicates the response identified by the entity-tag is valid. */
        Valid = COAP_CODE_ENCODE(2, 3),
        /** Like HTTP 204 "No Content", only used in response to POST and PUT requests. */
        Changed = COAP_CODE_ENCODE(2, 4),
        /** Like HTTP 200 "OK", only used in response to GET requests. */
        Content = COAP_CODE_ENCODE(2, 5),
        // !SECTION End of 2.xx Success response codes

        // SECTION 4.xx Client Error response codes
        /** Like HTTP 400 "Bad Request". */
        BadRequest = COAP_CODE_ENCODE(4, 0),
        /** The client is not authorized to perform the requested action. */
        Unauthorized = COAP_CODE_ENCODE(4, 1),
        /** The request could not be understood due to one or more unrecognized or malformed options. */
        BadOption = COAP_CODE_ENCODE(4, 2),
        /** Like HTTP 403 "Forbidden". */
        Forbidden = COAP_CODE_ENCODE(4, 3),
        /** Like HTTP 404 "Not Found". */
        NotFound = COAP_CODE_ENCODE(4, 4),
        /** Like HTTP 405 "Method Not Allowed" but with no parallel to the "Allow" header field. */
        MethodNotAllowed = COAP_CODE_ENCODE(4, 5),
        /** Like HTTP 406 "Not Acceptable", but with no response entity. */
        NotAcceptable = COAP_CODE_ENCODE(4, 6),
        /** Like HTTP 412 "Precondition Failed". */
        PreconditionFailed = COAP_CODE_ENCODE(4, 12),
        /** Like HTTP 413 "Request Entity Too Large". */
        RequestEntityTooLarge = COAP_CODE_ENCODE(4, 13),
        /** Like HTTP 415 "Unsupported Media MessageType". */
        UnsupportedContentFormat = COAP_CODE_ENCODE(4, 15),
        // !SECTION End of 4.xx Client Error response codes

        // SECTION 5.xx Server Error response codes
        /** Like HTTP 500 "Internal Server Error". */
        InternalServerError = COAP_CODE_ENCODE(5, 0),
        /** Like HTTP 501 "Not Implemented". */
        NotImplemented = COAP_CODE_ENCODE(5, 1),
        /** Like HTTP 502 "Bad Gateway". */
        BadGateway = COAP_CODE_ENCODE(5, 2),
        /** Like HTTP 503 "Service Unavailable" but uses Max-Age Option instead of "Retry-After" header. */
        ServiceUnavailable = COAP_CODE_ENCODE(5, 3),
        /** Like HTTP 504 "Gateway Timeout". */
        GatewayTimeout = COAP_CODE_ENCODE(5, 4),
        /** The server is unable or unwilling to act as a forward-proxy for the URI specified in the Proxy-Uri Option. */
        ProxyingNotSupported = COAP_CODE_ENCODE(5, 5),
        // !SECTION End of 5.xx Server Error response codes
    };

    /**
     * CoAP Option Numbers Registry
     *
     * https://datatracker.ietf.org/doc/html/rfc7252#section-12.2
     */
    enum class OptionNumber : uint16_t
    {
        IfMatch = 1,
        UriHost = 3,
        ETag = 4,
        IfNoneMatch = 5,
        Observe = 6,
        UriPort = 7,
        LocationPath = 8,
        UriPath = 11,
        ContentFormat = 12,
        MaxAge = 14,
        UriQuery = 15,
        Accept = 17,
        LocationQuery = 20,
        ProxyUri = 35,
        ProxyScheme = 39,
        Size1 = 60
    };

    /**
     * @brief Coap Observe option values.
     *
     * See https://datatracker.ietf.org/doc/html/rfc7641#section-2
     */
    enum class ObserveValue : uint8_t
    {
        Register = 0,
        Deregister = 1
    };
    /**
     * @brief The CoAP content format.
     *
     * The numeric identifier can be in the range 0-65535.
     * See https://datatracker.ietf.org/doc/html/rfc7252#section-5.10.3
     * and the relative *initial* registry at
     * https://datatracker.ietf.org/doc/html/rfc7252#section-12.3
     *
     * REVIEW: Expand as needed based on
     * https://www.iana.org/assignments/core-parameters/core-parameters.xhtml#content-formats
     */
    enum class ContentFormat : uint16_t
    {
        TextPlain = 0,
        ApplicationLinkFormat = 40,
        ApplicationXml = 41,
        ApplicationOctetStream = 42,
        ApplicationExi = 47,
        ApplicationJson = 50,
        ApplicationCbor = 60
    };

    /**
     * @brief Error codes used in the library.
     *
     * Negative values indicate errors.
     */
    enum class ErrorCode : int8_t
    {
        /** No error */
        None = 0,
        /** The requested resource was not found. */
        NotFound = -1,
        /**
         * The message is too large to fit in the allocated buffer.
         *
         * The operation could not be performed because the message size will exceed @ref COAP_MAX_MESSAGE_SIZE.
         */
        MessageTooLarge = -2,
        /** The message is malformed. */
        MalformedMessage = -3,
        /** The option is invalid. */
        InvalidOption = -4,
        /** The operation is not supported. */
        NotSupported = -5,
        /** General failure. */
        Failure = -99
    };

    // !SECTION End of Enums.

    // SECTION Functions.
    /**
     * @defgroup Functions
     * @brief CoAP helper functions.
     * @{
     */

    /**
     * @brief Get the next Message ID.
     *
     * Message ID are sequentially assigned, starting from a random value.
     * The role of the Message ID is only to detect duplicates.
     *
     * @return A 16-bit message ID.
     */
    uint16_t getNextMessageId();

    /**
     * @brief Generate a random token of the given length.
     *
     * @param[out] buffer The destination buffer of the generated token.
     * @param length The length (in bytes) of the token to generate.
     *               The maximum length is @ref COAP_MAX_TOKEN_LENGTH bytes. Any greater value will be clamped.
     *
     */
    void generateRandomToken(uint8_t *buffer, size_t length);

    /** @} */ // End of Functions group
    // !SECTION End of Functions.

    /**
     * @brief A CoAP message.
     *
     * This is a view on the binary representation of a CoAP message.
     *
     * See https://datatracker.ietf.org/doc/html/rfc7252#section-3
     */
    class Message
    {
    private:
        /**
         * @brief Message binary data.
         */
        uint8_t _message[COAP_MAX_MESSAGE_SIZE];

    public:
        /**
         * @brief Builds the CoAP message with default values.
         *
         * The version is set to @ref COAP_VERSION.
         * The type is set to @ref COAP_NONCON.
         * The token length is set to 0.
         * The code is set to @ref COAP_EMPTY.
         * The message ID is set to a new value from @ref getNextMessageId
         */
        Message();
        /**
         * @brief Set the message type.
         *
         * @param type The message type to set.
         */
        void setMessageType(MessageType type);
        /**
         * @brief Get the message type.
         *
         * The type is always present in a CoAP message.
         */
        MessageType getMessageType();
        /**
         * @brief Add a token of the given length to the message.
         *
         * A random token is generated and added to the message as per specifications.
         *
         * The token is an optional field in a CoAP message.
         * It is intended for use as a client-local identifier for
         * differentiating between concurrent requests.
         * The token can be max @ref COAP_MAX_TOKEN_LENGTH bytes long.
         *
         * @param length The length (in bytes) of the token.
         *               The maximum length is @ref COAP_MAX_TOKEN_LENGTH bytes. Any greater value will be clamped.
         * @return 0 on success, or a negative error code on failure.
         *
         * Example:
         * @code{.cpp}
         * CoapMessage msg;
         * const uint8_t* token = msg.addToken(4);
         * @endcode
         */
        ErrorCode addToken(size_t length);

        /**
         * @brief Get the current token from the message.
         *
         * @param[out] length Pointer to a size_t variable where the token length will be stored.
         * @return A pointer to the token within the message. The caller should not read beyond the token length.
         *
         * Example:
         * @code{.cpp}
         * Coap::Message msg;
         * size_t tokenLength;
         * const uint8_t* token = msg.getToken(&tokenLength);
         * @endcode
         */
        const uint8_t *getToken(size_t *length);
        /**
         * @brief Set the message code.
         *
         * @param code The message code to set.
         */
        void setMessageCode(MessageCode code);
        /**
         * @brief Get the message code.
         *
         * The code is always present in a CoAP message.
         */
        MessageCode getMessageCode();

        /**
         * @brief Add an option to the message.
         *
         * The option is added according to the CoAP option encoding rules.
         *
         * For options that can appear only once, the existing option is replaced.
         * For options that can appear multiple times, the option is appended to the existing ones.
         *
         * Prefer using specialized methods for common options like @ref COAP_CONTENT_FORMAT,
         * @ref COAP_URI_PATH, or @ref COAP_URI_QUERY when available.
         *
         * See https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
         *
         * @param number
         * @param value
         * @param length
         * @return
         */
        ErrorCode addOption(OptionNumber number, const uint8_t *value, size_t length);
    };

} // End of namespace Coap

#endif
