#include "Arduino.h"
#include "ProsecCoAP.h"

void CoapPacket::addOption(uint8_t number, uint8_t length, uint8_t *value)
{
    if (optionCount >= COAP_MAX_OPTION_NUM)
    {
        return;
    }
    options[optionCount].number = number;
    options[optionCount].length = length;
    options[optionCount].value = value;

    ++optionCount;
}

bool CoapPacket::getObserveValue(COAP_OBSERVE_VALUE &value)
{
    for (int i = 0; i < optionCount; i++)
    {
        if (options[i].number != COAP_OBSERVE)
            continue;
        if (options[i].length > 3)
            return false;
        uint32_t v = 0;
        for (uint8_t j = 0; j < options[i].length; j++)
        {
            v = (v << 8) | options[i].value[j];
        }
        // Validate: only 0 (register) and 1 (cancel) are valid per RFC 7641.
        if (v > 1)
            return false; // Invalid observe value.
        value = (COAP_OBSERVE_VALUE)v;
        return true;
    }
    return false;
}

Coap::Coap(
    UDP &udp,
    size_t coapBufferSize /* default value is COAP_BUF_MAX_SIZE */
)
{
    this->_udp = &udp;
    this->_coapBufferSize = coapBufferSize;
    this->_txBuffer = new uint8_t[this->_coapBufferSize];
    this->_rxBuffer = new uint8_t[this->_coapBufferSize];
}

Coap::~Coap()
{
    if (this->_txBuffer != NULL)
        delete[] this->_txBuffer;

    if (this->_rxBuffer != NULL)
        delete[] this->_rxBuffer;
}

bool Coap::start()
{
    this->start(COAP_DEFAULT_PORT);
    return true;
}

bool Coap::start(uint16_t port)
{
    this->_udp->begin(port);
    return true;
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip)
{
    return this->sendPacket(packet, ip, COAP_DEFAULT_PORT);
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip, uint16_t port)
{
    uint8_t *p = this->_txBuffer;
    uint16_t runningDelta = 0;
    uint16_t packetSize = 0;

    // Coap packet base header.
    *p = 0x01 << 6;
    *p |= (packet.type & 0x03) << 4;
    *p++ |= (packet.tokenLength & 0x0F);
    *p++ = packet.code;
    *p++ = (packet.messageId >> 8);
    *p++ = (packet.messageId & 0xFF);
    p = this->_txBuffer + COAP_HEADER_SIZE;
    packetSize += 4;

    // Add the token, if present and valid.
    if (packet.token != NULL && packet.tokenLength <= 0x0F)
    {
        memcpy(p, packet.token, packet.tokenLength);
        p += packet.tokenLength;
        packetSize += packet.tokenLength;
    }

    // Add option header according to specifications.
    // https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
    for (int i = 0; i < packet.optionCount; i++)
    {
        uint32_t optionDelta;
        uint8_t len, delta;

        if (packetSize + 5u + packet.options[i].length >= _coapBufferSize)
        {
            return 0;
        }
        optionDelta = packet.options[i].number - runningDelta;
        COAP_OPTION_DELTA(optionDelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet.options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13)
        {
            *p++ = (optionDelta - 13);
            packetSize++;
        }
        else if (delta == 14)
        {
            *p++ = ((optionDelta - 269) >> 8);
            *p++ = (0xFF & (optionDelta - 269));
            packetSize += 2;
        }
        if (len == 13)
        {
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        }
        else if (len == 14)
        {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize += 2;
        }

        memcpy(p, packet.options[i].value, packet.options[i].length);
        p += packet.options[i].length;
        packetSize += packet.options[i].length + 1;
        runningDelta = packet.options[i].number;
    }

    // Append the payload.
    if (packet.payloadLength > 0)
    {
        if ((packetSize + 1 + packet.payloadLength) >= _coapBufferSize)
        {
            return 0;
        }
        *p++ = 0xFF;
        memcpy(p, packet.payload, packet.payloadLength);
        packetSize += 1 + packet.payloadLength;
    }

    _udp->beginPacket(ip, port);
    _udp->write(this->_txBuffer, packetSize);
    _udp->endPacket();

    return packet.messageId;
}

uint16_t Coap::getRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_GET, NULL, 0, NULL, 0);
}

uint16_t Coap::deleteRequest(IPAddress ip, uint16_t port, const char *endpoint, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_DELETE, NULL, 0, NULL, 0);
}

uint16_t Coap::putRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_PUT, NULL, 0, (uint8_t *)payload, strlen(payload));
}

