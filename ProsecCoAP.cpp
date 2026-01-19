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
    this->coapBufferSize = coapBufferSize;
    this->txBuffer = new uint8_t[this->coapBufferSize];
    this->rxBuffer = new uint8_t[this->coapBufferSize];
}

Coap::~Coap()
{
    if (this->txBuffer != NULL)
        delete[] this->txBuffer;

    if (this->rxBuffer != NULL)
        delete[] this->rxBuffer;
}

bool Coap::start()
{
    this->start(COAP_DEFAULT_PORT);
    return true;
}

bool Coap::start(int port)
{
    this->_udp->begin(port);
    return true;
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip)
{
    return this->sendPacket(packet, ip, COAP_DEFAULT_PORT);
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip, int port)
{
    uint8_t *p = this->txBuffer;
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;

    // Coap packet base header.
    *p = 0x01 << 6;
    *p |= (packet.type & 0x03) << 4;
    *p++ |= (packet.tokenLength & 0x0F);
    *p++ = packet.code;
    *p++ = (packet.messageId >> 8);
    *p++ = (packet.messageId & 0xFF);
    p = this->txBuffer + COAP_HEADER_SIZE;
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
        uint32_t optdelta;
        uint8_t len, delta;

        if (packetSize + 5u + packet.options[i].length >= coapBufferSize)
        {
            return 0;
        }
        optdelta = packet.options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet.options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13)
        {
            *p++ = (optdelta - 13);
            packetSize++;
        }
        else if (delta == 14)
        {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
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
        running_delta = packet.options[i].number;
    }

    // make payload
    if (packet.payloadLength > 0)
    {
        if ((packetSize + 1 + packet.payloadLength) >= coapBufferSize)
        {
            return 0;
        }
        *p++ = 0xFF;
        memcpy(p, packet.payload, packet.payloadLength);
        packetSize += 1 + packet.payloadLength;
    }

    _udp->beginPacket(ip, port);
    _udp->write(this->txBuffer, packetSize);
    _udp->endPacket();

    return packet.messageId;
}

uint16_t Coap::get(IPAddress ip, int port, const char *url)
{
    return this->send(ip, port, url, COAP_CON, COAP_GET, NULL, 0, NULL, 0);
}

uint16_t Coap::put(IPAddress ip, int port, const char *url, const char *payload)
{
    return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, strlen(payload));
}

uint16_t Coap::put(IPAddress ip, int port, const char *url, const char *payload, size_t payloadLength)
{
    return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, payloadLength);
}

uint16_t Coap::post(IPAddress ip, int port, const char *url, const char *payload)
{
    return this->send(ip, port, url, COAP_CON, COAP_POST, NULL, 0, (uint8_t *)payload, strlen(payload));
}

uint16_t Coap::post(IPAddress ip, int port, const char *url, const char *payload, size_t payloadLength)
{
    return this->send(ip, port, url, COAP_CON, COAP_POST, NULL, 0, (uint8_t *)payload, payloadLength);
}

// Send CoAP request with payload, without specifying content type.
uint16_t Coap::send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength)
{
    return this->send(ip, port, url, type, method, token, tokenLength, payload, payloadLength, COAP_NONE);
}

// Send CoAP request with payload and content type.
uint16_t Coap::send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType)
{
    return this->send(ip, port, url, type, method, token, tokenLength, payload, payloadLength, contentType, rand());
}

