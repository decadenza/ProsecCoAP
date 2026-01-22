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
#ifndef COAP_MAX_CALLBACK
#define COAP_MAX_CALLBACK 10
#endif

#define COAP_HEADER_SIZE 4u
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF
#ifndef COAP_MAX_OPTION_NUM
#define COAP_MAX_OPTION_NUM 10
#endif
#ifndef COAP_BUF_MAX_SIZE
#define COAP_BUF_MAX_SIZE 128
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
#ifndef COAP_MAX_OBSERVE_ENDPOINT_LEN
#define COAP_MAX_OBSERVE_ENDPOINT_LEN 32
#endif
#define COAP_DEFAULT_PORT 5683

#define RESPONSE_CODE(class, detail) ((class << 5) | (detail))
#define COAP_OPTION_DELTA(v, n) (v < 13 ? (*n = (0xFF & v)) : (v <= 0xFF + 13 ? (*n = 13) : (*n = 14)))

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
    COAP_EMPTY = RESPONSE_CODE(0, 0),
    COAP_CREATED = RESPONSE_CODE(2, 1),
    COAP_DELETED = RESPONSE_CODE(2, 2),
    COAP_VALID = RESPONSE_CODE(2, 3),
    COAP_CHANGED = RESPONSE_CODE(2, 4),
    COAP_CONTENT = RESPONSE_CODE(2, 5),

    COAP_BAD_REQUEST = RESPONSE_CODE(4, 0),
    COAP_UNAUTHORIZED = RESPONSE_CODE(4, 1),
    COAP_BAD_OPTION = RESPONSE_CODE(4, 2),
    COAP_FORBIDDEN = RESPONSE_CODE(4, 3),
    COAP_NOT_FOUND = RESPONSE_CODE(4, 4),
    COAP_METHOD_NOT_ALLOWED = RESPONSE_CODE(4, 5),
    COAP_NOT_ACCEPTABLE = RESPONSE_CODE(4, 6),
    COAP_PRECONDITION_FAILED = RESPONSE_CODE(4, 12),
    COAP_REQUEST_ENTITY_TOO_LARGE = RESPONSE_CODE(4, 13),
    COAP_UNSUPPORTED_CONTENT_FORMAT = RESPONSE_CODE(4, 15),

    COAP_INTERNAL_SERVER_ERROR = RESPONSE_CODE(5, 0),
    COAP_NOT_IMPLEMENTED = RESPONSE_CODE(5, 1),
    COAP_BAD_GATEWAY = RESPONSE_CODE(5, 2),
    COAP_SERVICE_UNAVAILABLE = RESPONSE_CODE(5, 3),
    COAP_GATEWAY_TIMEOUT = RESPONSE_CODE(5, 4),
    COAP_PROXYING_NOT_SUPPORTED = RESPONSE_CODE(5, 5)
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
    COAP_OBSERVE_VALUE_REGISTER = 0, // https://datatracker.ietf.org/doc/html/rfc7641#section-2
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

class CoapPacket
{
public:
    uint8_t type = 0;
    uint8_t code = 0;
    const uint8_t *token = NULL;
    uint8_t tokenLength = 0;
    const uint8_t *payload = NULL;
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
     * @brief Fetch the observe value (either 1 or 0).
     *
     * @return true if the observe option is present and value retrieved successfully, false otherwise.
     */
    bool getObserveValue(COAP_OBSERVE_VALUE &value);
};

#if defined(ESP8266)
#include <functional>
typedef std::function<void(CoapPacket &, IPAddress, uint16_t)> CoapCallback;
#elif defined(ESP32)
#include <functional>
typedef std::function<void(CoapPacket &, IPAddress, uint16_t)> CoapCallback;
#else
typedef void (*CoapCallback)(CoapPacket &, IPAddress, uint16_t);
#endif

/**
 * @brief Register for CoAP callbacks and their associated URI paths.
 */
class CoapRegister
{
private:
    String uriPaths[COAP_MAX_CALLBACK];
    CoapCallback callbacks[COAP_MAX_CALLBACK];

public:
    /**
     * @brief Create an empty URI callback registry.
     */
    CoapRegister()
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
        {
            uriPaths[i] = "";
            callbacks[i] = NULL;
        }
    };

    /**
     * @brief Register or update a callback for a URL.
     */
    void add(CoapCallback call, String path)
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
            if (callbacks[i] != NULL && uriPaths[i].equals(path))
            {
                callbacks[i] = call;
                return;
            }
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
        {
            if (callbacks[i] == NULL)
            {
                callbacks[i] = call;
                uriPaths[i] = path;
                return;
            }
        }
    };

    /**
     * @brief Find a callback bound to a URI path.
     */
    CoapCallback find(String path)
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
            if (callbacks[i] != NULL && uriPaths[i].equals(path))
                return callbacks[i];
        return NULL;
    };
};