uint16_t Coap::putRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payloadLength, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_PUT, NULL, 0, (uint8_t *)payload, payloadLength);
}

uint16_t Coap::postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_POST, NULL, 0, (uint8_t *)payload, strlen(payload));
}

uint16_t Coap::postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payloadLength, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_POST, NULL, 0, (uint8_t *)payload, payloadLength);
}

// Send CoAP request with payload, without specifying content type.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength)
{
    return this->send(ip, port, endpoint, type, method, token, tokenLength, payload, payloadLength, COAP_NONE);
}

// Send CoAP request with payload and content type.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType)
{
    return this->send(ip, port, endpoint, type, method, token, tokenLength, payload, payloadLength, contentType, rand());
}

// Send a CoAP request with payload, content type and explicit message ID.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType, uint16_t messageId)
{

    // make packet
    CoapPacket packet;

    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.tokenLength = tokenLength;
    packet.payload = payload;
    packet.payloadLength = payloadLength;
    packet.optionCount = 0;
    packet.messageId = messageId;

    // use URI_HOST UIR_PATH
    char ipAddress[16] = "";
    sprintf(ipAddress, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    packet.addOption(COAP_URI_HOST, strlen(ipAddress), (uint8_t *)ipAddress);

    /*
        Add Query Support
        Author: @YelloooBlue
    */

    // Parse endpoint.
    size_t idx = 0;
    bool hasQuery = false;
    for (size_t i = 0; i < strlen(endpoint); i++)
    {
        // The reserved characters "/"  "?"  "&"
        if (endpoint[i] == '/')
        {
            packet.addOption(COAP_URI_PATH, i - idx, (uint8_t *)(endpoint + idx)); // one URI_PATH (terminated by '/')
            idx = i + 1;
        }
        else if (endpoint[i] == '?' && !hasQuery)
        {
            packet.addOption(COAP_URI_PATH, i - idx, (uint8_t *)(endpoint + idx)); // the last URI_PATH (between / and ?)
            hasQuery = true;                                                       // now start to parse the query
            idx = i + 1;
        }
        else if (endpoint[i] == '&' && hasQuery)
        {
            packet.addOption(COAP_URI_QUERY, i - idx, (uint8_t *)(endpoint + idx)); // one URI_QUERY (terminated by '&')
            idx = i + 1;
        }
    }

    if (idx <= strlen(endpoint))
    {
        if (hasQuery)
        {
            packet.addOption(COAP_URI_QUERY, strlen(endpoint) - idx, (uint8_t *)(endpoint + idx)); // the last URI_QUERY (between &/? and the end)
        }
        else
        {
            packet.addOption(COAP_URI_PATH, strlen(endpoint) - idx, (uint8_t *)(endpoint + idx)); // the last URI_PATH (between / and the end)
        }
    }

    /*
        Adding query support ends
        Date: 2024.03.03
    */

    // if Content-Format option
    uint8_t optionBuffer[2]{0};
    if (contentType != COAP_NONE)
    {
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);
    }

    return this->sendPacket(packet, ip, port);
}

int Coap::parseOption(CoapOption *option, uint16_t *runningDelta, uint8_t **buffer, size_t bufferLength)
{
    uint8_t *p = *buffer;
    uint8_t headerLength = 1;
    uint16_t len, delta;

    if (bufferLength < headerLength)
        return -1;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13)
    {
        headerLength++;
        if (bufferLength < headerLength)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headerLength += 2;
        if (bufferLength < headerLength)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (delta == 15)
        return -1;

    if (len == 13)
    {
        headerLength++;
        if (bufferLength < headerLength)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headerLength += 2;
        if (bufferLength < headerLength)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buffer + bufferLength))
        return -1;
    option->number = delta + *runningDelta;
    option->value = p + 1;
    option->length = len;
    *buffer = p + 1 + len;
    *runningDelta += delta;

    return 0;
}

