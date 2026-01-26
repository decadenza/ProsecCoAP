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

/**
 * // SECTION Constants
 * @defgroup Constants
 * @brief CoAP constants.
 *
 * Some of these constants can be overridden by defining them before including this header.
 * Refer to each constant's documentation for details.
 *
 * @{
 */
#ifndef COAP_MAX_CALLBACK
/**
 * @brief Maximum number of callbacks that can be registered.
 *
 * This value can be overridden.
 */
#define COAP_MAX_CALLBACK 10
#endif
#define COAP_HEADER_SIZE 4u
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF
#ifndef COAP_MAX_OPTION_NUM
/**
 * @brief Maximum number of options in a CoAP packet.
 *
 * This limit is applied both to incoming and outgoing packets.
 *
 * This value can be overridden.
 */
#define COAP_MAX_OPTION_NUM 10
#endif
/**
 * @brief The maximum size of the CoAP buffer used for sending and receiving packets.
 *
 * To override this value, pass the desired buffer size to the Coap constructor.
 * For example:
 * @code{.cpp}
 * Coap coap(udp, 256); // Allocate 256 bytes for CoAP buffer.
 * @endcode
 */
#define COAP_BUF_MAX_SIZE 128
#ifndef COAP_MAX_OBSERVERS
/**
 * @brief Maximum number of _observers that can be registered at runtime.
 *
 * This value can be overridden.
 */
#define COAP_MAX_OBSERVERS 4
#endif
#ifndef COAP_OBSERVER_LEASE_MS
#define COAP_OBSERVER_LEASE_MS 60000UL
#endif
#ifndef COAP_MAX_OBSERVE_ENDPOINT_LEN
/**
 * @brief Maximum length of the endpoint string for an observer.
 *
 * This value can be overridden.
 */
#define COAP_MAX_OBSERVE_ENDPOINT_LEN 32
#endif
#define COAP_DEFAULT_PORT 5683
#define COAP_RESPONSE_CODE_ENCODE(class, detail) ((class << 5) | (detail))
#define COAP_OPTION_DELTA(v, n) (v < 13 ? (*n = (0xFF & v)) : (v <= 0xFF + 13 ? (*n = 13) : (*n = 14)))
// SECTION CoAP retransmission parameters
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
#define COAP_MAX_RETRANSMIT 4
#endif
#ifndef COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE
/**
 * @brief The maximum number of confirmable messages that are tracked for retransmission.
 * This value can be overridden.
 */
#define COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE 4
#endif
// !SECTION End of CoAP retransmission parameters

/**
 * @brief Limit to the number of outgoing confirmable messages being tracked.
 */
typedef enum
{
    COAP_CON = 0,
    COAP_NONCON = 1,
    COAP_ACK = 2,
    COAP_RESET = 3
} COAP_TYPE;

typedef enum
{
    COAP_GET = 1,
    COAP_POST = 2,
    COAP_PUT = 3,
    COAP_DELETE = 4
} COAP_METHOD;

typedef enum
{
    COAP_EMPTY = COAP_RESPONSE_CODE_ENCODE(0, 0),
    COAP_CREATED = COAP_RESPONSE_CODE_ENCODE(2, 1),
    COAP_DELETED = COAP_RESPONSE_CODE_ENCODE(2, 2),
    COAP_VALID = COAP_RESPONSE_CODE_ENCODE(2, 3),
    COAP_CHANGED = COAP_RESPONSE_CODE_ENCODE(2, 4),
    COAP_CONTENT = COAP_RESPONSE_CODE_ENCODE(2, 5),

    COAP_BAD_REQUEST = COAP_RESPONSE_CODE_ENCODE(4, 0),
    COAP_UNAUTHORIZED = COAP_RESPONSE_CODE_ENCODE(4, 1),
    COAP_BAD_OPTION = COAP_RESPONSE_CODE_ENCODE(4, 2),
    COAP_FORBIDDEN = COAP_RESPONSE_CODE_ENCODE(4, 3),
    COAP_NOT_FOUND = COAP_RESPONSE_CODE_ENCODE(4, 4),
    COAP_METHOD_NOT_ALLOWED = COAP_RESPONSE_CODE_ENCODE(4, 5),
    COAP_NOT_ACCEPTABLE = COAP_RESPONSE_CODE_ENCODE(4, 6),
    COAP_PRECONDITION_FAILED = COAP_RESPONSE_CODE_ENCODE(4, 12),
    COAP_REQUEST_ENTITY_TOO_LARGE = COAP_RESPONSE_CODE_ENCODE(4, 13),
    COAP_UNSUPPORTED_CONTENT_FORMAT = COAP_RESPONSE_CODE_ENCODE(4, 15),

    COAP_INTERNAL_SERVER_ERROR = COAP_RESPONSE_CODE_ENCODE(5, 0),
    COAP_NOT_IMPLEMENTED = COAP_RESPONSE_CODE_ENCODE(5, 1),
    COAP_BAD_GATEWAY = COAP_RESPONSE_CODE_ENCODE(5, 2),
    COAP_SERVICE_UNAVAILABLE = COAP_RESPONSE_CODE_ENCODE(5, 3),
    COAP_GATEWAY_TIMEOUT = COAP_RESPONSE_CODE_ENCODE(5, 4),
    COAP_PROXYING_NOT_SUPPORTED = COAP_RESPONSE_CODE_ENCODE(5, 5)
} COAP_RESPONSE_CODE;

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

