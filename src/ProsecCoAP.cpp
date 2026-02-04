#include "Arduino.h"
#include "ProsecCoAP.h"

namespace Coap
{
    Message::Message(MessageType type, MessageCode code, uint16_t id)
    {
        // Initialize message with default CoAP header values.
        this->_message[0] = (COAP_VERSION << 6) | (static_cast<uint8_t>(type) << 4); // Version 1, given code, Token Length 0
        this->_message[1] = static_cast<uint8_t>(code);                              // Code data.
        this->_message[2] = (id >> 8) & 0xFF;                                        // Message ID high byte.
        this->_message[3] = id & 0xFF;                                               // Message ID low byte.
        this->_messageLength = COAP_HEADER_SIZE;                                     // Keep track of the current message size.
    }

    ErrorCode Message::fromUdp(UDP *udp, Message &message)
    {
        message._messageLength = 0; // Initialize to zero in case of failure.

        int len = udp->parsePacket();
        if (len <= COAP_HEADER_SIZE)
        {
            // The size is too small to be a valid CoAP message.
            // Nothing to read.
            return ErrorCode::NOT_FOUND;
        }
        if (static_cast<unsigned int>(len) > COAP_MAX_MESSAGE_SIZE) // We know that len is positive, so the cast is safe.
        {
            // Allocated buffer is not large enough. User may increase COAP_MAX_MESSAGE_SIZE.
            return ErrorCode::NOT_SUPPORTED;
        }
        // Read the packet into the message buffer.
        // The returned value is the actual number of bytes read.
        len = udp->read(message._message, len);
        if (len <= COAP_HEADER_SIZE)
        {
            // The minimum size must be the header size.
            // Error while reading from UDP.
            return ErrorCode::NETWORK;
        }
        // Set the message length.
        message._messageLength = len;
        // Check that the message start with the expected CoAP version.
        if (message.getVersion() != COAP_VERSION)
            return ErrorCode::NOT_SUPPORTED;
        return ErrorCode::NONE;
    }

    ErrorCode Message::buildResponse(const Message *request, MessageCode code, Message &response)
    {
        if (request->getLength() < COAP_HEADER_SIZE)
        {
            return ErrorCode::MALFORMED_MESSAGE;
        }
        // Initialize response message (version, type, code)
        // and use the message id from the request.
        response = Message(MessageType::ACK, code, request->getId());

        // The response length is currently just the header size.
        // If the request has a token, copy it and update the token length in the response.
        size_t tokenLength = request->getTokenLength();
        memcpy(response._message + COAP_HEADER_SIZE, // Destination.
               request->_message + COAP_HEADER_SIZE, // Source.
               tokenLength);                         // Length.
        // Set token length, stored in the 4 lower bits of the first byte.
        response._message[0] |= static_cast<uint8_t>(tokenLength) & 0x0F;
        // Update the total message length.
        response._messageLength = COAP_HEADER_SIZE + tokenLength;
        return ErrorCode::NONE;
    }

    uint16_t Message::getId() const
    {
        // 3rd and 4th bytes of the message MUST contain the Message ID.
        return (static_cast<uint16_t>(this->_message[2]) << 8) | static_cast<uint16_t>(this->_message[3]);
    }

    ErrorCode Message::_insert(size_t startPosition, const uint8_t *data, size_t length)
    {
        if (this->_messageLength + length > COAP_MAX_MESSAGE_SIZE)
        {
            // Insertion would exceed maximum message size.
            // Return without modification to the message.
            return ErrorCode::MESSAGE_TOO_LARGE;
        }

        // Shift existing bytes to make space for new data.
        memmove(this->_message + startPosition + length, // Destination.
                this->_message + startPosition,          // Source.
                this->_messageLength - startPosition);   // Amount of bytes to move (all the bytes after the insertion point).

        // Copy new data into the message.
        memcpy(this->_message + startPosition, data, length);

        // Update current message length.
        this->_messageLength += length;
        return ErrorCode::NONE;
    }

    ErrorCode Message::_remove(size_t startPosition, size_t length)
    {
        if (length == 0)
        {
            return ErrorCode::NONE; // Nothing to remove.
        }

        // Check if removal is valid.
        if (startPosition + length > this->_messageLength)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }

        // Shift rightmost bytes over the removed segment.
        // Even if we are removing the rightmost bytes, memmove with zero length is safe.
        memmove(this->_message + startPosition,                   // Destination.
                this->_message + startPosition + length,          // Source.
                this->_messageLength - (startPosition + length)); // Amount of bytes to shift.

        // Update current message length.
        this->_messageLength -= length;
        return ErrorCode::NONE;
    }

    uint16_t Message::_getNextId()
    {
        // Message ID is a simple sequential identifier.
        // However, to avoid collisions after a reset, start with a random value.
        static uint16_t id = (uint16_t)random(0, 0xFFFF);
        return id++;
    }

    void Message::setType(MessageType type)
    {
        // Clear the current type bits.
        this->_message[0] &= 0b11001111;
        // Set the new type.
        this->_message[0] |= (static_cast<uint8_t>(type) << 4) & 0b00110000;
    }

    MessageType Message::getType() const
    {
        // Extract the type bits from the first byte.
        uint8_t t = (this->_message[0] >> 4) & 0b00000011;
        return static_cast<MessageType>(t);
    }

    void Message::setCode(MessageCode code)
    {
        this->_message[1] = static_cast<uint8_t>(code);
    }

    MessageCode Message::getCode() const
    {
        return static_cast<MessageCode>(this->_message[1]);
    }

    size_t Message::getTokenLength() const
    {
        // Extract token length from the first byte of the message.
        return this->_message[0] & 0x0F;
    }

    ErrorCode Message::addToken(size_t length)
    {

        if (length > COAP_MAX_TOKEN_LENGTH)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }

        // SECTION Remove any existing token.
        size_t existingTokenLength = this->_message[0] & 0x0F; // Get existing token length.
        this->_remove(COAP_HEADER_SIZE, existingTokenLength);  // No-op if existingTokenLength is 0.

        // SECTION Generate a random token.
        uint8_t tokenBuffer[COAP_MAX_TOKEN_LENGTH];

        // Fill token buffer with random bytes, minimizing calls to random().
        // sizeof(long) is generally 4 bytes on Arduino platforms, however this is not guaranteed by the standard.
        // Each random() call only provides 31 random bits, so we can extract up to 3 fully random bytes per call.
        constexpr size_t chunkSize = sizeof(long) - 1; // Generally will be 4 - 1 = 3 bytes.
        constexpr long max = (1UL << (chunkSize * 8)); // Generally 16 777 216.
        for (size_t c = 0; c < length; c += chunkSize)
        {
            long r = random(0, max);
            size_t bytesToCopy = (length - c < chunkSize) ? (length - c) : chunkSize;
            memcpy(tokenBuffer + c, &r, bytesToCopy);
        }
        // !SECTION

        // Write the generated token into the message buffer and update overall message length.
        ErrorCode err = this->_insert(COAP_HEADER_SIZE, tokenBuffer, length);
        if (err != ErrorCode::NONE)
        {
            // Error. The message was not modified.
            return err;
        }
        // SECTION Update message header to reflect new token length.
        // The token length is stored in the 4 lower bits of the first byte.
        this->_message[0] |= static_cast<uint8_t>(length) & 0x0F;
        return ErrorCode::NONE;
    }

    ErrorCode Message::getToken(const uint8_t *&buffer, size_t &length) const
    {
        length = this->getTokenLength();
        if (length == 0)
        {
            // Special case: no token present. Still a valid message.
            buffer = nullptr;
            return ErrorCode::NONE;
        }
        if (length > COAP_MAX_TOKEN_LENGTH || this->_messageLength < COAP_HEADER_SIZE + length)
        {
            // The message is malformed: it indicates the presence of a token
            // whose length would be greater allowed or whose value will exceed the whole message size.
            length = 0;
            buffer = nullptr;
            return ErrorCode::MALFORMED_MESSAGE;
        }
        // Give access by reference to the token within the message.
        buffer = this->_message + COAP_HEADER_SIZE;
        return ErrorCode::NONE;
    }

    ErrorCode Message::addOption(OptionNumber newNumber, const uint8_t *newValue, size_t newLength)
    {
        OptionIterator it(this);
        Option opt;
        uint16_t lastOptionNumber = 0;
        size_t currentByte = it._currentByte;   // Next byte to read.
        while (it.next(opt) == ErrorCode::NONE) // Exit the loop if there are no more options.
        {
            if (opt.number <= newNumber)
            {
                // Keep track of the last option number.
                // This also handles the case of multiple options with the same number.
                lastOptionNumber = static_cast<uint16_t>(opt.number);
                currentByte = it._currentByte; // Update to the next byte to read.
            }
            else
            {
                // We found an option with a number greater than the new one.
                // The insertion point is before this option, at currentByte.
                break;
            }
        }

        // currentByte is now the insertion point for the new option.

        // Check for duplicate single-instance options.
        if (static_cast<OptionNumber>(lastOptionNumber) == newNumber)
        {
            // The last option number is the same as the new one.

            // For single-instance options, check if one already exists
            // and return ErrorCode::NOT_SUPPORTED if so.
            // See https://datatracker.ietf.org/doc/html/rfc7252#section-5.10
            switch (static_cast<OptionNumber>(lastOptionNumber))
            {
            case OptionNumber::URI_HOST:
            case OptionNumber::IF_NONE_MATCH:
            case OptionNumber::OBSERVE: // Observe can only appear once!
            case OptionNumber::URI_PORT:
            case OptionNumber::CONTENT_FORMAT:
            case OptionNumber::MAX_AGE:
            case OptionNumber::ACCEPT:
            case OptionNumber::PROXY_URI:
            case OptionNumber::PROXY_SCHEME:
            case OptionNumber::SIZE1:
                return ErrorCode::NOT_SUPPORTED;
            default:
                // Last option is 0 or is a multi-instance option, allowed to add.
                break;
            }
        }

        // Build the new option header.
        uint8_t newOptionHeader[5] = {0};                                              // Max 5 bytes for option header (1 + 2 for delta + 2 for length).
        size_t newOptionHeaderLength = 1;                                              // Initial header length.
        uint16_t newOptionDelta = static_cast<uint16_t>(newNumber) - lastOptionNumber; // This is guaranteed to be greater or equal to 0.
        if (newOptionDelta > 269)
        {
            // Extended delta (16 bits).
            newOptionHeader[1] = ((newOptionDelta - 269) >> 8) & 0xFF;
            newOptionHeader[2] = (newOptionDelta - 269) & 0xFF;
            newOptionHeader[0] = (14 << 4); // Delta = 14 on the 4 MSb.
            newOptionHeaderLength += 2;
        }
        else if (newOptionDelta > 12)
        {
            // Extended delta (8 bits).
            newOptionHeader[1] = (newOptionDelta - 13) & 0xFF;
            newOptionHeader[0] = (13 << 4); // Delta = 13 on the 4 MSb.
            newOptionHeaderLength += 1;
        }
        else
        {
            // No extended delta.
            newOptionHeader[0] = (newOptionDelta << 4);
        }

        // Now process the length.
        if (newLength > 269)
        {
            // Extended length (16 bits).
            newOptionHeader[0] |= 14; // Length = 14 on the 4 LSb.
            newOptionHeader[newOptionHeaderLength] = ((newLength - 269) >> 8) & 0xFF;
            newOptionHeader[newOptionHeaderLength + 1] = (newLength - 269) & 0xFF;
            newOptionHeaderLength += 2;
        }
        else if (newLength > 12)
        {
            // Extended length (8 bits).
            newOptionHeader[0] |= 13; // Length = 13 on the 4 LSb.
            newOptionHeader[newOptionHeaderLength] = (newLength - 13) & 0xFF;
            newOptionHeaderLength += 1;
        }
        else
        {
            // No extended length.
            newOptionHeader[0] |= newLength;
        }

        // Write the header into the message buffer.
        ErrorCode err = this->_insert(currentByte, newOptionHeader, newOptionHeaderLength);
        if (err != ErrorCode::NONE)
        {
            return err; // Insertion failed, message unmodified.
        }
        // Write the value into the message buffer.
        currentByte += newOptionHeaderLength;
        err = this->_insert(currentByte, newValue, newLength);
        if (err != ErrorCode::NONE)
        {
            // Value insertion failed. Remove the previously inserted header.
            // REVIEW: If this fails, there is not much we can do... Message will be corrupted.
            if (this->_remove(currentByte - newOptionHeaderLength, newOptionHeaderLength) != ErrorCode::NONE)
            {
                // Serious error: message is now corrupted.
                return ErrorCode::MALFORMED_MESSAGE;
            };
            return err; // Return the original error.
        }

        // Option added successfully.
        // Update the following option delta value. The following delta must be adjusted
        // and it may only stay the same or decrease, never increase.

        // Move to the beginning of the next option, if present. Will exit if end of message reached.
        currentByte += newLength;
        if (currentByte >= this->_messageLength)
        {
            return ErrorCode::NONE; // Message ends here. No following option to adjust.
        }

        // The following option delta must be adjusted by *subtracting* the delta
        // added to the new option.
        if (newOptionDelta == 0)
        {
            return ErrorCode::NONE; // No adjustment needed. All good.
        }

        // SECTION Read the next option and adjust its delta.
        uint8_t optionHeader = this->_message[currentByte];
        currentByte++; // Move to the next byte (which may be extended delta/length or value start).
        uint16_t oldDelta = (optionHeader >> 4) & 0x0F;
        uint16_t newDelta = oldDelta - newOptionDelta;
        uint16_t length = optionHeader & 0x0F;
        if ((oldDelta < 13) & (newDelta < 13))
        {
            // Easy case: both old and new delta fit in 4 bits.
            this->_message[currentByte - 1] = (static_cast<uint8_t>(newDelta) << 4) | (optionHeader & 0x0F);
            return ErrorCode::NONE;
        }
        else
        {
            if (oldDelta == 13)
            {
                // Old delta was extended delta (8 bits).
                if (newDelta < 13)
                {
                    // The new delta fits in 4 bits.
                    // Remove the extended delta byte.
                    this->_message[currentByte - 1] = (static_cast<uint8_t>(newDelta) << 4) | (optionHeader & 0x0F);
                    // Remove the extended delta byte and return.
                    return this->_remove(currentByte, 1);
                }
                else
                {
                    // Since the new delta is still extended, just update the extended byte.
                    // Note that newDelta must be smaller of the old one. So this is safe.
                    this->_message[currentByte] = static_cast<uint8_t>(newDelta - 13);
                }
            }
            else if (oldDelta == 14)
            {
                // Old delta was extended delta (16 bits).
                if (newDelta < 13)
                {
                    // The new delta fits in 4 bits.
                    this->_message[currentByte - 1] = (static_cast<uint8_t>(newDelta) << 4) | (optionHeader & 0x0F);
                    // Remove the two extended delta bytes.
                    return this->_remove(currentByte, 2);
                }
                else if (newDelta < 269)
                {
                    // The new delta fits in extended delta (8 bits).
                    this->_message[currentByte - 1] = (13 << 4) | (optionHeader & 0x0F); // Set delta = 13.
                    this->_message[currentByte] = static_cast<uint8_t>(newDelta - 13);
                    // Remove the second of the two extended delta bytes.
                    return this->_remove(currentByte + 1, 1);
                }
                else
                {
                    // The new delta still needs extended delta (16 bits).
                    // Just update the two extended delta bytes.
                    uint16_t newDeltaOffset = newDelta - 269;
                    this->_message[currentByte] = (newDeltaOffset >> 8) & 0xFF;
                    this->_message[currentByte + 1] = newDeltaOffset & 0xFF;
                    return ErrorCode::NONE;
                }
            }
            else if (oldDelta == 15)
            {
                // Old delta is special case: payload marker.
                if (length == 15)
                {
                    // Payload marker reached. End of options.
                    return ErrorCode::NONE;
                }
                else
                {
                    return ErrorCode::MALFORMED_MESSAGE;
                }
            }
        }
        // !SECTION End of delta adjustment.

        // Successfully added option and adjusted the following one.
        return ErrorCode::NONE;
    }

    OptionIterator::OptionIterator(const Message *message)
    {
        _message = message;
        _currentByte = COAP_HEADER_SIZE + _message->getTokenLength();
        _currentOptionNumber = 0;
    }

    OptionIterator Message::getOptionIterator() const
    {
        // Return an option iterator for this message.
        return OptionIterator(this);
    }

    ErrorCode Message::addHost(IPAddress ip)
    {
        // Convert IPv4 address to string.
        // Note that IPAddress is IPv4.
        // See https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/IPAddress.h
        char ipAsString[16] = ""; // Max length of an IP as string is 15 (including dots) + null terminator.
        sprintf(ipAsString, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        // Add it as Uri-Host option.
        // Given the limitation above, we are sure that the option is in the 1-255 range
        // as per https://datatracker.ietf.org/doc/html/rfc7252#section-5.10
        return this->addOption(OptionNumber::URI_HOST, reinterpret_cast<const uint8_t *>(ipAsString), strlen(ipAsString));
    }

    ErrorCode Message::addPort(uint16_t port)
    {
        // Uri-Port option value is the port number as an unsigned integer.
        // We use the minimal representation (1 or 2 bytes).
        size_t length = (port > 255) ? 2 : 1;
        // Add it as Uri-Port option.
        return this->addOption(OptionNumber::URI_PORT, reinterpret_cast<const uint8_t *>(&port), length);
    }

    ErrorCode Message::addPath(const char *path)
    {
        if (path == nullptr || path[0] == '\0')
        {
            // Empty path is invalid.
            // Although it can technically be represented as an empty Uri-Path option,
            // it makes little sense to do so.
            return ErrorCode::INVALID_ARGUMENT;
        }
        bool hasQuery = false;
        size_t i = 0;

        // Iterate through the whole path string, splitting on '/', '?' and '&'.
        while (true)
        {
            size_t segmentStart = i;
            // Find the end of the current segment.
            while (path[i] != '/' && path[i] != '?' && path[i] != '\0' && path[i] != '&')
            {
                i++;
            }
            if (!hasQuery && path[i] == '&')
            {
                // '&' is only valid within the query part.
                return ErrorCode::INVALID_ARGUMENT;
            }
            size_t segmentLength = i - segmentStart;
            if (segmentLength > 0)
            {
                if (segmentLength > 255)
                {
                    // Segment too long to be represented as a single option.
                    // Refer to https://datatracker.ietf.org/doc/html/rfc7252#section-5.10
                    return ErrorCode::INVALID_ARGUMENT;
                }
                // Add the segment as Uri-Path or Uri-Query option.
                OptionNumber optionNumber = hasQuery ? OptionNumber::URI_QUERY : OptionNumber::URI_PATH;
                ErrorCode err = this->addOption(optionNumber,
                                                reinterpret_cast<const uint8_t *>(path + segmentStart),
                                                segmentLength);
                if (err != ErrorCode::NONE)
                {
                    // Failed to add option. Block the operation.
                    // Possibly the message will be malformed!
                    return err;
                }
            }
            // Check if we reached a query part.
            if (path[i] == '?')
            {
                hasQuery = true;
            }
            // If the last character was the string terminator, we are done.
            if (path[i] == '\0')
                return ErrorCode::NONE;
            // Else, skip the '/', '?' or '&'.
            i++;
        };
    }

    ErrorCode OptionIterator::next(Option &option)
    {
        // There are no more options if, either:
        // - We reached the end of the message.
        // - We reached the payload marker (0xFF), see below.
        if (this->_currentByte >= this->_message->getLength())
        {
            // No more options.
            return ErrorCode::NOT_FOUND;
        }
        const uint8_t *messageRaw = this->_message->asRaw();

        // See https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
        // Read the option header byte.
        uint8_t optionHeader = messageRaw[this->_currentByte];
        this->_currentByte++; // Points to next byte to read.
        uint16_t delta = (optionHeader >> 4) & 0x0F;
        option.length = optionHeader & 0x0F;

        // SECTION Process the delta special cases.
        if (delta == 13)
        {
            // Extended delta (8 bits).
            if (this->_message->getLength() < this->_currentByte + 1)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            delta = messageRaw[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (delta == 14)
        {
            // Extended delta (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            delta = ((static_cast<uint16_t>(messageRaw[this->_currentByte]) << 8) |
                     static_cast<uint16_t>(messageRaw[this->_currentByte + 1])) +
                    269;
            this->_currentByte += 2;
        }
        else if (delta == 15)
        {
            if (option.length == 15)
            {
                // Payload marker reached. End of options.
                return ErrorCode::NOT_FOUND;
            }
            else
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
        }
        // !SECTION End of delta processing.

        // SECTION Process the length special cases.
        if (option.length == 13)
        {
            // Extended length (8 bits).
            if (this->_message->getLength() < this->_currentByte)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            option.length = messageRaw[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (option.length == 14)
        {
            // Extended length (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            option.length = ((static_cast<uint16_t>(messageRaw[this->_currentByte]) << 8) |
                             static_cast<uint16_t>(messageRaw[this->_currentByte + 1])) +
                            269;
            this->_currentByte += 2;
        }
        else if (option.length == 15)
        {
            return ErrorCode::MALFORMED_MESSAGE;
        }
        // !SECTION End of length processing.

        if ((this->_message)->getLength() < this->_currentByte + option.length)
        {
            // The size of the option value exceeds the message size!
            return ErrorCode::MALFORMED_MESSAGE;
        }

        // Current option number is previous plus delta. Delta may be zero too.
        this->_currentOptionNumber += delta;
        option.number = static_cast<OptionNumber>(this->_currentOptionNumber);

        // The option value starts after delta and length bytes.
        option.value = &messageRaw[this->_currentByte];
        this->_currentByte += option.length; // Update for next call.

        return ErrorCode::NONE;
    }

    ErrorCode Message::getPayload(const uint8_t *&payload, size_t &length) const
    {
        // Find the payload marker (0xFF), if present.
        OptionIterator it = this->getOptionIterator();
        ErrorCode err;
        Option opt;
        while ((err = it.next(opt)) == ErrorCode::NONE)
        {
            // Just iterate to find the payload marker.
            // This loop will exit if:
            // - We reach the end of options or a payload marker (err == NotFound).
            // - We find a malformed option (err == MalformedMessage).
        }
        if (err != ErrorCode::NOT_FOUND)
        {
            // Something went wrong during iteration, possibly malformed message.
            return err;
        }
        // The current byte points to the byte after the payload marker or the end of message.
        // See next() for details.
        size_t firstByteOfPayload = it._currentByte - 1; // Go back to the payload marker.
        if (firstByteOfPayload >= this->getLength())
        {
            // Message ends here. No payload present.
            return ErrorCode::NOT_FOUND;
        }
        if (this->_message[firstByteOfPayload] != COAP_PAYLOAD_MARKER)
        {
            // Payload marker not present.
            return ErrorCode::NOT_FOUND;
        }
        // Payload found. Share access to it.
        // Payload starts after the payload marker.
        payload = this->_message + firstByteOfPayload + 1;
        length = this->getLength() - (firstByteOfPayload + 1);
        return ErrorCode::NONE;
    }

    ErrorCode Message::addPayload(const uint8_t *payload, size_t length)
    {
        if (length == 0 || payload == nullptr)
        {
            // No payload to add.
            return ErrorCode::INVALID_ARGUMENT;
        }
        // Check if there is already a payload by iterating through the options.
        // If we find the payload marker (0xFF), there is already a payload.
        OptionIterator it = this->getOptionIterator();
        ErrorCode err;
        Option opt;
        while ((err = it.next(opt)) == ErrorCode::NONE)
        {
            // Just iterate to find the payload marker.
            // This loop will exit if:
            // - We reach the end of options or a payload marker (err == NotFound).
            // - We find a malformed option (err == MalformedMessage).
        }
        if (err != ErrorCode::NOT_FOUND)
        {
            // Something went wrong during iteration, possibly malformed message.
            return err;
        }

        // The current byte points to the payload marker or the end of message.
        size_t firstByteOfPayload = it._currentByte;

        if (firstByteOfPayload < this->getLength() && this->_message[firstByteOfPayload] == COAP_PAYLOAD_MARKER)
        {
            // Payload marker already present. Cannot add another payload.
            return ErrorCode::NOT_SUPPORTED;
        }
        // We need to add the payload. We'll need length + 1 bytes (including the payload marker).
        if (this->getLength() + 1 + length > COAP_MAX_MESSAGE_SIZE)
        {
            // Adding the payload would exceed maximum message size.
            return ErrorCode::MESSAGE_TOO_LARGE;
        }
        // All good. Insert the payload marker.
        err = this->_insert(firstByteOfPayload, reinterpret_cast<const uint8_t *>(&COAP_PAYLOAD_MARKER), 1);
        if (err != ErrorCode::NONE)
        {
            return err;
        }
        // Insert the payload itself.
        err = this->_insert(firstByteOfPayload + 1, payload, length);
        if (err != ErrorCode::NONE)
        {
            // Try to cancel the previous insertion of the payload marker.
            this->_remove(firstByteOfPayload, 1);
            // Return the original error.
            return err;
        }
        return ErrorCode::NONE;
    }

    ErrorCode Message::addPayload(const uint8_t *payload, size_t length, ContentFormat format)
    {
        // NOTE: A content format of 0 means "text/plain; charset=utf-8".
        // See https://datatracker.ietf.org/doc/html/rfc7252#section-12.3
        // Therefore, it is a valid content format and needs to be added as an option.
        ErrorCode err;
        err = this->addOption(OptionNumber::CONTENT_FORMAT,
                              reinterpret_cast<const uint8_t *>(&format),
                              (static_cast<uint16_t>(format) > 255) ? 2 : 1); // Use minimal representation.
        if (err != ErrorCode::NONE)
        {
            return err;
        }

        return this->addPayload(payload, length);
    }

    void Detail::RetransmissionEntry::set(Coap::Message message, IPAddress ip, uint16_t port)
    {
        this->message = message;
        this->attempts = 0; // Mark as valid entry.
        this->timeoutBaseInterval = Detail::getRandomTimeout();
        this->nextAttemptDeadline = millis() + this->timeoutBaseInterval;
        this->ip = ip;
        this->port = port;
    }

    ErrorCode Detail::RetransmissionQueue::add(const Message &message, IPAddress ip, uint16_t port)
    {
        // Find an empty slot in the retransmission queue.
        for (size_t i = 0; i < COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE; i++)
        {
            if (this->_entries[i].isEmpty())
            {
                // Empty slot found. Add the new entry here.
                this->_entries[i].set(message, ip, port);
                return ErrorCode::NONE;
            }
        }
        // No empty slot found. Queue is full.
        return ErrorCode::NOT_SUPPORTED;
    }

    ErrorCode Detail::RetransmissionQueue::process(UDP *udp)
    {
        unsigned long now = millis();
        for (size_t i = 0; i < COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE; i++)
        {
            if (this->_entries[i].isEmpty())
            {
                // This entry is either empty or has exhausted its attempts.
                continue;
            }
            else if (static_cast<long>(now - this->_entries[i].getDeadline()) >= 0) // This handles millis() overflow correctly.
            {
                // Time to retransmit this message.
                // NOTE: We don't handle errors here. If sending fails, we just try again later.
                this->_entries[i].retransmit(udp);
            }
        }
        return ErrorCode::NONE;
    }

    void Detail::RetransmissionQueue::matchResponse(const Message &response)
    {
        if (response.getType() != MessageType::ACK &&
            response.getType() != MessageType::RST)
        {
            // Not an acknowledgment or reset message. Ignore.
            return;
        }
        // The ID is used to match the response to the request.
        uint16_t responseId = response.getId();
        for (size_t i = 0; i < COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE; i++)
        {
            if (this->_entries[i].isEmpty())
            {
                // Empty slot. Ignore.
                continue;
            }
            if (this->_entries[i].message.getId() == responseId)
            {
                // Match found. Set this entry as completed.
                this->_entries[i].setAsCompleted();
                return; // Exit after first match.
            }
        }
    }

    ErrorCode Detail::RetransmissionEntry::retransmit(UDP *udp)
    {
        // Call the low-level send function.
        ErrorCode err = Detail::sendUdp(udp, this->message.asRaw(), this->message.getLength(), this->ip, this->port);
        if (err != ErrorCode::NONE)
        {
            return err;
        }
        this->attempts++;
        // Set the next attempt deadline using exponential backoff.
        this->nextAttemptDeadline = millis() + (this->timeoutBaseInterval << this->attempts);
        return ErrorCode::NONE;
    }

    void Observer::setAsSeen(unsigned long currentTimeMs)
    {
        this->_lastSeenMs = currentTimeMs;
    }

    void Observer::setAsSeen()
    {
        this->_lastSeenMs = millis();
    }

    ErrorCode Node::serve(const char *path, Callback callback)
    {
        return this->_serverRegistry.add(path, callback);
    }

    ErrorCode Node::start()
    {
        // begin() returns 1 on success, 0 on failure.
        if (this->_udp->begin(this->_port) == 1)
        {
            return ErrorCode::NONE;
        }
        else
        {
            return ErrorCode::NETWORK;
        }
    }

    ErrorCode Node::loop()
    {
        // SECTION Server mode: process incoming packets.
        ErrorCode err;
        {                            // Reducing scope of incomingMessage and uriPath.
            Message incomingMessage; // Will be populated by fromUdp().
            String uriPath;
            uriPath.reserve(32); // Pre-allocate some space to reduce dynamic allocations. If you use long path, you obviously don't care.

            // fromUdp() returns ErrorCode::NONE while there are incoming messages.
            while ((err = Message::fromUdp(this->_udp, incomingMessage)) == ErrorCode::NONE)
            {
                // Empty URI path by default.
                uriPath = "";
                // Build the URI path from the Uri-Path option(s), if present.
                OptionIterator it = incomingMessage.getOptionIterator();
                Option opt;
                while (it.next(opt) == ErrorCode::NONE)
                {
                    // Options must be in number order.
                    if (opt.number < OptionNumber::URI_PATH)
                    {
                        continue;
                    }
                    else if (opt.number == OptionNumber::URI_PATH)
                    {
                        // Append '/' if uriPath is not empty.
                        if (uriPath.length() > 0)
                        {
                            uriPath += '/';
                        }
                        // Append the option value as a string.
                        for (size_t i = 0; i < opt.length; i++)
                        {
                            uriPath += static_cast<char>(opt.value[i]);
                        }
                    }
                    else
                    {
                        // No more Uri-Path options.
                        break;
                    }
                }

                MessageCode code = incomingMessage.getCode();

                if (code == MessageCode::GET ||
                    code == MessageCode::POST ||
                    code == MessageCode::PUT ||
                    code == MessageCode::DELETE)
                {
                    // ANCHOR This is a request message.
                    // https://datatracker.ietf.org/doc/html/rfc7252#section-5.1

                    // Match the message URI to the registered handlers.
                    // uriPath.c_str() will give the C-style string pointer.
                    Callback handler;
                    err = this->_serverRegistry.find(uriPath.c_str(), handler);
                    if (err != ErrorCode::NONE)
                    {
                        // No handler found for this path.
                        continue; // Ignore the message.
                    }
                    // Handler found. Call it.
                    handler(incomingMessage, (this->_udp)->remoteIP(), (this->_udp)->remotePort());
                }
                else
                {
                    // ANCHOR This is a response message.
                    // As per specs, "A response is identified by the Code field in the CoAP header being
                    // set to a Response Code."

                    // Match the response to an outstanding request in the retransmission queue, if present.
                    // This will match ACK or RST messages to their corresponding CON requests.
                    this->_retransmissionQueue.matchResponse(incomingMessage);

                    if (this->_responseHandler != nullptr)
                    {
                        // A response handler was set. Call it.
                        this->_responseHandler(incomingMessage,
                                               (this->_udp)->remoteIP(),
                                               (this->_udp)->remotePort());
                    }
                }
            }
            // err will be ErrorCode::NOT_FOUND if there are no more messages to read.
            // REVIEW: err may also be another error code if something went wrong.
        }

        // !SECTION End of server mode.

        // SECTION Client mode: process retransmission of outgoing messages.
        this->_retransmissionQueue.process(this->_udp);
        // !SECTION End of client mode.

        return ErrorCode::NONE;
    }

    ErrorCode Node::sendMessage(const Message &message, IPAddress ip, uint16_t port)
    {
        if (message.getType() == MessageType::CON)
        {
            this->_retransmissionQueue.add(message, ip, port);
        }
        return Detail::sendUdp(this->_udp, message.asRaw(), message.getLength(), ip, port);
    }
}