/**
 * Represents a CoAP observer.
 */
class Observer
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
     * by itself (@ref Observer::remove).
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

class Coap
{
private:
    UDP *_udp;
    CoapRegister _register;
    CoapCallback responseHandler = NULL;
    int _port;
    size_t _coapBufferSize;
    uint8_t *_txBuffer = NULL;
    uint8_t *_rxBuffer = NULL;

    /**
     * @brief Array of registered _observers.
     *
     * Preallocated array to hold observer entries at runtime.
     *
     * See also @ref COAP_MAX_OBSERVERS.
     */
    Observer _observers[COAP_MAX_OBSERVERS];

    /**
     * @brief Send a CoAP packet to the specified IP.
     *
     * It uses the default CoAP port, see @ref COAP_DEFAULT_PORT.
     *
     * @return The message ID of the sent packet.
     */
    uint16_t sendPacket(CoapPacket &packet, IPAddress ip);

    /**
     * @brief Send a CoAP packet to the specified IP and port.
     *
     * @return The message ID of the sent packet.
     */
    uint16_t sendPacket(CoapPacket &packet, IPAddress ip, uint16_t port);

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
     * @param coapBufferSize The size of the internal CoAP buffers. Default is @ref COAP_BUF_MAX_SIZE.
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
     * @return Number of _observers notified successfully.
     */
    int notifyObservers(const char *observedEndpoint, const char *payload, int payloadLength, COAP_CONTENT_TYPE type);

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
    int addObserver(Observer **observerOut, const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength);

    /**
     * @brief Remove an observer for a specific endpoint.
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
     * @brief Set the response callback for acknowledgements.
     */
    void response(CoapCallback c) { responseHandler = c; }

    /**
     * @brief Register a server callback for a URI.
     */
    void server(CoapCallback callback, String path) { _register.add(callback, path); }

    /**
     * @brief Send an empty message.
     *
     * According to the protocol, an "Empty Message" is a message with a Code of 0.00;
     * neither a request nor a response. An Empty message only contains the 4-byte header.
     *
     * @param ip The IP address to send the message to.
     * @param port The port to send the message to.
     * @param messageId The message ID for the empty message. This must match the message ID
     *                   of the message being acknowledged, or be a new message ID if initiating
     *                   a new message.
     */
    uint16_t sendEmptyMessage(IPAddress ip, uint16_t port, uint16_t messageId);

    /**
     * @brief Send a response with a payload.
     */
    uint16_t sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload);

    /**
     * @brief Send a response with payload and its explicit length.
     */
    uint16_t sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload, size_t payloadLength);

    /**
     * @brief Send a fully customized response.
     */
    uint16_t sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload, size_t payloadLength, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenLength);

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
    uint16_t getRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable = true);

    /**
     * @brief Send a DELETE request.
     *
     * @see getRequest.
     */
    uint16_t deleteRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable = true);

    /**
     * @brief Send a confirmable PUT request with a payload.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The payload to send.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t putRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, bool confirmable = true);

    /**
     * @brief Send a confirmable PUT with explicit payload and corresponding length.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The payload to send.
     * @param payloadLength The length of the payload.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t putRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payloadLength, bool confirmable = true);

    /**
     * @brief Send a confirmable POST with null-terminated payload.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The payload to send.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, bool confirmable = true);

    /**
     * @brief Send a confirmable POST with explicit payload length.
     *
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @param endpoint The endpoint to request.
     * @param payload The payload to send.
     * @param payloadLength The length of the payload.
     * @param confirmable Whether to send a confirmable (true) or non-confirmable (false) request. Default to true.
     *
     * @return The message ID of the request that was sent.
     */
    uint16_t postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payloadLength, bool confirmable = true);

    /**
     * @brief Send a raw CoAP message.
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength);

    /**
     * @brief Send a raw CoAP message specifying the content format.
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType);

    /**
     * @brief Send a raw CoAP message with explicit message ID.
     */
    uint16_t send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType, uint16_t messageId);

    /**
     * @brief Process incoming packets and dispatch handlers.
     */
    bool loop();
};

#endif
