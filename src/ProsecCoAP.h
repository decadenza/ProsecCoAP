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
 #ifndef COAP_MAX_MESSAGE_SIZE
/**
 * @brief Maximum size of a CoAP message in bytes.
 *
 * See https://datatracker.ietf.org/doc/html/rfc7252#section-4.6
 */
#define COAP_MAX_MESSAGE_SIZE 1280
#endif
#ifndef COAP_MAX_CALLBACK
/**
 * @brief Maximum number of callbacks that can be registered.
 */
#define COAP_MAX_CALLBACK 10
#endif
#ifndef COAP_MAX_OBSERVERS
/**
 * @brief Maximum number of _observers that can be registered at runtime.
 */
#define COAP_MAX_OBSERVERS 4
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
#define COAP_MAX_RETRANSMIT 4
#endif
#ifndef COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE
/**
 * @brief The maximum number of confirmable messages that are tracked for retransmission.
 * This value can be overridden.
 */
#define COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE 4
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
#define COAP_HEADER_SIZE 4u
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
 * @brief Helper to use response codes as defined in RFC 7252.
 */
#define COAP_CODE_ENCODE(class, detail) ((class << 5) | (detail))
/**
 * @brief The maximum length of a CoAP token.
 *
 * See also https://datatracker.ietf.org/doc/html/rfc7252#section-5.3.1.
 */
#define COAP_MAX_TOKEN_LENGTH 8U
/**
 * @brief The CoAP message type.
 *
 * It consists of 2 bits used to represent the message type in the CoAP header.
 * See https://datatracker.ietf.org/doc/html/rfc7252#section-3
 */
typedef enum
{
    /** Confirmable message */
    COAP_CON = 0,
    /** Non-confirmable message */
    COAP_NONCON = 1,
    /** Acknowledgement message */
    COAP_ACK = 2,
    /** Reset message */
    COAP_RESET = 3
} COAP_TYPE;

/**
 * @brief The CoAP code.
 *
 * The code is defined in Section 3 of RFC 7252 as an 8-bit unsigned integer,
 * split into a 3-bit class (most significant bits) and a 5-bit detail (least significant bits).
 * The class 0 indicates a request, classes 2.xx, 4.xx, and 5.xx indicate responses.
 *
 * The list of allowed codes is specified in the CoAP Code Registries
 * (https://datatracker.ietf.org/doc/html/rfc7252#section-12.1).
 */
typedef enum
{
    /** Empty message */
    COAP_EMPTY = COAP_CODE_ENCODE(0, 0),

    // SECTION 0.xx Request Codes
    COAP_GET = COAP_CODE_ENCODE(0, 1),
    COAP_POST = COAP_CODE_ENCODE(0, 2),
    COAP_PUT = COAP_CODE_ENCODE(0, 3),
    COAP_DELETE = COAP_CODE_ENCODE(0, 4)
    // !SECTION End of 0.xx Request Codes

    // SECTION 2.xx Success response codes
    /** Like HTTP 201 "Created", only used in response to POST and PUT requests. */
    COAP_CREATED = COAP_CODE_ENCODE(2, 1),
    /** Like HTTP 204 "No Content", only used in response to DELETE or POST requests that cause the resource to cease being available. */
    COAP_DELETED = COAP_CODE_ENCODE(2, 2),
    /** Related to HTTP 304 "Not Modified", indicates the response identified by the entity-tag is valid. */
    COAP_VALID = COAP_CODE_ENCODE(2, 3),
    /** Like HTTP 204 "No Content", only used in response to POST and PUT requests. */
    COAP_CHANGED = COAP_CODE_ENCODE(2, 4),
    /** Like HTTP 200 "OK", only used in response to GET requests. */
    COAP_CONTENT = COAP_CODE_ENCODE(2, 5),
    // !SECTION End of 2.xx Success response codes

    // SECTION 4.xx Client Error response codes
    /** Like HTTP 400 "Bad Request". */
    COAP_BAD_REQUEST = COAP_CODE_ENCODE(4, 0),
    /** The client is not authorized to perform the requested action. */
    COAP_UNAUTHORIZED = COAP_CODE_ENCODE(4, 1),
    /** The request could not be understood due to one or more unrecognized or malformed options. */
    COAP_BAD_OPTION = COAP_CODE_ENCODE(4, 2),
    /** Like HTTP 403 "Forbidden". */
    COAP_FORBIDDEN = COAP_CODE_ENCODE(4, 3),
    /** Like HTTP 404 "Not Found". */
    COAP_NOT_FOUND = COAP_CODE_ENCODE(4, 4),
    /** Like HTTP 405 "Method Not Allowed" but with no parallel to the "Allow" header field. */
    COAP_METHOD_NOT_ALLOWED = COAP_CODE_ENCODE(4, 5),
    /** Like HTTP 406 "Not Acceptable", but with no response entity. */
    COAP_NOT_ACCEPTABLE = COAP_CODE_ENCODE(4, 6),
    /** Like HTTP 412 "Precondition Failed". */
    COAP_PRECONDITION_FAILED = COAP_CODE_ENCODE(4, 12),
    /** Like HTTP 413 "Request Entity Too Large". */
    COAP_REQUEST_ENTITY_TOO_LARGE = COAP_CODE_ENCODE(4, 13),
    /** Like HTTP 415 "Unsupported Media Type". */
    COAP_UNSUPPORTED_CONTENT_FORMAT = COAP_CODE_ENCODE(4, 15),
    // !SECTION End of 4.xx Client Error response codes

    // SECTION 5.xx Server Error response codes
    /** Like HTTP 500 "Internal Server Error". */
    COAP_INTERNAL_SERVER_ERROR = COAP_CODE_ENCODE(5, 0),
    /** Like HTTP 501 "Not Implemented". */
    COAP_NOT_IMPLEMENTED = COAP_CODE_ENCODE(5, 1),
    /** Like HTTP 502 "Bad Gateway". */
    COAP_BAD_GATEWAY = COAP_CODE_ENCODE(5, 2),
    /** Like HTTP 503 "Service Unavailable" but uses Max-Age Option instead of "Retry-After" header. */
    COAP_SERVICE_UNAVAILABLE = COAP_CODE_ENCODE(5, 3),
    /** Like HTTP 504 "Gateway Timeout". */
    COAP_GATEWAY_TIMEOUT = COAP_CODE_ENCODE(5, 4),
    /** The server is unable or unwilling to act as a forward-proxy for the URI specified in the Proxy-Uri Option. */
    COAP_PROXYING_NOT_SUPPORTED = COAP_CODE_ENCODE(5, 5)
    // !SECTION End of 5.xx Server Error response codes
} COAP_CODE;