typedef enum
{
    COAP_OBSERVE_VALUE_INVALID = -2,   // Observe value invalid (fallback, out of standard).
    COAP_OBSERVE_VALUE_NOT_FOUND = -1, // Observe value not found (fallback, out of standard).
    COAP_OBSERVE_VALUE_REGISTER = 0,   // https://datatracker.ietf.org/doc/html/rfc7641#section-2
    COAP_OBSERVE_VALUE_DEREGISTER = 1
} COAP_OBSERVE_VALUE;

typedef enum
{
    COAP_NONE = -1,
    COAP_TEXT_PLAIN = 0,
    COAP_APPLICATION_LINK_FORMAT = 40,
    COAP_APPLICATION_XML = 41,
    COAP_APPLICATION_OCTET_STREAM = 42,
    COAP_APPLICATION_EXI = 47,
    COAP_APPLICATION_JSON = 50,
    COAP_APPLICATION_CBOR = 60
} COAP_CONTENT_TYPE;

/** @} */ // End of Constants group
// !SECTION End of constants.

/**
 * @defgroup Functions
 * @brief CoAP functions.
 * @{
 */

/**
 * @brief Generate a random message ID.
 * @return A random 16-bit message ID.
 */
uint16_t CoapGetRandomMessageId();

/** @} */ // End of Functions group

/**
 * @brief Represents a CoAP option.
 *
 * See also https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
 */
class CoapOption
{
public:
    /**
     * The CoAP option number.
     */
    uint8_t number;
    /**
     * The length of the option.
     */
    uint8_t length;
    /**
     * The pointer to the option value.
     */
    uint8_t *value;
};

/**
 * @brief A CoAP packet.
 */
class CoapPacket
{
public:
    uint8_t type = 0;
    uint8_t code = 0;
    const uint8_t *token = NULL;
    uint8_t tokenLength = 0;
    const void *payload = NULL;
    size_t payloadLength = 0;
    uint16_t messageId = 0;
    uint8_t optionCount = 0;
    CoapOption options[COAP_MAX_OPTION_NUM];

    /**
     * @brief Add an option to the packet.
     *
     * The option is stored and will be later parsed when sending the packet.
     *
     * @param number The option number.
     * @param length The length of the option value.
     * @param value The pointer to the option value.
     */
    void addOption(uint8_t number, uint8_t length, uint8_t *value);

    /**
     * @brief Fetch the observe value from the packet.
     *
     * @return The observe value if the observe option is present, @see COAP_OBSERVE_VALUE.
     */
    COAP_OBSERVE_VALUE getObserveValue();
};

#if defined(ESP8266) || defined(ESP32)
#include <functional>
typedef std::function<void(CoapPacket &, IPAddress, uint16_t)> CoapCallback;
#else
typedef void (*CoapCallback)(CoapPacket &, IPAddress, uint16_t);
#endif

class Coap; // Forward declaration.

/**
 * @brief A CoAP observer.
 *
 * A CoAP observer represents a client that has registered to observe a specific resource on the server.
 */
class CoapObserver
{

    friend class Coap; // Give full access to Coap class.

private:
    /**
     * @brief Whether this entry is in use.
     */
    bool _active = false;
    /**
     * @brief The IP address of the observer.
     */
    IPAddress _ip;
    /**
     * @brief The port of the observer.
     */
    uint16_t _port = 0;
    /**
     * @brief The token used by the observer.
     */
    uint8_t _token[8] = {0};
    uint8_t _tokenLength = 0;
    uint32_t _observationSequentialNumber = 0;
    unsigned long _lastSeenMs = 0; // TODO: Implement cleaning up old _observers.
    /**
     * @brief The endpoint being observed.
     */
    char _endpoint[COAP_MAX_OBSERVE_ENDPOINT_LEN] = {0};

public:
    /**
     * Remove the observer from the list of active _observers.
     *
     * An observer can only be added from the Coap class (@ref Coap::addObserver).
     * An observer can be removed either by the Coap class (@ref Coap::removeObserver) or
     * by itself (@ref CoapObserver::remove).
     *
     * @return true if the observer was removed successfully, false if the observer was already inactive.
     */
    bool remove();