// Send a CoAP request with payload, content type and explicit message ID.
uint16_t Coap::send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType, uint16_t messageId)
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
    char ip_address[16] = "";
    sprintf(ip_address, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    packet.addOption(COAP_URI_HOST, strlen(ip_address), (uint8_t *)ip_address);

    /*
        Add Query Support
        Author: @YelloooBlue
    */

    // Parse url.
    size_t idx = 0;
    bool hasQuery = false;
    for (size_t i = 0; i < strlen(url); i++)
    {
        // The reserved characters "/"  "?"  "&"
        if (url[i] == '/')
        {
            packet.addOption(COAP_URI_PATH, i - idx, (uint8_t *)(url + idx)); // one URI_PATH (terminated by '/')
            idx = i + 1;
        }
        else if (url[i] == '?' && !hasQuery)
        {
            packet.addOption(COAP_URI_PATH, i - idx, (uint8_t *)(url + idx)); // the last URI_PATH (between / and ?)
            hasQuery = true;                                                  // now start to parse the query
            idx = i + 1;
        }
        else if (url[i] == '&' && hasQuery)
        {
            packet.addOption(COAP_URI_QUERY, i - idx, (uint8_t *)(url + idx)); // one URI_QUERY (terminated by '&')
            idx = i + 1;
        }
    }

    if (idx <= strlen(url))
    {
        if (hasQuery)
        {
            packet.addOption(COAP_URI_QUERY, strlen(url) - idx, (uint8_t *)(url + idx)); // the last URI_QUERY (between &/? and the end)
        }
        else
        {
            packet.addOption(COAP_URI_PATH, strlen(url) - idx, (uint8_t *)(url + idx)); // the last URI_PATH (between / and the end)
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

int Coap::parseOption(CoapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen)
{
    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen)
        return -1;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (delta == 15)
        return -1;

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + buflen))
        return -1;
    option->number = delta + *running_delta;
    option->value = p + 1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

bool Coap::loop()
{
    uint32_t packet_length = _udp->parsePacket();

    while (packet_length > 0)
    {
        packet_length = _udp->read(this->rxBuffer, packet_length >= coapBufferSize ? coapBufferSize : packet_length);

        CoapPacket packet;

        // parse coap packet header
        if (packet_length < COAP_HEADER_SIZE || (((this->rxBuffer[0] & 0xC0) >> 6) != 1))
        {
            packet_length = _udp->parsePacket();
            continue;
        }

        packet.type = (this->rxBuffer[0] & 0x30) >> 4;
        packet.tokenLength = this->rxBuffer[0] & 0x0F;
        packet.code = this->rxBuffer[1];
        packet.messageId = 0xFF00 & (this->rxBuffer[2] << 8);
        packet.messageId |= 0x00FF & this->rxBuffer[3];

        if (packet.tokenLength == 0)
            packet.token = NULL;
        else if (packet.tokenLength <= 8)
            packet.token = this->rxBuffer + 4;
        else
        {
            packet_length = _udp->parsePacket();
            continue;
        }

        // parse packet options/payload
        if (COAP_HEADER_SIZE + packet.tokenLength < packet_length)
        {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = this->rxBuffer + packet_length;
            uint8_t *p = this->rxBuffer + COAP_HEADER_SIZE + packet.tokenLength;
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

            String url = "";
            // call endpoint url function
            for (int i = 0; i < packet.optionCount; i++)
            {
                if (packet.options[i].number == COAP_URI_PATH && packet.options[i].length > 0)
                {
                    if (packet.options[i].length > 0)
                    {
                        // Ignore empty URI_PATH segments.
                        continue;
                    }
                    if (url.length() > 0)
                        url += "/";
                    // Append the URI_PATH segment to the URL.
                    url += String((const char *)packet.options[i].value, packet.options[i].length);
                }
            }

            if (!uri.find(url))
            {
                sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageId, NULL, 0,
                             COAP_NOT_FOUND, COAP_NONE, NULL, 0);
            }
            else
            {
                uri.find(url)(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }

        if (packet.type == COAP_CON)
        {
            // Received a message that requires acknowledgment.
            // Reply with an empty ACK.
            sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageId);
        }

        // next packet
        packet_length = _udp->parsePacket();
    }

    return true;
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageId)
{
    return this->sendResponse(ip, port, messageId, NULL, 0, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload)
{
    return this->sendResponse(ip, port, messageId, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload, size_t payloadLength)
{
    return this->sendResponse(ip, port, messageId, payload, payloadLength, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageId, const char *payload, size_t payloadLength,
                            COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenLength)
{
    // make packet
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

static bool urlEquals(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return false;
    return strcmp(a, b) == 0;
}

static bool tokenEquals(const uint8_t *a, uint8_t alen, const uint8_t *b, uint8_t blen)
{
    if (alen != blen)
        return false;
    if (alen == 0)
        return true;
    if (a == NULL || b == NULL)
        return false;
    return memcmp(a, b, alen) == 0;
}

int Coap::addObserver(Observer **observer_out, const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenLength)
{
    if (url == NULL)
        return -1; // Invalid URL.
    if (strlen(url) >= COAP_MAX_OBSERVE_URL_LEN)
        return -1; // Invalid URL.
    if (tokenLength > 8)
        return -1; // Invalid token.

    if (observer_out)
        *observer_out = NULL;

    unsigned long now = millis();

    // Check for active duplicates.
    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].url, url) && tokenEquals(observers[i].token, observers[i].tokenLength, token, tokenLength))
        {
            // Duplicate active observer, just update last seen time.
            observers[i].lastSeenMs = now;
            if (observer_out)
                *observer_out = &observers[i];
            return 0;
        }
    }

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
        {
            observers[i].active = true;
            observers[i].ip = ip;
            observers[i].port = (uint16_t)port;
            observers[i].tokenLength = tokenLength;
            if (tokenLength > 0 && token != NULL)
                memcpy(observers[i].token, token, tokenLength);
            observers[i].observationSequentialNumber = 0;
            observers[i].lastSeenMs = now;
            strncpy(observers[i].url, url, COAP_MAX_OBSERVE_URL_LEN - 1);
            observers[i].url[COAP_MAX_OBSERVE_URL_LEN - 1] = 0;
            if (observer_out)
                *observer_out = &observers[i];
            return 0;
        }
    }

    return -2; // Full, could not add observer.
}

