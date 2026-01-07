#include "proseccoap.h"
#include "Arduino.h"

void CoapPacket::addOption(uint8_t number, uint8_t length, uint8_t *opt_payload)
{
    if (optionCount >= COAP_MAX_OPTION_NUM)
    {
        return;
    }
    options[optionCount].number = number;
    options[optionCount].length = length;
    options[optionCount].buffer = opt_payload;

    ++optionCount;
}

bool CoapPacket::isObserve()
{
    for (int i = 0; i < optionCount; i++)
    {
        if (options[i].number == COAP_OBSERVE)
        {
            return true;
        }
    }
    return false;
}

bool CoapPacket::getObserveValue(uint32_t &value)
{
    for (int i = 0; i < optionnum; i++)
    {
        if (options[i].number != COAP_OBSERVE)
            continue;
        if (options[i].length > 3)
            return false;
        uint32_t v = 0;
        for (uint8_t j = 0; j < options[i].length; j++)
        {
            v = (v << 8) | options[i].buffer[j];
        }
        value = v;
        return true;
    }
    return false;
}

Coap::Coap(
    UDP &udp,
    int coapBufferSize /* default value is COAP_BUF_MAX_SIZE */
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

    // make coap packet base header
    *p = 0x01 << 6;
    *p |= (packet.type & 0x03) << 4;
    *p++ |= (packet.tokenLength & 0x0F);
    *p++ = packet.code;
    *p++ = (packet.messageId >> 8);
    *p++ = (packet.messageId & 0xFF);
    p = this->txBuffer + COAP_HEADER_SIZE;
    packetSize += 4;

    // make token
    if (packet.token != NULL && packet.tokenLength <= 0x0F)
    {
        memcpy(p, packet.token, packet.tokenLength);
        p += packet.tokenLength;
        packetSize += packet.tokenLength;
    }

    // make option header
    for (int i = 0; i < packet.optionCount; i++)
    {
        uint32_t optdelta;
        uint8_t len, delta;

        if (packetSize + 5 + packet.options[i].length >= coapBufferSize)
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

        memcpy(p, packet.options[i].buffer, packet.options[i].length);
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

uint16_t Coap::send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength)
{
    return this->send(ip, port, url, type, method, token, tokenLength, payload, payloadLength, COAP_NONE);
}

uint16_t Coap::send(IPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenLength, const uint8_t *payload, size_t payloadLength, COAP_CONTENT_TYPE contentType)
{
    return this->send(ip, port, url, type, method, token, tokenLength, payload, payloadLength, contentType, rand());
}

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
    option->buffer = p + 1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

bool Coap::loop()
{
    int32_t packet_length = _udp->parsePacket();

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
                    char urlname[packet.options[i].length + 1];
                    memcpy(urlname, packet.options[i].buffer, packet.options[i].length);
                    urlname[packet.options[i].length] = 0;
                    if (url.length() > 0)
                        url += "/";
                    url += (const char *)urlname;
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
    packet.code = code;
    packet.token = token;
    packet.tokenLength = tokenLength;
    packet.payload = (uint8_t *)payload;
    packet.payloadLength = payloadLength;
    packet.optionCount = 0;
    packet.messageId = messageId;

    // if more options?
    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF);
    packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    return this->sendPacket(packet, ip, port);
}

static uint8_t encodeUintOption(uint32_t value, uint8_t out[3])
{
    if (value == 0)
    {
        out[0] = 0;
        return 1;
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

uint16_t Coap::sendObserveResponse(IPAddress ip, int port, uint16_t messageid, const char *payload, size_t payloadlen,
                                   COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenlen, uint32_t observe_seq)
{
    CoapPacket packet;

    packet.type = COAP_ACK;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t *)payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

    uint8_t observeBuf[3] = {0};
    uint8_t observeLen = encodeUintOption(observe_seq, observeBuf);
    packet.addOption(COAP_OBSERVE, observeLen, observeBuf);

    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF);
    packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    return this->sendPacket(packet, ip, port);
}

Observer::Observer(IPAddress ip, int port, const uint8_t *token, int token_length)
    : ip(ip), port(port), token_length(token_length), counter(0)
{
    if (this->token_length > 8)
        this->token_length = 8;
    if (this->token_length > 0 && token != NULL)
        memcpy(this->token, token, this->token_length);
}