    /**
     * @brief Get the last seen time in milliseconds.
     *
     * Note that this value may wrap around after a long uptime.
     */
    unsigned long getLastSeenMs();
};

/**
 * @cond INTERNAL
 * @namespace detail
 * @brief Namespace for internal CoAP functions.
 *
 * Objects in this namespace are not intended to be used directly by the user.
 *
 */
namespace detail
{

    /**
     * @brief Register for CoAP callbacks and their associated URI paths.
     */
    class CoapRegister
    {
    private:
        String _uriPaths[COAP_MAX_CALLBACK];
        CoapCallback _callbacks[COAP_MAX_CALLBACK];

    public:
        /**
         * @brief Create an empty URI callback registry.
         */
        CoapRegister();

        /**
         * @brief Register or update a callback for a URL path.
         *
         * @return 0 if callback was added successfully (new entry)
         * @return 1 if callback was updated (overwriting existing entry)
         * @return -1 if callback could not be added (no space available)
         */
        int add(CoapCallback callback, String path);

        /**
         * @brief Find a callback bound to a URI path.
         *
         * @return The callback if found, NULL otherwise.
         */
        CoapCallback find(String path);
    };

    /**
     * @brief An item in the retransmission queue.
     */
    struct CoapRetrasmissionItem
    {
        // Count of retransmission attempts done.
        // If attempts reach COAP_MAX_RETRANSMIT, the item is considered expired.
        unsigned short attempts = COAP_MAX_RETRANSMIT;
        // Next scheduled attempt deadline.
        unsigned long nextAttemptDeadline = 0;
        // The base timeout interval (randomly assigned between COAP_ACK_MIN_TIMEOUT_MS and COAP_ACK_MAX_TIMEOUT_MS).
        unsigned long timeoutInterval = 0;
        // Destination IP address.
        IPAddress ip;
        // Destination port.
        uint16_t port = 0;
        // The packet that needs to be retransmitted.
        CoapPacket packet;
    };

    /**
     * @brief Class to track outgoing confirmable messages.
     *
     * This is used by @ref Coap::loop to implement retransmission as per specifications.
     */
    class CoapRetrasmissionQueue
    {
    private:
        // Store packets for outgoing confirmable retransmissions.
        CoapRetrasmissionItem _items[COAP_MAX_CONFIRMABLE_MESSAGE_QUEUE]{}; // NOTE: Initialised items will have attempts = COAP_MAX_RETRANSMIT;
    public:
        /**
         * @brief Generate the random initial timeout between
         * COAP_ACK_MIN_TIMEOUT_MS and COAP_ACK_MAX_TIMEOUT_MS.
         *
         * @return The random timeout in milliseconds.
         */
        unsigned long getRandomTimeout();

        /**
          @brief Add a new packet to the outgoing queue.

          The packet *must* be of type COAP_CON.

          @return 0 on success, -1 in case of queue full.
        */
        int add(IPAddress ip, uint16_t port, const CoapPacket &packet);

        /**
         * @brief Reset the queue, discarding all queued messages.
         */
        void reset();

        /**
         * @brief Retransmit confirmable packets for requests that exceeded the timeout.
         *
         * @param coapInstance The running Coap instance.
         *
         * The packets that exceeded the @ref COAP_MAX_RETRANSMIT number of attempts, will be discarded.
         *
         * @return The number of packets retransmitted. 0 if none were retransmitted.
         * @return -1 if an error occurred.
         */
        int process(Coap &coapInstance);

        /**
         * @brief Mark an item as received.
         *
         * If the messageId exists, the corresponding item in the queue will be marked as received.
         *
         * @return 0 if the item was found and marked as received.
         * @return -1 if the item was not found.
         */
        int markItemAsReceived(uint16_t messageId);
    };

}
/**
 * namespace detail
 *  @endcond
 * */

/**
 * @brief The main CoAP instance.
 */
class Coap
{
    friend class detail::CoapRetrasmissionQueue; // Allow access to sendPacket.

private:
    UDP *_udp;
    detail::CoapRegister _register;
    CoapCallback _acknowledgementHandler = NULL;
    int _port;
    size_t _coapBufferSize;
    uint8_t *_txBuffer = NULL;
    uint8_t *_rxBuffer = NULL;
    detail::CoapRetrasmissionQueue _confirmableMessageQueue;