bool Coap::loop()
{
    uint32_t packetLength = _udp->parsePacket();

    while (packetLength > 0)
    {
        packetLength = _udp->read(this->_rxBuffer, packetLength >= _coapBufferSize ? _coapBufferSize : packetLength);

        CoapPacket packet;

        // parse coap packet header
        if (packetLength < COAP_HEADER_SIZE || (((this->_rxBuffer[0] & 0xC0) >> 6) != 1))
        {
            packetLength = _udp->parsePacket();
            continue;
        }

        packet.type = (this->_rxBuffer[0] & 0x30) >> 4;
        packet.tokenLength = this->_rxBuffer[0] & 0x0F;
        packet.code = this->_rxBuffer[1];
        packet.messageId = 0xFF00 & (this->_rxBuffer[2] << 8);
        packet.messageId |= 0x00FF & this->_rxBuffer[3];

        if (packet.tokenLength == 0)
            packet.token = NULL;
        else if (packet.tokenLength <= 8)
            packet.token = this->_rxBuffer + 4;
        else
        {
            packetLength = _udp->parsePacket();
            continue;
        }

        // parse packet options/payload
        if (COAP_HEADER_SIZE + packet.tokenLength < packetLength)
        {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = this->_rxBuffer + packetLength;
            uint8_t *p = this->_rxBuffer + COAP_HEADER_SIZE + packet.tokenLength;
            while (optionIndex < COAP_MAX_OPTION_NUM && p < end && *p != 0xFF)
            {
                // packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end - p))
                    return false;
                optionIndex++;
            }
            packet.optionCount = optionIndex;
            if (p < end && *p == 0xFF)
            {
                packet.payload = p + 1;
                packet.payloadLength = end - (p + 1);
            }
            else
            {
                packet.payload = NULL;
                packet.payloadLength = 0;
            }
        }

        if (packet.type == COAP_ACK)
        {
            // call response function
            if (responseHandler)
            {
                responseHandler(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }
        else
        {

            String path = "";
            path.reserve(64); // Pre-allocate memory to avoid fragmentation.

            for (int i = 0; i < packet.optionCount; i++)
            {
                // Append the URI_PATH segment to the URL, if needed.
                if (path.length() > 0)
                {
                    path += "/";
                }

                // Directly append the bytes to the String object
                for (size_t j = 0; j < packet.options[i].length; j++)
                {
                    path += (char)packet.options[i].value[j];
                }
            }

            if (!_register.find(path))
            {
                Serial.println("[CoapRegister] Not found: " + path);
                // Send a 4.04 Not Found response (https://datatracker.ietf.org/doc/html/rfc7252#section-2.2).
                sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageId, NULL, 0,
                             COAP_NOT_FOUND, COAP_NONE, NULL, 0);
            }
            else
            {
                Serial.println("[CoapRegister] Found: " + path);
                _register.find(path)(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }

        if (packet.type == COAP_CON)
        {
            // Received a message that requires acknowledgment.
            // Reply with an empty ACK.
            sendEmptyMessage(_udp->remoteIP(), _udp->remotePort(), packet.messageId);
        }

        // next packet
        packetLength = _udp->parsePacket();
    }

    return true;
}

uint16_t Coap::sendEmptyMessage(IPAddress ip, uint16_t port, uint16_t messageId)
{
    return this->sendResponse(ip, port, messageId, NULL, 0, COAP_EMPTY, COAP_NONE, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload)
{
    return this->sendResponse(ip, port, messageId, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload, size_t payloadLength)
{
    return this->sendResponse(ip, port, messageId, payload, payloadLength, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t messageId, const char *payload, size_t payloadLength,
                            COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenLength)
{
    // Populate the packet data.
    CoapPacket packet;

    packet.type = COAP_ACK;
    packet.code = COAP_CONTENT;
    packet.token = token;
    packet.tokenLength = tokenLength;
    packet.payload = (uint8_t *)payload;
    packet.payloadLength = payloadLength;
    packet.optionCount = 0;
    packet.messageId = messageId;

    // Adding Content-Format option.
    uint8_t optionValue[2] = {0};
    optionValue[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionValue[1] = ((uint16_t)type & 0x00FF);
    packet.addOption(COAP_CONTENT_FORMAT, 2, optionValue);

    return this->sendPacket(packet, ip, port);
}

static uint8_t encodeUintOption(uint32_t value, uint8_t out[3])
{
    if (value == 0)
    {
        // Special case: zero is encoded as a zero-length option.
        // https://datatracker.ietf.org/doc/html/rfc7252#section-3.2
        return 0;
    }
    if (value <= 0xFF)
    {
        out[0] = (uint8_t)value;
        return 1;
    }
    if (value <= 0xFFFF)
    {
        out[0] = (uint8_t)(value >> 8);
        out[1] = (uint8_t)(value & 0xFF);
        return 2;
    }
    out[0] = (uint8_t)((value >> 16) & 0xFF);
    out[1] = (uint8_t)((value >> 8) & 0xFF);
    out[2] = (uint8_t)(value & 0xFF);
    return 3;
}

static bool pathEquals(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return false;
    return strcmp(a, b) == 0;
}

static bool tokenEquals(const uint8_t *a, uint8_t aLength, const uint8_t *b, uint8_t bLength)
{
    if (aLength != bLength)
        return false;
    if (aLength == 0) // Both lengths are zero.
        return true;
    if (a == NULL || b == NULL)
        return false;
    return memcmp(a, b, aLength) == 0;
}

int Coap::addObserver(Observer **observerOut, const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength)
{
    if (endpoint == NULL)
        return -1; // Invalid endpoint.
    if (strlen(endpoint) >= COAP_MAX_OBSERVE_ENDPOINT_LEN)
        return -1; // Invalid endpoint.
    if (tokenLength > 8)
        return -1; // Invalid token.

    if (observerOut)
        *observerOut = NULL;

    unsigned long now = millis();

    // Check for active duplicates.
    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!_observers[i]._active)
            continue;
        if (_observers[i]._ip == ip && _observers[i]._port == (uint16_t)port && pathEquals(_observers[i]._endpoint, endpoint) && tokenEquals(_observers[i]._token, _observers[i]._tokenLength, token, tokenLength))
        {
            // Duplicate active observer, just update last seen time.
            _observers[i]._lastSeenMs = now;
            if (observerOut)
                *observerOut = &_observers[i];
            return 0;
        }
    }

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!_observers[i]._active)
        {
            _observers[i]._active = true;
            _observers[i]._ip = ip;
            _observers[i]._port = (uint16_t)port;
            _observers[i]._tokenLength = tokenLength;
            if (tokenLength > 0 && token != NULL)
                memcpy(_observers[i]._token, token, tokenLength);
            _observers[i]._observationSequentialNumber = 0;
            _observers[i]._lastSeenMs = now;
            strncpy(_observers[i]._endpoint, endpoint, COAP_MAX_OBSERVE_ENDPOINT_LEN - 1);
            _observers[i]._endpoint[COAP_MAX_OBSERVE_ENDPOINT_LEN - 1] = 0;
            if (observerOut)
                *observerOut = &_observers[i];
            return 0;
        }
    }

    return -2; // Full, could not add observer.
}

unsigned int Coap::getObserverCount()
{
    unsigned int count = 0;
    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (_observers[i]._active)
            count++;
    }
    return count;
}

bool Coap::removeObserver(const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength)
{
    if (endpoint == NULL)
        return false;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!_observers[i]._active)
            continue;
        if (_observers[i]._ip == ip && _observers[i]._port == (uint16_t)port && pathEquals(_observers[i]._endpoint, endpoint) && tokenEquals(_observers[i]._token, _observers[i]._tokenLength, token, tokenLength))
        {
            return _observers[i].remove();
        }
    }

    return false;
}

