#include "Arduino.h"
#include "ProsecCoAP.h"

void CoapPacket::addOption(uint8_t number, uint8_t length, uint8_t *value)
{
    if (option_count >= COAP_MAX_OPTION_NUM)
    {
        return;
    }
    options[option_count].number = number;
    options[option_count].length = length;
    options[option_count].value = value;

    ++option_count;
}

bool CoapPacket::getObserveValue(COAP_OBSERVE_VALUE &value)
{
    for (int i = 0; i < option_count; i++)
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
    uint8_t *p = this->txBuffer;
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;

    // Coap packet base header.
    *p = 0x01 << 6;
    *p |= (packet.type & 0x03) << 4;
    *p++ |= (packet.token_length & 0x0F);
    *p++ = packet.code;
    *p++ = (packet.message_id >> 8);
    *p++ = (packet.message_id & 0xFF);
    p = this->txBuffer + COAP_HEADER_SIZE;
    packetSize += 4;

    // Add the token, if present and valid.
    if (packet.token != NULL && packet.token_length <= 0x0F)
    {
        memcpy(p, packet.token, packet.token_length);
        p += packet.token_length;
        packetSize += packet.token_length;
    }

    // Add option header according to specifications.
    // https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
    for (int i = 0; i < packet.option_count; i++)
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

    // Append the payload.
    if (packet.payload_length > 0)
    {
        if ((packetSize + 1 + packet.payload_length) >= coapBufferSize)
        {
            return 0;
        }
        *p++ = 0xFF;
        memcpy(p, packet.payload, packet.payload_length);
        packetSize += 1 + packet.payload_length;
    }

    _udp->beginPacket(ip, port);
    _udp->write(this->txBuffer, packetSize);
    _udp->endPacket();

    return packet.message_id;
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

uint16_t Coap::putRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payload_length, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_PUT, NULL, 0, (uint8_t *)payload, payload_length);
}

uint16_t Coap::postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_POST, NULL, 0, (uint8_t *)payload, strlen(payload));
}

uint16_t Coap::postRequest(IPAddress ip, uint16_t port, const char *endpoint, const char *payload, size_t payload_length, bool confirmable)
{
    return this->send(ip, port, endpoint, confirmable ? COAP_CON : COAP_NONCON, COAP_POST, NULL, 0, (uint8_t *)payload, payload_length);
}

// Send CoAP request with payload, without specifying content type.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t token_length, const uint8_t *payload, size_t payload_length)
{
    return this->send(ip, port, endpoint, type, method, token, token_length, payload, payload_length, COAP_NONE);
}

// Send CoAP request with payload and content type.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t token_length, const uint8_t *payload, size_t payload_length, COAP_CONTENT_TYPE contentType)
{
    return this->send(ip, port, endpoint, type, method, token, token_length, payload, payload_length, contentType, rand());
}