    /**
     * @brief Array of registered _observers.
     *
     * Preallocated array to hold observer entries at runtime.
     *
     * See also @ref COAP_MAX_OBSERVERS.
     */
    CoapObserver _observers[COAP_MAX_OBSERVERS];

    /**
     * @brief Send a CoAP packet to the specified IP.
     *
     * It uses the default CoAP port, see @ref COAP_DEFAULT_PORT.
     *
     * @return 0 if the packet was sent correctly.
     * @return -1 if the packet could not be sent.
     */
    int sendPacket(CoapPacket &packet, IPAddress ip);

    /**
     * @brief Send a CoAP packet to the specified IP and port.
     *
     * @return 0 if the packet was sent correctly.
     * @return -1 if the packet could not be sent.
     */
    int sendPacket(CoapPacket &packet, IPAddress ip, uint16_t port);

    /**
     * Parse the options according to specifications.
     *
     * @return 0 in case of success, -1 on error.
     *
     * See also https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
     */
    int parseOption(CoapOption *option, uint16_t *runningDelta, uint8_t **buffer, size_t bufferLength);

public:
    /**
     * @brief Construct a CoAP instance using the given UDP transport.
     *
     * @param udp The UDP transport to use for sending and receiving CoAP packets.
     * @param coapBufferSize The size of the internal CoAP buffer. Default is @see COAP_BUF_MAX_SIZE.
     */
    Coap(
        UDP &udp,
        size_t coapBufferSize = COAP_BUF_MAX_SIZE);

    /**
     * @brief Destroy the CoAP instance and free buffers.
     */
    ~Coap();

    /**
     * @brief Notify all _observers of a specific endpoint.
     *
     * This method sends a notification to all registered _observers for the given endpoint.
     * As per specifications, the notification includes the Observe option with a sequential number.
     * The notification will be a non-confirmable message (COAP_NONCON).
     *
     * @param observedEndpoint The endpoint being observed.
     * @param payload The payload to send to _observers.
     * @param payloadLength The length of the payload.
     * @param type The content type of the payload.
     *
     * @return Number of observers notified successfully.
     * @return -1 if an error occurred.
     */
    int notifyObservers(const char *observedEndpoint, const void *payload, int payloadLength, COAP_CONTENT_TYPE type);

    /**
     * @brief Add a new observer for a specific URL.
     *
     * If the observer is already registered, the existing observer is returned.
     *
     * @param observerOut Pointer to an Observer pointer that will be set to the newly added observer, or to the existing observer if already registered.
     * @param endpoint The endpoint to observe.
     * @param ip The IP address of the observer.
     * @param port The port of the observer.
     * @param token The token used by the observer.
     * @param tokenLength The length of the token.
     *
     * @return 0 if the observer was added successfully.
     *         -1 if the url is invalid.
     *         -2 if the observer table is full.
     */
    int addObserver(CoapObserver **observerOut, const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength);

    /**
     * @brief Get the current number of active observers.
     */
    unsigned int getObserverCount();

    /**
     * @brief Remove an observer for a specific endpoint.
     *
     * According to https://datatracker.ietf.org/doc/html/rfc7641#section-4.1,
     * after a GET request for deregistration, the server should send a response to the client.
     * @see sendResponse.
     */
    bool removeObserver(const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength);

    /**
     * @brief Start the server on the default port.
     */
    bool start();

    /**
     * @brief Start the server on a custom port.
     */
    bool start(uint16_t port);

    /**
     * @brief Set the unique response callback for acknowledgements.
     *
     * The response handler is invoked when an ACK message is received,
     * allowing the application to handle the acknowledgement.
     * The callback is unique for all the requests sent by this Coap instance.
     *
     * Note that transmission ACK are also received internally by the retrasmission queue.
     *
     * Responses to different requests can be differentiated by matching the message ID.
     *
     * @param handler The callback function to handle acknowledgements.
     */
    void acknowledgeWith(CoapCallback handler) { _acknowledgementHandler = handler; }

    /**
     * @brief Register a server callback for a URI path.
     *
     * @param callback The callback function to handle requests.
     * @param path The URI path to bind the callback to.
     *
     * @return 1 if the callback was updated, 0 if added successfully.
     *        -1 if callback could not be added.
     */
    int server(CoapCallback callback, String path) { return _register.add(callback, path); }

