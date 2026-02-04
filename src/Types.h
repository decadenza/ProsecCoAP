#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

// Include Arduino UDP library.
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
#ifndef COAP_MAX_CALLBACKS
/**
 * @brief Maximum number of callbacks that can be registered at runtime.
 *
 * This limits the number of endpoints that can be served.
 */
#define COAP_MAX_CALLBACKS 10U
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
#define COAP_ACK_MAX_TIMEOUT_MS (unsigned long)(COAP_ACK_MIN_TIMEOUT_MS * COAP_ACK_RANDOM_FACTOR)
#ifndef COAP_MAX_RETRANSMIT
/**
 * @brief The maximum number of retransmission attempts for confirmable messages.
 * Default to 4 as per RFC 7252, Section 4.8.
 * This value can be overridden.
 */
#define COAP_MAX_RETRANSMIT 4U
#endif
#ifndef COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE
/**
 * @brief The maximum number of confirmable messages that are stored for retransmission.
 *
 * The total memory used by the queue will be *about*:
 * COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE * COAP_MAX_MESSAGE_SIZE bytes.
 * The actual memory usage may be slightly higher due to the @ref Message representation.
 *
 * Reduce this value to save memory.
 */
#define COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE 2U
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
constexpr uint8_t COAP_VERSION = 0x01;
/**
 * @brief The size of the CoAP header in bytes.
 */
constexpr uint8_t COAP_HEADER_SIZE = 4;
/**
 * @brief The payload marker byte.
 */
constexpr uint8_t COAP_PAYLOAD_MARKER = 0xFF;
/**
 * @brief The default CoAP port number.
 *
 * As per RFC 7252, the default port for CoAP is 5683.
 * The the CoAP instance may define a different port.
 */
constexpr uint16_t COAP_DEFAULT_PORT = 5683;
/**
 * @brief Helper to encode class and detail into a 8-bit response code as defined in RFC 7252.
 */
constexpr uint8_t COAP_CODE_ENCODE(uint8_t class_, uint8_t detail) { return (class_ << 5) | (detail); }
/**
 * @brief The maximum length of a CoAP token.
 *
 * See also https://datatracker.ietf.org/doc/html/rfc7252#section-5.3.1.
 */
constexpr uint8_t COAP_MAX_TOKEN_LENGTH = 8;
/** @} */ // End of "Non configurable constants" group