// Send a CoAP request with payload, content type and explicit message ID.
uint16_t Coap::send(IPAddress ip, uint16_t port, const char *endpoint, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t token_length, const uint8_t *payload, size_t payload_length, COAP_CONTENT_TYPE contentType, uint16_t message_id)
{

    // make packet
    CoapPacket packet;

    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.token_length = token_length;
    packet.payload = payload;
    packet.payload_length = payload_length;
    packet.option_count = 0;
    packet.message_id = message_id;

    // use URI_HOST UIR_PATH
    char ip_address[16] = "";
    sprintf(ip_address, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    packet.addOption(COAP_URI_HOST, strlen(ip_address), (uint8_t *)ip_address);

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
        packet.token_length = this->rxBuffer[0] & 0x0F;
        packet.code = this->rxBuffer[1];
        packet.message_id = 0xFF00 & (this->rxBuffer[2] << 8);
        packet.message_id |= 0x00FF & this->rxBuffer[3];

        if (packet.token_length == 0)
            packet.token = NULL;
        else if (packet.token_length <= 8)
            packet.token = this->rxBuffer + 4;
        else
        {
            packet_length = _udp->parsePacket();
            continue;
        }

        // parse packet options/payload
        if (COAP_HEADER_SIZE + packet.token_length < packet_length)
        {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = this->rxBuffer + packet_length;
            uint8_t *p = this->rxBuffer + COAP_HEADER_SIZE + packet.token_length;
            while (optionIndex < COAP_MAX_OPTION_NUM && p < end && *p != 0xFF)
            {
                // packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end - p))
                    return false;
                optionIndex++;
            }
            packet.option_count = optionIndex;
            if (p < end && *p == 0xFF)
            {
                packet.payload = p + 1;
                packet.payload_length = end - (p + 1);
            }
            else
            {
                packet.payload = NULL;
                packet.payload_length = 0;
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

            for (int i = 0; i < packet.option_count; i++)
            {
                if (packet.options[i].number == COAP_URI_PATH && packet.options[i].length > 0)
                {
                    if (path.length() > 0)
                        path += "/";
                    // Append the URI_PATH segment to the URL.
                    path += (const char *)packet.options[i].value;
                }
            }

            if (!uri.find(path))
            {
                sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.message_id, NULL, 0,
                             COAP_NOT_FOUND, COAP_NONE, NULL, 0);
            }
            else
            {
                uri.find(path)(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }

        if (packet.type == COAP_CON)
        {
            // Received a message that requires acknowledgment.
            // Reply with an empty ACK.
            sendEmptyMessage(_udp->remoteIP(), _udp->remotePort(), packet.message_id);
        }

        // next packet
        packet_length = _udp->parsePacket();
    }

    return true;
}

uint16_t Coap::sendEmptyMessage(IPAddress ip, uint16_t port, uint16_t message_id)
{
    return this->sendResponse(ip, port, message_id, NULL, 0, COAP_EMPTY, COAP_NONE, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t message_id, const char *payload)
{
    return this->sendResponse(ip, port, message_id, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t message_id, const char *payload, size_t payload_length)
{
    return this->sendResponse(ip, port, message_id, payload, payload_length, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, uint16_t port, uint16_t message_id, const char *payload, size_t payload_length,
                            COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int token_length)
{
    // Populate the packet data.
    CoapPacket packet;

    packet.type = COAP_ACK;
    packet.code = COAP_CONTENT;
    packet.token = token;
    packet.token_length = token_length;
    packet.payload = (uint8_t *)payload;
    packet.payload_length = payload_length;
    packet.option_count = 0;
    packet.message_id = message_id;

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

int Coap::addObserver(Observer **observer_out, const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t token_length)
{
    if (endpoint == NULL)
        return -1; // Invalid endpoint.
    if (strlen(endpoint) >= COAP_MAX_OBSERVE_ENDPOINT_LEN)
        return -1; // Invalid endpoint.
    if (token_length > 8)
        return -1; // Invalid token.

    if (observer_out)
        *observer_out = NULL;

    unsigned long now = millis();

    // Check for active duplicates.
    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].endpoint, endpoint) && tokenEquals(observers[i].token, observers[i].token_length, token, token_length))
        {
            // Duplicate active observer, just update last seen time.
            observers[i].last_seen_ms = now;
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
            observers[i].token_length = token_length;
            if (token_length > 0 && token != NULL)
                memcpy(observers[i].token, token, token_length);
            observers[i].observation_sequential_number = 0;
            observers[i].last_seen_ms = now;
            strncpy(observers[i].endpoint, endpoint, COAP_MAX_OBSERVE_ENDPOINT_LEN - 1);
            observers[i].endpoint[COAP_MAX_OBSERVE_ENDPOINT_LEN - 1] = 0;
            if (observer_out)
                *observer_out = &observers[i];
            return 0;
        }
    }

    return -2; // Full, could not add observer.
}

bool Coap::removeObserver(const char *endpoint, IPAddress ip, uint16_t port, const uint8_t *token, uint8_t token_length)
{
    if (endpoint == NULL)
        return false;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].endpoint, endpoint) && tokenEquals(observers[i].token, observers[i].token_length, token, token_length))
        {
            return observers[i].remove();
        }
    }

    return false;
}

bool Observer::remove()
{
    if (!this->active)
        return false; // Already inactive. Nothing to remove.

    // Mark as inactive.
    this->active = false;
    return true;
}

unsigned long Observer::getLastSeenMs()
{
    return this->last_seen_ms;
}

int Coap::notifyObservers(const char *observed_endpoint, const char *payload, int payload_len, COAP_CONTENT_TYPE type)
{
    if (observed_endpoint == NULL)
        return 0;
    unsigned long now = millis();
    int sent = 0;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (!urlEquals(observers[i].endpoint, observed_endpoint))
            continue;

        if (COAP_OBSERVER_LEASE_MS > 0 && (unsigned long)(now - observers[i].last_seen_ms) > COAP_OBSERVER_LEASE_MS)
        {
            observers[i].active = false;
            continue;
        }

        CoapPacket packet;
        packet.type = COAP_NONCON;
        packet.code = COAP_CONTENT;
        packet.token = observers[i].token_length ? observers[i].token : NULL;
        packet.token_length = observers[i].token_length;
        packet.payload = (uint8_t *)payload;
        packet.payload_length = payload_len;
        packet.option_count = 0;
        packet.message_id = rand();

        uint32_t observe_sequence = ++observers[i].observation_sequential_number;
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