    /**
     * @brief Send a confirmable empty message.
     *
     * This may be used as CoAP ping.
     *
     * According to the protocol, an "Empty Message" is a message with a Code of 0.00;
     * neither a request nor a response. An Empty message only contains the 4-byte header.
     * An empty message is always confirmable (COAP_CON). Non confirmable empty messages cannot be sent.
     *
     * @param ip The IP address to send the message to.
     * @param port The port to send the message to.
     * @return The message ID used for the empty message.
     */
    uint16_t sendEmptyMessage(IPAddress ip, uint16_t port);

    /**
     * @brief Send a response.
     *
     * Starting from the request packet, it *converts* it into a response packet with the given response code
     * and payload data.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param requestPacket The packet to which the response corresponds.
     * @param code The response code to send.
     *        From it, the IP, port, message ID and token are inferred.
     * @param payload The pointer to the payload to send. It must correspond to the specified content type.
     * @param payloadLength The length of the payload. For empty payloads, set to 0. For string payloads, the length should not include the null terminator.
     * @param type The content type of the payload.
     *
     * @return 0 on success, -1 on failure.
     */
    int sendResponse(IPAddress ip, uint16_t port, CoapPacket &requestPacket, COAP_RESPONSE_CODE code, const void *payload, size_t payloadLength, COAP_CONTENT_TYPE type);

    /**
     * @brief Send a GET request.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    /**
     * @brief Send a confirmable GET request.
     *
     * This overload defaults to a confirmable request, matching the protocol's
     * expectation for reliability. Use the variant with the confirmable flag to
     * explicitly send a non-confirmable request when desired.
     */
    uint16_t sendGetRequest(IPAddress ip, uint16_t port, const char *endpoint);

    /**
     * @brief Send a GET request with explicit confirmable flag.
     */
    uint16_t sendGetRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable);

    /**
     * @brief Send a DELETE request.
     *
     * @see sendGetRequest.
     */
    /**
     * @brief Send a confirmable DELETE request.
     */
    uint16_t sendDeleteRequest(IPAddress ip, uint16_t port, const char *endpoint);

    /**
     * @brief Send a DELETE request with explicit confirmable flag.
     */
    uint16_t sendDeleteRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable);

    /**
     * @brief Send a confirmable PUT request.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The pointer to the payload to send.
     * @param payloadLength The length of the payload. For string payloads, the length should not include the null terminator.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t sendPutRequest(IPAddress ip, uint16_t port, const char *endpoint, const void *payload, size_t payloadLength);

    /**
     * @brief Send a PUT request with explicit confirmable flag.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The pointer to the payload to send.
     * @param payloadLength The length of the payload. For string payloads, the length should not include the null terminator.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t sendPutRequest(IPAddress ip, uint16_t port, const char *endpoint, const void *payload, size_t payloadLength, bool confirmable);

    /**
     * @brief Send a confirmable POST request.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The pointer to the payload to send.
     * @param payloadLength The length of the payload. For string payloads, the length should not include the null terminator.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t sendPostRequest(IPAddress ip, uint16_t port, const char *endpoint, const void *payload, size_t payloadLength);

    /**
     * @brief Send a POST request with explicit confirmable flag.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The pointer to the payload to send.
     * @param payloadLength The length of the payload. For string payloads, the length should not include the null terminator.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t sendPostRequest(IPAddress ip, uint16_t port, const char *endpoint, const void *payload, size_t payloadLength, bool confirmable);

    /**
     * @brief Send a raw CoAP message without specifying content type.
     *
     * Specifying content type is not compulsory and can be inferred from the applications.
     * * @see https://datatracker.ietf.org/doc/html/rfc7252#section-5.5.1
     *
     * To send a message with content type, use the overload with the contentType parameter.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param type The CoAP message type (confirmable, non-confirmable, etc
     * @param method The CoAP method (GET, POST, etc).
     * @param token The pointer to the token to use.
     * @param tokenLength The length of the token.
     * @param payload The pointer to the payload to send.
     * @param payloadLength The length of the payload. For string payloads, the length should not include the null terminator.
     * @return The message ID of the request that was sent.
     *
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength);

    /**
     * @brief Send a raw CoAP message specifying the content format.
     *
     * @see send without contentType for more details.
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType);

    /**
     * @brief Send a raw CoAP message specifying all the parameters.
     *
     * The message will be queued for transmission and sent in the next loop cycle.
     * If the message is confirmable, retransmissions will be handled according to CoAP specifications.
     *
     * @see send without contentType for more details.
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType, uint16_t messageId);

    /**
     * @brief Process incoming packets and dispatch handlers.
     *
     * This method should be called regularly in the main loop.
     * It checks for incoming CoAP packets, processes them, and dispatches
     * them to the appropriate registered handlers.
     * It also deals with retransmissions of confirmable messages.
     */
    bool loop();
};

#endif