/**
 * CoAP Option Numbers Registry
 *
 * https://datatracker.ietf.org/doc/html/rfc7252#section-12.2
 */
typedef enum
{
    COAP_IF_MATCH = 1,
    COAP_URI_HOST = 3,
    COAP_E_TAG = 4,
    COAP_IF_NONE_MATCH = 5,
    COAP_OBSERVE = 6,
    COAP_URI_PORT = 7,
    COAP_LOCATION_PATH = 8,
    COAP_URI_PATH = 11,
    COAP_CONTENT_FORMAT = 12,
    COAP_MAX_AGE = 14,
    COAP_URI_QUERY = 15,
    COAP_ACCEPT = 17,
    COAP_LOCATION_QUERY = 20,
    COAP_PROXY_URI = 35,
    COAP_PROXY_SCHEME = 39,
    COAP_SIZE1 = 60
} COAP_OPTION_NUMBER;

/**
 * @brief Coap Observe option values.
 *
 * See https://datatracker.ietf.org/doc/html/rfc7641#section-2
 */
typedef enum
{
    COAP_OBSERVE_VALUE_REGISTER = 0,
    COAP_OBSERVE_VALUE_DEREGISTER = 1
} COAP_OBSERVE_VALUE;

/**
 * @brief The CoAP content format.
 *
 * See https://datatracker.ietf.org/doc/html/rfc7252#section-5.10.3
 * and the relative *initial* registry at
 * https://datatracker.ietf.org/doc/html/rfc7252#section-12.3
 *
 * REVIEW: Expand as needed based on
 * https://www.iana.org/assignments/core-parameters/core-parameters.xhtml#content-formats
 */
typedef enum
{
    COAP_TEXT_PLAIN = 0,
    COAP_APPLICATION_LINK_FORMAT = 40,
    COAP_APPLICATION_XML = 41,
    COAP_APPLICATION_OCTET_STREAM = 42,
    COAP_APPLICATION_EXI = 47,
    COAP_APPLICATION_JSON = 50,
    COAP_APPLICATION_CBOR = 60
} COAP_CONTENT_FORMAT;

/** @} */ // End of "Other constants" group

// !SECTION End of all constants.

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
uint16_t CoapGetNextMessageId();

/**
 * @brief Generate a random token of the given length.
 *
 * @param buffer The buffer where to store the generated token.
 * @param length The length (in bytes) of the token to generate.
 *               The maximum length is @ref COAP_MAX_TOKEN_LENGTH bytes. Any greater value will be clamped.
 *
 */
void CoapGenerateRandomToken(uint8_t *buffer, size_t length);

/** @} */ // End of Functions group

/**
 * @brief A CoAP message.
 *
 * This is a view on the binary representation of a CoAP message.
 */
class CoapMessage
{

private:
    /**
     * @brief Message binary data.
     */
    uint8_t _message[COAP_MAX_MESSAGE_SIZE];
}

#endif