bool Observer::remove()
{
    if (!this->_active)
        return false; // Already inactive. Nothing to remove.

    // Mark as inactive.
    this->_active = false;
    return true;
}

unsigned long Observer::getLastSeenMs()
{
    return this->_lastSeenMs;
}

int Coap::notifyObservers(const char *observedEndpoint, const char *payload, int payloadLength, COAP_CONTENT_TYPE type)
{
    if (observedEndpoint == NULL)
        return 0;
    unsigned long now = millis();
    int sent = 0;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!_observers[i]._active)
            continue;
        if (!pathEquals(_observers[i]._endpoint, observedEndpoint))
            continue;

        if (COAP_OBSERVER_LEASE_MS > 0 && (unsigned long)(now - _observers[i]._lastSeenMs) > COAP_OBSERVER_LEASE_MS)
        {
            _observers[i]._active = false;
            continue;
        }

        CoapPacket packet;
        packet.type = COAP_NONCON;
        packet.code = COAP_CONTENT;
        packet.token = _observers[i]._tokenLength ? _observers[i]._token : NULL;
        packet.tokenLength = _observers[i]._tokenLength;
        packet.payload = (uint8_t *)payload;
        packet.payloadLength = payloadLength;
        packet.optionCount = 0;
        packet.messageId = rand();

        uint32_t observeSequence = ++_observers[i]._observationSequentialNumber;
        uint8_t observeBuf[3] = {0};
        uint8_t observeLength = encodeUintOption(observeSequence, observeBuf);
        packet.addOption(COAP_OBSERVE, observeLength, observeBuf);

        uint8_t optionBuffer[2] = {0};
        optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)type & 0x00FF);
        packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

        // NOTE: A notification is like a CoAP response, so it carries no POST method.
        if (this->sendPacket(packet, _observers[i]._ip, _observers[i]._port) != 0)
            sent++;
    }
    return sent;
}