uint16_t Coap::notify(Observer *observer, const char *payload, int payloadLength, COAP_CONTENT_TYPE type)
{
    CoapPacket packet;

    packet.type = COAP_NONCON; // Notifications are non-confirmable.
    packet.code = COAP_CONTENT;
    packet.token = observer->token;
    packet.tokenLength = observer->tokenLength;
    packet.payload = (uint8_t *)payload;
    packet.payloadLength = payloadLength;
    packet.optionCount = 0;
    uint32_t observe_seq = ++observer->counter;
    packet.messageId = rand();

    uint8_t observeBuf[3] = {0};
    uint8_t observeLen = encodeUintOption(observe_seq, observeBuf);
    packet.addOption(COAP_OBSERVE, observeLen, observeBuf);

    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF);
    packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    return this->sendPacket(packet, observer->ip, observer->port);
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

bool Coap::addObserver(const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenlen)
{
    if (url == NULL)
        return false;
    if (strlen(url) >= COAP_MAX_OBSERVE_URL_LEN)
        return false;
    if (tokenlen > 8)
        return false;

    unsigned long now = millis();

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].in_use)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].url, url) && tokenEquals(observers[i].token, observers[i].tokenlen, token, tokenlen))
        {
            observers[i].last_seen_ms = now;
            return true;
        }
    }

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].in_use)
        {
            observers[i].in_use = true;
            observers[i].ip = ip;
            observers[i].port = (uint16_t)port;
            observers[i].tokenlen = tokenlen;
            if (tokenlen > 0 && token != NULL)
                memcpy(observers[i].token, token, tokenlen);
            observers[i].observe_seq = 0;
            observers[i].last_seen_ms = now;
            strncpy(observers[i].url, url, COAP_MAX_OBSERVE_URL_LEN - 1);
            observers[i].url[COAP_MAX_OBSERVE_URL_LEN - 1] = 0;
            return true;
        }
    }

    return false;
}

bool Coap::removeObserver(const char *url, IPAddress ip, int port, const uint8_t *token, uint8_t tokenlen)
{
    if (url == NULL)
        return false;
    bool removed = false;
    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].in_use)
            continue;
        if (observers[i].ip == ip && observers[i].port == (uint16_t)port && urlEquals(observers[i].url, url) && tokenEquals(observers[i].token, observers[i].tokenlen, token, tokenlen))
        {
            observers[i].in_use = false;
            observers[i].tokenlen = 0;
            observers[i].observe_seq = 0;
            observers[i].last_seen_ms = 0;
            observers[i].url[0] = 0;
            removed = true;
        }
    }
    return removed;
}

int Coap::notify(const char *url, const char *payload, int payload_len, COAP_CONTENT_TYPE type)
{
    if (url == NULL)
        return 0;
    unsigned long now = millis();
    int sent = 0;

    for (int i = 0; i < COAP_MAX_OBSERVERS; i++)
    {
        if (!observers[i].in_use)
            continue;
        if (!urlEquals(observers[i].url, url))
            continue;

        if (COAP_OBSERVER_LEASE_MS > 0 && (now - observers[i].last_seen_ms) > COAP_OBSERVER_LEASE_MS)
        {
            observers[i].in_use = false;
            continue;
        }

        CoapPacket packet;
        packet.type = COAP_NONCON;
        packet.code = COAP_CONTENT;
        packet.token = observers[i].tokenlen ? observers[i].token : NULL;
        packet.tokenlen = observers[i].tokenlen;
        packet.payload = (uint8_t *)payload;
        packet.payloadlen = payload_len;
        packet.optionnum = 0;
        packet.messageid = rand();

        uint32_t observe_seq = ++observers[i].observe_seq;
        uint8_t observeBuf[3] = {0};
        uint8_t observeLen = encodeUintOption(observe_seq, observeBuf);
        packet.addOption(COAP_OBSERVE, observeLen, observeBuf);

        uint8_t optionBuffer[2] = {0};
        optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)type & 0x00FF);
        packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

        if (this->sendPacket(packet, observers[i].ip, observers[i].port) != 0)
            sent++;
    }
    return sent;
}
