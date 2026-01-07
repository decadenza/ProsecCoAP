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
#ifndef __SIMPLE_COAP_H__
#define __SIMPLE_COAP_H__

#include "Udp.h"
#ifndef COAP_MAX_CALLBACK
#define COAP_MAX_CALLBACK 10
#endif

#define COAP_HEADER_SIZE 4
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF
#ifndef COAP_MAX_OPTION_NUM
#define COAP_MAX_OPTION_NUM 10
#endif
#ifndef COAP_BUF_MAX_SIZE
#define COAP_BUF_MAX_SIZE 128
#endif
#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS 4
#endif
#ifndef COAP_OBSERVER_LEASE_MS
#define COAP_OBSERVER_LEASE_MS 60000UL
#endif
#ifndef COAP_MAX_OBSERVE_URL_LEN
#define COAP_MAX_OBSERVE_URL_LEN 32
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
    COAP_PROXY_SCHEME = 39
} COAP_OPTION_NUMBER;

typedef enum
{
    COAP_OBSERVE_VALUE_REGISTER = 0, // https://datatracker.ietf.org/doc/html/rfc7641#section-3.1
    COAP_OBSERVE_VALUE_CANCEL = 1    // https://datatracker.ietf.org/doc/html/rfc7641#section-3.6
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

class CoapOption
{
public:
    uint8_t number;
    uint8_t length;
    uint8_t *buffer;
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
     */
    void addOption(uint8_t number, uint8_t length, uint8_t *optPayload);

    /**
     * @brief Check whether the packet requests observation.
     */
    bool isObserve();

    /**
     * @brief Fetch the observe value (either 1 or 0).
     *
     * @return true if the observe option is present and value retrieved successfully, false otherwise.
     */
    bool getObserveValue(COAP_OBSERVE_VALUE &value);
};

#if defined(ESP8266)
#include <functional>
typedef std::function<void(CoapPacket &, IPAddress, int)> CoapCallback;
#elif defined(ESP32)
#include <functional>
typedef std::function<void(CoapPacket &, IPAddress, int)> CoapCallback;
#else
typedef void (*CoapCallback)(CoapPacket &, IPAddress, int);
#endif

class CoapUri
{
private:
    String urls[COAP_MAX_CALLBACK];
    CoapCallback callbacks[COAP_MAX_CALLBACK];

public:
    /**
     * @brief Create an empty URI callback registry.
     */
    CoapUri()
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
        {
            urls[i] = "";
            callbacks[i] = NULL;
        }
    };

    /**
     * @brief Register or update a callback for a URL.
     */
    void add(CoapCallback call, String url)
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
            if (callbacks[i] != NULL && urls[i].equals(url))
            {
                callbacks[i] = call;
                return;
            }
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
        {
            if (callbacks[i] == NULL)
            {
                callbacks[i] = call;
                urls[i] = url;
                return;
            }
        }
    };

    /**
     * @brief Find a callback bound to a URL.
     */
    CoapCallback find(String url)
    {
        for (int i = 0; i < COAP_MAX_CALLBACK; i++)
            if (callbacks[i] != NULL && urls[i].equals(url))
                return callbacks[i];
        return NULL;
    };
};

/**
 * @brief The Observer class is used to manage CoAP observers.
 */
class Observer
{
public:
    IPAddress ip;
    int port = 0;
    uint8_t token[8];
    int tokenLength = 0;
    uint16_t counter = 0; // Will be used as message ID.

    /**
     * @brief Construct a new Observer object.
     */
    Observer(IPAddress ip, int port, const uint8_t *token, int tokenLength);
};

class Coap
{
private:
    UDP *_udp;
    CoapUri uri;
    CoapCallback responseHandler = NULL;
    int _port;
    int coapBufferSize;
    uint8_t *txBuffer = NULL;
    uint8_t *rxBuffer = NULL;

    /**
     * Represents an observe entry in the observer array.
     */
    struct ObserveEntry
    {
        /**
         * @brief Whether this entry is in use.
         */
        bool in_use = false;
        /**
         * @brief The IP address of the observer.
         */
        IPAddress ip;
        /**
         * @brief The port of the observer.
         */
        uint16_t port = 0;
        /**
         * @brief The token used by the observer.
         */
        uint8_t token[8] = {0};
        uint8_t tokenLength = 0;
        uint32_t observationSequentialNumber = 0;
        unsigned long lastSeenMs = 0; // TODO: Implement cleaning up old observers.
        /**
         * @brief The URL being observed.
         */
        char url[COAP_MAX_OBSERVE_URL_LEN] = {0};
    };
    ObserveEntry observers[COAP_MAX_OBSERVERS];

    uint16_t sendPacket(CoapPacket &packet, IPAddress ip);
    uint16_t sendPacket(CoapPacket &packet, IPAddress ip, int port);
    int parseOption(CoapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen);

public:
    /**
     * @brief Construct a CoAP instance using the given UDP transport.
     */
    Coap(
        UDP &udp,
        int coapBufferSize = COAP_BUF_MAX_SIZE);

    /**
     * @brief Destroy the CoAP instance and free buffers.
     */
    ~Coap();

    /**
     * @brief Notify all observers of a specific URL with a non-confirmable message.
     */
    int notifyObservers(const char *url, const char *payload, int payload_len, COAP_CONTENT_TYPE type);

    /**
     * @brief Add a new observer for a specific URL.
     *
     * @return true if the observer was added successfully, false otherwise.
     */
    bool addObserver(const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenLength);

    /**
     * @brief Send an observe confirmation response to a client.
     */
    uint16_t sendObserveConfirmation(IPAddress ip, int port, uint16_t messageId, const char *payload, size_t payloadLength, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenLength, uint32_t observationSequentialNumber);

    /**
     * @brief Remove an observer for a specific URL.
     */
    bool removeObserver(const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenLength);

    /**
     * @brief Start the server on the default port.
     */
    bool start();

    /**
     * @brief Start the server on a custom port.
     */
    bool start(int port);

    /**
     * @brief Set the response callback for acknowledgements.
     */
    void response(CoapCallback c) { responseHandler = c; }

    /**
     * @brief Register a server callback for a URI.
     */
    void server(CoapCallback c, String url) { uri.add(c, url); }

    /**
     * @brief Send a basic acknowledgment with empty payload.
     */
    uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId);

    /**
     * @brief Send a text acknowledgment response.
     */
    uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload);

    /**
     * @brief Send a typed acknowledgment response with explicit length.
     */
    uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload, size_t payloadLength);

    /**
     * @brief Send a fully customized acknowledgment response.
     */
    uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload, size_t payloadLength, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenLength);

    /**
     * @brief Send a confirmable GET request.
     */
    uint16_t get(IPAddress ip, int port, const char *url);

    /**
     * @brief Send a confirmable PUT with null-terminated payload.
     */
    uint16_t put(IPAddress ip, int port, const char *url, const char *payload);

    /**
     * @brief Send a confirmable PUT with explicit payload length.
     */
    uint16_t put(IPAddress ip, int port, const char *url, const char *payload, size_t payloadLength);

    /**
     * @brief Send a CoAP request with optional payload.
     */
    uint16_t send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength);

    /**
     * @brief Send a CoAP request specifying the content format.
     */
    uint16_t send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType);

    /**
     * @brief Send a CoAP request with explicit message ID.
     */
    uint16_t send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType, uint16_t messageId);

    /**
     * @brief Process incoming packets and dispatch handlers.
     */
    bool loop();
};

#endif