uint16_t Coap::sendObserveRegisterConfirmation(Observer *observer, uint16_t messageId)
{
    CoapPacket packet;

    packet.type = COAP_ACK;   // ACK the registration.
    packet.code = COAP_EMPTY; // No payload. Refer to https://www.rfc-editor.org/rfc/rfc7252#section-5.2.2
    packet.token = observer->token;
    packet.tokenLength = observer->tokenLength;
    packet.payload = NULL;
    packet.payloadLength = 0;
    packet.optionCount = 0;
    packet.messageId = messageId;

    // When registering a new observer, the observe option value is send back.
    // https://datatracker.ietf.org/doc/html/rfc7641#section-3.1
    // The value will be the sequential number, hard coded to start from 0.
    uint8_t observeBuf[3] = {0};
    uint8_t observeLen = encodeUintOption(0, observeBuf);
    packet.addOption(COAP_OBSERVE, observeLen, observeBuf);

    return this->sendPacket(packet, observer->ip, observer->port);
}

bool Coap::removeObserver(const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenLength)
{
    if (url == NULL)
        return false;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].url, url) && tokenEquals(observers[i].token, observers[i].tokenLength, token, tokenLength))
        {
            return observers[i].remove();
        }
    }

    return false;
}

bool Observer::remove()
{
    bool removed = false;
    if (!this->active)
        // Already inactive. Nothing to remove.
        return false;
    if (this->ip == ip && this->port == (uint16_t)port && urlEquals(this->url, url) && tokenEquals(this->token, this->tokenLength, token, tokenLength))
    {
        // Mark as inactive.
        this->active = false;
        removed = true;
    }

    return removed;
}

unsigned long Observer::getLastSeenMs()
{
    return this->lastSeenMs;
}

int Coap::notifyObservers(const char *url, const char *payload, int payload_len, COAP_CONTENT_TYPE type)
{
    if (url == NULL)
        return 0;
    unsigned long now = millis();
    int sent = 0;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (!urlEquals(observers[i].url, url))
            continue;

        if (COAP_OBSERVER_LEASE_MS > 0 && (unsigned long)(now - observers[i].lastSeenMs) > COAP_OBSERVER_LEASE_MS)
        {
            observers[i].active = false;
            continue;
        }

        CoapPacket packet;
        packet.type = COAP_NONCON;
        packet.code = COAP_CONTENT;
        packet.token = observers[i].tokenLength ? observers[i].token : NULL;
        packet.tokenLength = observers[i].tokenLength;
        packet.payload = (uint8_t *)payload;
        packet.payloadLength = payload_len;
        packet.optionCount = 0;
        packet.messageId = rand();

        uint32_t observe_sequence = ++observers[i].observationSequentialNumber;
        uint8_t observeBuf[3] = {0};
        uint8_t observeLen = encodeUintOption(observe_sequence, observeBuf);
        packet.addOption(COAP_OBSERVE, observeLen, observeBuf);

        uint8_t optionBuffer[2] = {0};
        optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)type & 0x00FF);
        packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

        // NOTE: A notification is like a CoAP response, so it carries no POST method.
        if (this->sendPacket(packet, observers[i].ip, observers[i].port) != 0)
            sent++;
    }
    return sent;
}
