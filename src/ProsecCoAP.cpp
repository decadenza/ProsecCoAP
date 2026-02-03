#include "Arduino.h"
#include "ProsecCoAP.h"
#include "utility/helpers.h"

namespace Coap
{
    Message::Message(MessageType type, MessageCode code)
    {
        // Initialize message with default CoAP header values.
        this->_message[0] = (COAP_VERSION << 6) | (static_cast<uint8_t>(type) << 4); // Version 1, given code, Token Length 0
        this->_message[1] = static_cast<uint8_t>(code);                              // Code data.
        uint16_t messageId = _getNextId();
        this->_message[2] = (messageId >> 8) & 0xFF; // Message ID high byte.
        this->_message[3] = messageId & 0xFF;        // Message ID low byte.
        this->_messageLength = COAP_HEADER_SIZE;     // Keep track of the current message size.
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
            return ErrorCode::MessageTooLarge;
        }

        // Shift existing bytes to make space for new data.
        memmove(this->_message + startPosition + length, // Destination.
                this->_message + startPosition,          // Source.
                this->_messageLength - startPosition);   // Amount of bytes to move (all the bytes after the insertion point).

        // Copy new data into the message.
        memcpy(this->_message + startPosition, data, length);

        // Update current message length.
        this->_messageLength += length;
        return ErrorCode::None;
    }

    ErrorCode Message::_remove(size_t startPosition, size_t length)
    {
        if (length == 0)
        {
            return ErrorCode::None; // Nothing to remove.
        }

        // Check if removal is valid.
        if (startPosition + length > this->_messageLength)
        {
            return ErrorCode::InvalidArgument;
        }

        // Shift rightmost bytes over the removed segment.
        // Even if we are removing the rightmost bytes, memmove with zero length is safe.
        memmove(this->_message + startPosition,                   // Destination.
                this->_message + startPosition + length,          // Source.
                this->_messageLength - (startPosition + length)); // Amount of bytes to shift.

        // Update current message length.
        this->_messageLength -= length;
        return ErrorCode::None;
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
            return ErrorCode::InvalidArgument;
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
        if (err != ErrorCode::None)
        {
            // Error. The message was not modified.
            return err;
        }
        // SECTION Update message header to reflect new token length.
        // The token length is stored in the 4 lower bits of the first byte.
        this->_message[0] |= static_cast<uint8_t>(length) & 0x0F;
        return ErrorCode::None;
    }

    ErrorCode Message::getToken(const uint8_t *&buffer, size_t &length) const
    {
        length = this->getTokenLength();
        if (length == 0)
        {
            // Special case: no token present. Still a valid message.
            buffer = nullptr;
            return ErrorCode::None;
        }
        if (length > COAP_MAX_TOKEN_LENGTH || this->_messageLength < COAP_HEADER_SIZE + length)
        {
            // The message is malformed: it indicates the presence of a token
            // whose length would be greater allowed or whose value will exceed the whole message size.
            length = 0;
            buffer = nullptr;
            return ErrorCode::MalformedMessage;
        }
        // Give access by reference to the token within the message.
        buffer = this->_message + COAP_HEADER_SIZE;
        return ErrorCode::None;
    }

    ErrorCode Message::addOption(OptionNumber newNumber, const uint8_t *newValue, size_t newLength)
    {
        // Read the token length.
        size_t tokenLength = this->getTokenLength();
        // Go to HEADER_SIZE + token length to find the start of the options.
        size_t currentByte = COAP_HEADER_SIZE + tokenLength; // Points to the next byte to read.
        // Options are stored in order of increasing option number.
        // Iterate through existing options to find the insertion point.
        // In case of multiple options with the same number, the new option is added after existing ones.
        uint16_t lastOptionNumber = 0;
        while (currentByte < this->_messageLength)
        {
            // Read the option header byte.
            uint8_t optionHeader = this->_message[currentByte];
            currentByte++;                               // Move to the next byte (which may be extended delta/length or value start).
            uint16_t delta = (optionHeader >> 4) & 0x0F; // REVIEW: The protocol leaves the door open to larger deltas, but highly unlikely to happen.
            size_t length = optionHeader & 0x0F;

            // SECTION Process the delta special cases.
            if (delta == 13)
            {
                // Extended delta (8 bits).
                if (this->_messageLength < currentByte + 1)
                {
                    return ErrorCode::MalformedMessage;
                }
                delta = this->_message[currentByte] + 13;
                currentByte++;
            }
            else if (delta == 14)
            {
                // Extended delta (16 bits).
                if (this->_messageLength < currentByte + 2)
                {
                    return ErrorCode::MalformedMessage;
                }
                delta = ((static_cast<uint16_t>(this->_message[currentByte]) << 8) |
                         static_cast<uint16_t>(this->_message[currentByte + 1])) +
                        269;
                currentByte += 2;
            }
            else if (delta == 15)
            {
                if (length == 15)
                {
                    // Payload marker reached. End of options.
                    return ErrorCode::None;
                }
                else
                {
                    return ErrorCode::MalformedMessage;
                }
            }

            // If we reached an option number greater than the new one, we found the insertion point.
            if (lastOptionNumber + delta > static_cast<uint16_t>(newNumber))
            {
                // Move back to the beginning of this option for insertion.
                currentByte--;
                // Move back over any extended delta/length bytes.
                if (delta == 13)
                {
                    currentByte--;
                }
                else if (delta == 14)
                {
                    currentByte -= 2;
                }
                break; // Insertion point found.
            }
            else
            {
                // Store the last option number for next round.
                // This also handles the case of multiple options with the same number.
                lastOptionNumber += delta;
            }
            // !SECTION End of delta processing.

            // SECTION Process the length special cases.
            if (length == 13)
            {
                // Extended length (8 bits).
                if (this->_messageLength < currentByte + 1)
                {
                    return ErrorCode::MalformedMessage;
                }
                length = this->_message[currentByte] + 13;
                currentByte++;
            }
            else if (length == 14)
            {
                // Extended length (16 bits).
                if (this->_messageLength < currentByte + 2)
                {
                    return ErrorCode::MalformedMessage;
                }
                length = ((static_cast<uint16_t>(this->_message[currentByte]) << 8) |
                          static_cast<uint16_t>(this->_message[currentByte + 1])) +
                         269;
                currentByte += 2;
            }
            else if (length == 15)
            {
                return ErrorCode::MalformedMessage;
            }
            // !SECTION End of length processing.
            // Move to the beginning of the next option (or to the insertion point).
            currentByte += length;
        }
        // currentByte is now the insertion point for the new option.

        if (static_cast<OptionNumber>(lastOptionNumber) == newNumber)
        {
            // The last option number is the same as the new one.

            // For single-instance options, check if one already exists
            // and return ErrorCode::NotSupported if so.
            // See https://datatracker.ietf.org/doc/html/rfc7252#section-5.10
            switch (static_cast<OptionNumber>(lastOptionNumber))
            {
            case OptionNumber::UriHost:
            case OptionNumber::IfNoneMatch:
            case OptionNumber::Observe: // Observe can only appear once!
            case OptionNumber::UriPort:
            case OptionNumber::ContentFormat:
            case OptionNumber::MaxAge:
            case OptionNumber::Accept:
            case OptionNumber::ProxyUri:
            case OptionNumber::ProxyScheme:
            case OptionNumber::Size1:
                return ErrorCode::NotSupported;
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
        if (err != ErrorCode::None)
        {
            return err; // Insertion failed, message unmodified.
        }
        // Write the value into the message buffer.
        currentByte += newOptionHeaderLength;
        err = this->_insert(currentByte, newValue, newLength);
        if (err != ErrorCode::None)
        {
            // Value insertion failed. Remove the previously inserted header.
            // REVIEW: If this fails, there is not much we can do... Message will be corrupted.
            if (this->_remove(currentByte - newOptionHeaderLength, newOptionHeaderLength) != ErrorCode::None)
            {
                // Serious error: message is now corrupted.
                return ErrorCode::MalformedMessage;
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
            return ErrorCode::None; // Message ends here. No following option to adjust.
        }

        // The following option delta must be adjusted by *subtracting* the delta
        // added to the new option.
        if (newOptionDelta == 0)
        {
            return ErrorCode::None; // No adjustment needed. All good.
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
            return ErrorCode::None;
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
                    return ErrorCode::None;
                }
            }
            else if (oldDelta == 15)
            {
                // Old delta is special case: payload marker.
                if (length == 15)
                {
                    // Payload marker reached. End of options.
                    return ErrorCode::None;
                }
                else
                {
                    return ErrorCode::MalformedMessage;
                }
            }
        }
        // !SECTION End of delta adjustment.

        // Successfully added option and adjusted the following one.
        return ErrorCode::None;
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
        return this->addOption(OptionNumber::UriHost, reinterpret_cast<const uint8_t *>(ipAsString), strlen(ipAsString));
    }

    ErrorCode Message::addPort(uint16_t port)
    {
        // Uri-Port option value is the port number as an unsigned integer.
        // We use the minimal representation (1 or 2 bytes).
        size_t length = (port > 255) ? 2 : 1;
        // Add it as Uri-Port option.
        return this->addOption(OptionNumber::UriPort, reinterpret_cast<const uint8_t *>(&port), length);
    }

    ErrorCode Message::addPath(const char *path)
    {
        if (path == nullptr || path[0] == '\0')
        {
            // Empty path is invalid.
            // Although it can technically be represented as an empty Uri-Path option,
            // it makes little sense to do so.
            return ErrorCode::InvalidArgument;
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
                return ErrorCode::InvalidArgument;
            }
            size_t segmentLength = i - segmentStart;
            if (segmentLength > 0)
            {
                if (segmentLength > 255)
                {
                    // Segment too long to be represented as a single option.
                    // Refer to https://datatracker.ietf.org/doc/html/rfc7252#section-5.10
                    return ErrorCode::InvalidArgument;
                }
                // Add the segment as Uri-Path or Uri-Query option.
                OptionNumber optionNumber = hasQuery ? OptionNumber::UriQuery : OptionNumber::UriPath;
                ErrorCode err = this->addOption(optionNumber,
                                                reinterpret_cast<const uint8_t *>(path + segmentStart),
                                                segmentLength);
                if (err != ErrorCode::None)
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
                return ErrorCode::None;
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
            return ErrorCode::NotFound;
        }
        // See https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
        // Read the option header byte.
        uint8_t optionHeader = (this->_message)->_message[this->_currentByte];
        this->_currentByte++; // Points to next byte to read.
        uint16_t delta = (optionHeader >> 4) & 0x0F;
        option.length = optionHeader & 0x0F;

        // SECTION Process the delta special cases.
        if (delta == 13)
        {
            // Extended delta (8 bits).
            if (this->_message->getLength() < this->_currentByte + 1)
            {
                return ErrorCode::MalformedMessage;
            }
            delta = (this->_message)->_message[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (delta == 14)
        {
            // Extended delta (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MalformedMessage;
            }
            delta = ((static_cast<uint16_t>((this->_message)->_message[this->_currentByte]) << 8) |
                     static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 1])) +
                    269;
            this->_currentByte += 2;
        }
        else if (delta == 15)
        {
            if (option.length == 15)
            {
                // Payload marker reached. End of options.
                return ErrorCode::NotFound;
            }
            else
            {
                return ErrorCode::MalformedMessage;
            }
        }
        // !SECTION End of delta processing.

        // SECTION Process the length special cases.
        if (option.length == 13)
        {
            // Extended length (8 bits).
            if (this->_message->getLength() < this->_currentByte)
            {
                return ErrorCode::MalformedMessage;
            }
            option.length = (this->_message)->_message[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (option.length == 14)
        {
            // Extended length (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MalformedMessage;
            }
            option.length = ((static_cast<uint16_t>((this->_message)->_message[this->_currentByte]) << 8) |
                             static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 1])) +
                            269;
            this->_currentByte += 2;
        }
        else if (option.length == 15)
        {
            return ErrorCode::MalformedMessage;
        }
        // !SECTION End of length processing.

        if ((this->_message)->getLength() < this->_currentByte + option.length)
        {
            // The size of the option value exceeds the message size!
            return ErrorCode::MalformedMessage;
        }

        // Current option number is previous plus delta. Delta may be zero too.
        this->_currentOptionNumber += delta;
        option.number = static_cast<OptionNumber>(this->_currentOptionNumber);

        // The option value starts after delta and length bytes.
        option.value = &(this->_message)->_message[this->_currentByte];
        this->_currentByte += option.length; // Update for next call.

        return ErrorCode::None;
    }

    ErrorCode Message::getPayload(const uint8_t *&payload, size_t &length) const
    {
        // Find the payload marker (0xFF), if present.
        OptionIterator it = this->getOptionIterator();
        ErrorCode err;
        Option opt;
        while ((err = it.next(opt)) == ErrorCode::None)
        {
            // Just iterate to find the payload marker.
            // This loop will exit if:
            // - We reach the end of options or a payload marker (err == NotFound).
            // - We find a malformed option (err == MalformedMessage).
        }
        if (err != ErrorCode::NotFound)
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
            return ErrorCode::NotFound;
        }
        if (this->_message[firstByteOfPayload] != COAP_PAYLOAD_MARKER)
        {
            // Payload marker not present.
            return ErrorCode::NotFound;
        }
        // Payload found. Share access to it.
        // Payload starts after the payload marker.
        payload = this->_message + firstByteOfPayload + 1;
        length = this->getLength() - (firstByteOfPayload + 1);
        return ErrorCode::None;
    }

    ErrorCode Message::addPayload(const uint8_t *payload, size_t length)
    {
        if (length == 0 || payload == nullptr)
        {
            // No payload to add.
            return ErrorCode::InvalidArgument;
        }
        // Check if there is already a payload by iterating through the options.
        // If we find the payload marker (0xFF), there is already a payload.
        OptionIterator it = this->getOptionIterator();
        ErrorCode err;
        Option opt;
        while ((err = it.next(opt)) == ErrorCode::None)
        {
            // Just iterate to find the payload marker.
            // This loop will exit if:
            // - We reach the end of options or a payload marker (err == NotFound).
            // - We find a malformed option (err == MalformedMessage).
        }
        if (err != ErrorCode::NotFound)
        {
            // Something went wrong during iteration, possibly malformed message.
            return err;
        }

        // The current byte points to the payload marker or the end of message.
        size_t firstByteOfPayload = it._currentByte;

        if (firstByteOfPayload < this->getLength() && this->_message[firstByteOfPayload] == COAP_PAYLOAD_MARKER)
        {
            // Payload marker already present. Cannot add another payload.
            return ErrorCode::NotSupported;
        }
        // We need to add the payload. We'll need length + 1 bytes (including the payload marker).
        if (this->getLength() + 1 + length > COAP_MAX_MESSAGE_SIZE)
        {
            // Adding the payload would exceed maximum message size.
            return ErrorCode::MessageTooLarge;
        }
        // All good. Insert the payload marker.
        err = this->_insert(firstByteOfPayload, reinterpret_cast<const uint8_t *>(&COAP_PAYLOAD_MARKER), 1);
        if (err != ErrorCode::None)
        {
            return err;
        }
        // Insert the payload itself.
        err = this->_insert(firstByteOfPayload + 1, payload, length);
        if (err != ErrorCode::None)
        {
            // Try to cancel the previous insertion of the payload marker.
            this->_remove(firstByteOfPayload, 1);
            // Return the original error.
            return err;
        }
        return ErrorCode::None;
    }

    ErrorCode Message::addPayload(const uint8_t *payload, size_t length, ContentFormat format)
    {
        // NOTE: A content format of 0 means "text/plain; charset=utf-8".
        // See https://datatracker.ietf.org/doc/html/rfc7252#section-12.3
        // Therefore, it is a valid content format and needs to be added as an option.
        ErrorCode err;
        err = this->addOption(OptionNumber::ContentFormat,
                              reinterpret_cast<const uint8_t *>(&format),
                              (static_cast<uint16_t>(format) > 255) ? 2 : 1); // Use minimal representation.
        if (err != ErrorCode::None)
        {
            return err;
        }

        return this->addPayload(payload, length);
    }

    void Node::start()
    {
        this->_udp->begin(this->_port);
    }
}