// !SECTION End of all constants.

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
        CON = 0,
        /** Non-confirmable message */
        NON = 1,
        /** Acknowledgement message */
        ACK = 2,
        /** Reset message */
        RST = 3
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
        EMPTY = COAP_CODE_ENCODE(0, 0),

        // SECTION 0.xx Request MessageCodes
        GET = COAP_CODE_ENCODE(0, 1),
        POST = COAP_CODE_ENCODE(0, 2),
        PUT = COAP_CODE_ENCODE(0, 3),
        DELETE = COAP_CODE_ENCODE(0, 4),
        // !SECTION End of 0.xx Request MessageCodes

        // SECTION 2.xx Success response codes
        /** Like HTTP 201 "Created", only used in response to POST and PUT requests. */
        CREATED = COAP_CODE_ENCODE(2, 1),
        /** Like HTTP 204 "No Content", only used in response to DELETE or POST requests that cause the resource to cease being available. */
        DELETED = COAP_CODE_ENCODE(2, 2),
        /** Related to HTTP 304 "Not Modified", indicates the response identified by the entity-tag is valid. */
        VALID = COAP_CODE_ENCODE(2, 3),
        /** Like HTTP 204 "No Content", only used in response to POST and PUT requests. */
        CHANGED = COAP_CODE_ENCODE(2, 4),
        /** Like HTTP 200 "OK", only used in response to GET requests. */
        CONTENT = COAP_CODE_ENCODE(2, 5),
        // !SECTION End of 2.xx Success response codes

        // SECTION 4.xx Client Error response codes
        /** Like HTTP 400 "Bad Request". */
        BAD_REQUEST = COAP_CODE_ENCODE(4, 0),
        /** The client is not authorized to perform the requested action. */
        UNAUTHORIZED = COAP_CODE_ENCODE(4, 1),
        /** The request could not be understood due to one or more unrecognized or malformed options. */
        BAD_OPTION = COAP_CODE_ENCODE(4, 2),
        /** Like HTTP 403 "Forbidden". */
        FORBIDDEN = COAP_CODE_ENCODE(4, 3),
        /** Like HTTP 404 "Not Found". */
        NOT_FOUND = COAP_CODE_ENCODE(4, 4),
        /** Like HTTP 405 "Method Not Allowed" but with no parallel to the "Allow" header field. */
        METHOD_NOT_ALLOWED = COAP_CODE_ENCODE(4, 5),
        /** Like HTTP 406 "Not Acceptable", but with no response entity. */
        NOT_ACCEPTABLE = COAP_CODE_ENCODE(4, 6),
        /** Like HTTP 412 "Precondition Failed". */
        PRECONDITION_FAILED = COAP_CODE_ENCODE(4, 12),
        /** Like HTTP 413 "Request Entity Too Large". */
        REQUEST_ENTITY_TOO_LARGE = COAP_CODE_ENCODE(4, 13),
        /** Like HTTP 415 "Unsupported Media MessageType". */
        UNSUPPORTED_CONTENT_FORMAT = COAP_CODE_ENCODE(4, 15),
        // !SECTION End of 4.xx Client Error response codes

        // SECTION 5.xx Server Error response codes
        /** Like HTTP 500 "Internal Server Error". */
        INTERNAL_SERVER_ERROR = COAP_CODE_ENCODE(5, 0),
        /** Like HTTP 501 "Not Implemented". */
        NOT_IMPLEMENTED = COAP_CODE_ENCODE(5, 1),
        /** Like HTTP 502 "Bad Gateway". */
        BAD_GATEWAY = COAP_CODE_ENCODE(5, 2),
        /** Like HTTP 503 "Service Unavailable" but uses Max-Age Option instead of "Retry-After" header. */
        SERVICE_UNAVAILABLE = COAP_CODE_ENCODE(5, 3),
        /** Like HTTP 504 "Gateway Timeout". */
        GATEWAY_TIMEOUT = COAP_CODE_ENCODE(5, 4),
        /** The server is unable or unwilling to act as a forward-proxy for the URI specified in the Proxy-Uri Option. */
        PROXYING_NOT_SUPPORTED = COAP_CODE_ENCODE(5, 5),
        // !SECTION End of 5.xx Server Error response codes
    };

    /**
     * CoAP Option Numbers Registry
     *
     * https://datatracker.ietf.org/doc/html/rfc7252#section-12.2
     */
    enum class OptionNumber : uint16_t
    {
        IF_MATCH = 1,
        URI_HOST = 3,
        E_TAG = 4,
        IF_NONE_MATCH = 5,
        OBSERVE = 6,
        URI_PORT = 7,
        LOCATION_PATH = 8,
        URI_PATH = 11,
        CONTENT_FORMAT = 12,
        MAX_AGE = 14,
        URI_QUERY = 15,
        ACCEPT = 17,
        LOCATION_QUERY = 20,
        PROXY_URI = 35,
        PROXY_SCHEME = 39,
        SIZE1 = 60
    };

    /**
     * @brief Coap Observe option values.
     *
     * See https://datatracker.ietf.org/doc/html/rfc7641#section-2
     */
    enum class ObserveValue : uint8_t
    {
        REGISTER = 0,
        DEREGISTER = 1
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
        TEXT_PLAIN = 0,
        APPLICATION_LINK_FORMAT = 40,
        APPLICATION_XML = 41,
        APPLICATION_OCTET_STREAM = 42,
        APPLICATION_EXI = 47,
        APPLICATION_JSON = 50,
        APPLICATION_CBOR = 60
    };

    /**
     * @brief Error codes used in the library.
     *
     * Negative values indicate errors.
     */
    enum class ErrorCode : int8_t
    {
        /** No error */
        NONE = 0,
        /** The requested resource was not found. */
        NOT_FOUND = -1,
        /**
         * The message is too large to fit in the allocated buffer.
         *
         * The operation could not be performed because the message size will exceed @ref COAP_MAX_MESSAGE_SIZE.
         */
        MESSAGE_TOO_LARGE = -2,
        /** The message is malformed. */
        MALFORMED_MESSAGE = -3,
        /** One (or more) of the supplied arguments is invalid. */
        INVALID_ARGUMENT = -4,
        /** The operation is not supported. */
        NOT_SUPPORTED = -5,
        /** A network error occurred. */
        NETWORK = -6,
        /** General unexpected failure. */
        UNEXPECTED = -99
    };

    // !SECTION End of Enums.

    class Message; // Forward declaration.

    /**
     * @brief Callback function type for handling incoming messages.
     *
     * @param message The received CoAP message.
     * @param ip The IP address of the sender.
     * @param port The UDP port of the sender.
     */
    typedef void (*Callback)(Message &message, IPAddress ip, uint16_t port);
}

#endif // TYPES_H_INCLUDED