#include "Arduino.h"
#include "ProsecCoAP.h"
#include "Utils.h"

namespace Coap
{

    ErrorCode getRandomToken(size_t length, uint8_t *buffer)
    {
        if (length > COAP_MAX_TOKEN_LENGTH)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }

        // Fill token buffer with random bytes, minimizing calls to random().
        // sizeof(long) is generally 4 bytes on Arduino platforms, however this is not guaranteed by the standard.
        // Each random() call only provides 31 random bits, so we can only extract up to 3 fully random bytes per call
        // starting from the LSB.
        constexpr size_t chunkSize = 3;
        constexpr long max = (1UL << (chunkSize * 8)); // 16 777 216.

        size_t processedBytes = 0;
        while (processedBytes < length)
        {
            uint32_t r = static_cast<uint32_t>(random(0, max));
            size_t bytesToCopy = (length - processedBytes < chunkSize) ? (length - processedBytes) : chunkSize;
            for (size_t i = 0; i < bytesToCopy; i++)
            {
                buffer[processedBytes + i] = static_cast<uint8_t>((r >> (8 * i)) & 0xFF);
            }
            processedBytes += bytesToCopy;
        }

        return ErrorCode::OK;
    }

    void Message::setId(uint16_t id)
    {
        // Message ID in network byte order (big-endian).
        this->_message[2] = (id >> 8) & 0xFF; // Message ID high byte.
        this->_message[3] = id & 0xFF;        // Message ID low byte.
    }

    Message::Message(MessageType type, MessageCode code, uint16_t id)
    {
        // Initialize message with default CoAP header values.
        this->_message[0] = (COAP_VERSION << 6) | (static_cast<uint8_t>(type) << 4); // Version 1, given code, Token Length 0
        this->_message[1] = static_cast<uint8_t>(code);                              // Code data.
        this->setId(id);                                                             // Set message ID.
        this->_messageLength = COAP_HEADER_SIZE;                                     // Keep track of the current message size.
    }

    ErrorCode Message::fromUdp(UDP *udp, Message &message)
    {
        message._messageLength = 0; // Initialize to zero in case of failure.

        int len = udp->parsePacket();
        if (len < COAP_HEADER_SIZE)
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
        if (len < COAP_HEADER_SIZE)
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
        return ErrorCode::OK;
    }

    ErrorCode Message::buildResponse(MessageCode code, Message &response) const
    {
        if (this->getLength() < COAP_HEADER_SIZE)
        {
            return ErrorCode::MALFORMED_MESSAGE;
        }
        // Initialize response message (version, type, code)
        // and use the message id from the request.
        response = Message(MessageType::ACK, code, this->getId());

        // The response length is currently just the header size.
        // If the request has a token, copy it and update the token length in the response.
        size_t tokenLength = this->getTokenLength();
        memcpy(response._message + COAP_HEADER_SIZE, // Destination.
               this->_message + COAP_HEADER_SIZE,    // Source.
               tokenLength);                         // Length.
        // Set token length, stored in the 4 lower bits of the first byte.
        response._message[0] |= static_cast<uint8_t>(tokenLength) & 0x0F;
        // Update the total message length.
        response._messageLength = COAP_HEADER_SIZE + tokenLength;
        return ErrorCode::OK;
    }

    ErrorCode Message::buildNotification(Observer &observer, Message &notification) const
    {
        notification = *this; // Start with a copy of the original message.
        // Set a new message ID (it cannot be the same of the original message).
        uint16_t newId = Message::_getNextId();
        notification.setId(newId);
        // Set message type to NON by default.
        notification.setType(MessageType::NON);

        // Overwrite any existing token and add the observer token.
        const uint8_t *observerToken = observer.getToken();
        size_t observerTokenLength = observer.getTokenLength();
        ErrorCode err = notification.setToken(observerToken, observerTokenLength); // It overwrites any existing token in the message.
        if (err != ErrorCode::OK)
        {
            // Could not add observer token to the message.
            return err;
        }

        // Add Observe option with the appropriate incremental value.
        uint32_t observeValue = observer.getNextSequentialNumber();
        // The Observe option value is a 24-bit unsigned integer, so it can be up to 3 bytes long.
        // We use the minimum number of bytes needed to represent the value, as suggested by specifications.
        size_t observeValueBytes = 3;
        if (observeValue == 0)
        {
            observeValueBytes = 0; // Special case for zero, which can be represented with zero bytes.
        }
        else if (observeValue <= 0xFF)
        {
            observeValueBytes = 1;
        }
        else if (observeValue <= 0xFFFF)
        {
            observeValueBytes = 2;
        }
        uint8_t observeValueBigEndian[4];                               // 4 bytes to hold the big-endian representation of the 24-bit value, with leading zeros.
        Utils::toNetworkByteOrder(observeValue, observeValueBigEndian); // Convert observe value to big-endian byte order, as required by CoAP specifications.

        // Ignoring the leading zeros in the MSB.
        err = notification.addOption(OptionNumber::OBSERVE, observeValueBigEndian + 1 + (3 - observeValueBytes), observeValueBytes);
        if (err != ErrorCode::OK)
        {
            // Could not add Observe option to the message.
            return err;
        }

        return ErrorCode::OK;
    }

    uint16_t Message::getId() const
    {
        // 3rd and 4th bytes of the message MUST contain the Message ID.
        return (static_cast<uint16_t>(this->_message[2]) << 8) | static_cast<uint16_t>(this->_message[3]);
    }

    ErrorCode Message::_insert(size_t startPosition, const uint8_t *data, size_t length)
    {
        if (length == 0)
        {
            return ErrorCode::OK; // Nothing to insert.
        }
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
        return ErrorCode::OK;
    }

    ErrorCode Message::_remove(size_t startPosition, size_t length)
    {
        if (length == 0)
        {
            return ErrorCode::OK; // Nothing to remove.
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
        return ErrorCode::OK;
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

    ErrorCode Message::setToken(const uint8_t *token, size_t length)
    {
        if (length > COAP_MAX_TOKEN_LENGTH)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }

        // SECTION Remove any existing token.
        size_t existingTokenLength = this->_message[0] & 0x0F; // Get existing token length.
        this->_remove(COAP_HEADER_SIZE, existingTokenLength);  // No-op if existingTokenLength is 0.

        // Write the generated token into the message buffer and update overall message length.
        ErrorCode err = this->_insert(COAP_HEADER_SIZE, token, length);
        if (err != ErrorCode::OK)
        {
            // Error. The message was not modified.
            return err;
        }
        // SECTION Update message header to reflect new token length.
        // The token length is stored in the 4 lower bits of the first byte.
        this->_message[0] |= static_cast<uint8_t>(length) & 0x0F;
        return ErrorCode::OK;
    }

    ErrorCode Message::addRandomToken(size_t length)
    {

        // SECTION Generate a random token.
        uint8_t tokenBuffer[COAP_MAX_TOKEN_LENGTH];

        ErrorCode err = getRandomToken(length, tokenBuffer);
        if (err != ErrorCode::OK)
        {
            return err; // Failed to generate random token.
        }
        // !SECTION

        return this->setToken(tokenBuffer, length);
    }

    const uint8_t *Message::getToken() const
    {
        // Give access by reference to the token within the message.
        // Caller can use getTokenLength() to get the token length in bytes (which may also be zero).
        return this->_message + COAP_HEADER_SIZE;
    }

    bool Message::matchesToken(const uint8_t *token, size_t length) const
    {
        size_t tokenLength = this->getTokenLength();
        if (tokenLength != length)
        {
            return false; // Lengths differ, cannot match.
        }
        const uint8_t *messageToken = this->getToken();
        return (memcmp(messageToken, token, length) == 0);
    }

    ErrorCode Message::addOption(OptionNumber newNumber, const uint8_t *newValue, size_t newLength)
    {
        OptionIterator it = this->getOptionIterator();
        Option opt;
        uint16_t lastOptionNumber = 0;
        size_t currentByte = it._currentByte; // Next byte to read.
        ErrorCode err;
        while ((err = it.next(opt)) == ErrorCode::OK) // Exit the loop if there are no more options.
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
                // The insertion point is at currentByte.
                break;
            }
        }

        if (err != ErrorCode::OK && err != ErrorCode::NOT_FOUND)
        {
            // An error occurred while iterating through options. The message may be malformed.
            return err;
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

        if (this->getLength() + newOptionHeaderLength + newLength > COAP_MAX_MESSAGE_SIZE)
        {
            // Insertion would exceed maximum message size.
            // Return without modification to the message.
            return ErrorCode::MESSAGE_TOO_LARGE;
        }

        // Write the header into the message buffer.
        err = this->_insert(currentByte, newOptionHeader, newOptionHeaderLength);
        if (err != ErrorCode::OK)
        {
            return err; // Insertion failed, message unmodified.
        }
        // Write the value into the message buffer.
        currentByte += newOptionHeaderLength;
        err = this->_insert(currentByte, newValue, newLength);
        if (err != ErrorCode::OK)
        {
            // Value insertion failed. Remove the previously inserted header.
            // REVIEW: If this fails, there is not much we can do... Message will be corrupted.
            if (this->_remove(currentByte - newOptionHeaderLength, newOptionHeaderLength) != ErrorCode::OK)
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
            return ErrorCode::OK; // Message ends here. No following option to adjust.
        }

        // The following option delta must be adjusted by *subtracting* the delta
        // added to the new option.
        if (newOptionDelta == 0)
        {
            return ErrorCode::OK; // No adjustment needed. All good.
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
            return ErrorCode::OK;
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
                    return ErrorCode::OK;
                }
            }
            else if (oldDelta == 15)
            {
                // Old delta is special case: payload marker.
                if (length == 15)
                {
                    // Payload marker reached. End of options.
                    return ErrorCode::OK;
                }
                else
                {
                    return ErrorCode::MALFORMED_MESSAGE;
                }
            }
        }
        // !SECTION End of delta adjustment.

        // Successfully added option and adjusted the following one.
        return ErrorCode::OK;
    }

    OptionIterator::OptionIterator(const Message &message)
        : _message(message)
    {
        _currentByte = COAP_HEADER_SIZE + _message.getTokenLength();
        _currentOptionNumber = 0;
    }

    OptionIterator Message::getOptionIterator() const
    {
        // Return an option iterator for this message.
        return OptionIterator(*this);
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
        // We use the minimal representation (0, 1 or 2 bytes).
        size_t length = 2;
        if (port == 0)
            length = 0; // Special case for port 0, which can be represented with zero bytes.
        else if (port <= 0xFF)
            length = 1;

        uint8_t portBigEndian[2];
        Utils::toNetworkByteOrder(port, portBigEndian); // Convert port to big-endian byte order, as required by CoAP specifications.
        // If length is 0, write an option with zero bytes.
        // If length is 1, only the second byte is used.
        // If length is 2, both bytes are used.
        return this->addOption(OptionNumber::URI_PORT, portBigEndian + (2 - length), length);
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
            if (path[i] == '\0')
            {
                // End of string reached. We are done.
                return ErrorCode::OK;
            }
            if (path[i] == '/')
            {
                // Skip slashes, even consecutive ones.
                i++;
                continue;
            }
            if (path[i] == '?')
            {
                // Start of query part. Skip the '?' and continue.
                hasQuery = true;
                i++;
                continue;
            }
            if (path[i] == '&')
            {
                if (!hasQuery)
                {
                    // '&' is only valid within the query part.
                    return ErrorCode::INVALID_ARGUMENT;
                }
                // Else we are in the query part, so skip the '&' and continue.
                i++;
                continue;
            }
            size_t segmentStart = i; // First character of the current segment that is not '/', '?' or '&'.
            // Find the end of the current segment.
            do
            {
                i++;
            } while (path[i] != '/' && path[i] != '?' && path[i] != '\0' && path[i] != '&');
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
                if (err != ErrorCode::OK)
                {
                    // Failed to add option. Block the operation.
                    // Possibly the message will be malformed!
                    return err;
                }
            }
        };
    }

    ErrorCode OptionIterator::next(Option &option)
    {
        // There are no more options if, either:
        // - We reached the end of the message.
        // - We reached the payload marker (0xFF), see below.
        if (this->_currentByte >= this->_message.getLength())
        {
            // No more options.
            return ErrorCode::NOT_FOUND;
        }
        const uint8_t *messageRaw = this->_message.asRaw();

        // See https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
        // Read the option header byte.
        uint8_t optionHeader = messageRaw[this->_currentByte];
        this->_currentByte++; // Update next byte to read.
        uint16_t delta = (optionHeader >> 4) & 0x0F;
        option.length = optionHeader & 0x0F;

        // SECTION Process the delta special cases.
        if (delta == 13)
        {
            // Extended delta (8 bits).
            if (this->_message.getLength() < this->_currentByte + 1)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            delta = messageRaw[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (delta == 14)
        {
            // Extended delta (16 bits).
            if (this->_message.getLength() < this->_currentByte + 2)
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
            if (this->_message.getLength() < this->_currentByte)
            {
                return ErrorCode::MALFORMED_MESSAGE;
            }
            option.length = messageRaw[this->_currentByte] + 13;
            this->_currentByte++;
        }
        else if (option.length == 14)
        {
            // Extended length (16 bits).
            if (this->_message.getLength() < this->_currentByte + 2)
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

        if (this->_message.getLength() < this->_currentByte + option.length)
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

        return ErrorCode::OK;
    }

    ErrorCode Message::getPayload(const uint8_t *&payload, size_t &length) const
    {
        // Find the payload marker (0xFF), if present.
        OptionIterator it = this->getOptionIterator();
        ErrorCode err;
        Option opt;
        while ((err = it.next(opt)) == ErrorCode::OK)
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
        return ErrorCode::OK;
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
        while ((err = it.next(opt)) == ErrorCode::OK)
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
        if (err != ErrorCode::OK)
        {
            return err;
        }
        // Insert the payload itself.
        err = this->_insert(firstByteOfPayload + 1, payload, length);
        if (err != ErrorCode::OK)
        {
            // Try to cancel the previous insertion of the payload marker.
            this->_remove(firstByteOfPayload, 1);
            // Return the original error.
            return err;
        }
        return ErrorCode::OK;
    }

    ErrorCode Message::addPayload(const uint8_t *payload, size_t length, ContentFormat format)
    {
        // NOTE: A content format of 0 means "text/plain; charset=utf-8".
        // See https://datatracker.ietf.org/doc/html/rfc7252#section-12.3
        // Therefore, it is a valid content format and needs to be added as an option.
        ErrorCode err;
        size_t formatBytes = 2; // Use minimal representation if possible.
        if (static_cast<uint16_t>(format) == 0)
        {
            formatBytes = 0; // Use minimal representation if possible.
        }
        else if (static_cast<uint16_t>(format) <= 0xFF)
        {
            formatBytes = 1;
        }

        uint8_t formatBigEndian[2];

        Utils::toNetworkByteOrder(static_cast<uint16_t>(format), formatBigEndian); // Convert content format to big-endian byte order, as required by CoAP specifications.

        // If formatBytes is 0, write an option with zero bytes.
        // If formatBytes is 1, only the LSB is used.
        // If formatBytes is 2, both bytes are used.
        err = this->addOption(OptionNumber::CONTENT_FORMAT,
                              formatBigEndian + (2 - formatBytes),
                              formatBytes);
        if (err != ErrorCode::OK)
        {
            return err;
        }

        return this->addPayload(payload, length);
    }

    ErrorCode Message::getObserveValue(uint32_t &observeValue)
    {
        OptionIterator it = this->getOptionIterator();
        Option option;
        while (it.next(option) == ErrorCode::OK)
        {
            if (option.number < OptionNumber::OBSERVE)
                continue;
            if (option.number == OptionNumber::OBSERVE)
            {
                // The Observe option value is 0 to 3 bytes long.
                // The protocol uses Network Byte Order (big-endian).
                // Shift the bytes accordingly.
                observeValue = 0;
                for (size_t i = 0; i < option.length && i < 3; i++) // Length is capped to 3.
                {
                    observeValue |= static_cast<uint32_t>(option.value[i]) << (8 * (2 - i));
                }
                observeValue >>= (8 * (3 - option.length)); // Shift back to the right if length is less than 3.
                return ErrorCode::OK;
            }
            else
            {
                // Since options are ordered by number, if we have passed the Observe option number,
                // it means that the Observe option is not present.
                break;
            }
        }
        return ErrorCode::NOT_FOUND;
    }

    bool Message::isObserveRegister()
    {
        if (this->getCode() != MessageCode::GET)
        {
            return false;
        }
        uint32_t observeValue;
        if (this->getObserveValue(observeValue) == ErrorCode::OK)
        {
            return observeValue == static_cast<uint32_t>(ObserveValue::REGISTER);
        }
        return false;
    }

    bool Message::isObserveDeregister()
    {
        if (this->getCode() != MessageCode::GET)
        {
            return false;
        }
        uint32_t observeValue;
        if (this->getObserveValue(observeValue) == ErrorCode::OK)
        {
            return observeValue == static_cast<uint32_t>(ObserveValue::DEREGISTER);
        }
        return false;
    }

    ErrorCode Message::getPath(String *path) const
    {
        if (path == nullptr)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }
        OptionIterator it = this->getOptionIterator();
        Option opt;
        String result = "";
        bool hasQuery = false;
        while (it.next(opt) == ErrorCode::OK)
        {
            if (opt.number == OptionNumber::URI_PATH) // Uri path always comes before query.
            {
                // Append '/' before each path segment.
                result += '/';
                // Append the path segment one character at a time.
                for (size_t i = 0; i < opt.length; i++)
                {
                    result += static_cast<char>(opt.value[i]);
                }
            }
            else if (opt.number == OptionNumber::URI_QUERY)
            {
                // Append '?' before the first query segment, '&' before the following ones.
                result += hasQuery ? '&' : '?';
                hasQuery = true;
                // Append the query segment value one character at a time.
                for (size_t i = 0; i < opt.length; i++)
                {
                    result += static_cast<char>(opt.value[i]);
                }
            }
        }
        *path = result;
        return ErrorCode::OK;
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
                return ErrorCode::OK;
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
        return ErrorCode::OK;
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
        if (err != ErrorCode::OK)
        {
            return err;
        }
        this->attempts++;
        // Set the next attempt deadline using exponential backoff.
        this->nextAttemptDeadline = millis() + (this->timeoutBaseInterval << this->attempts);
        return ErrorCode::OK;
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
            return ErrorCode::OK;
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
            uriPath.reserve(64); // Pre-allocate some space to reduce dynamic allocations. If you use long paths, you obviously don't care.

            // fromUdp() returns ErrorCode::OK while there are incoming messages.
            while ((err = Message::fromUdp(this->_udp, incomingMessage)) == ErrorCode::OK)
            {
                // Process the incoming message.
                MessageCode code = incomingMessage.getCode();

                if (code == MessageCode::EMPTY && incomingMessage.getType() == MessageType::CON)
                {
                    // ANCHOR: Handle empty confirmable messages (CoAP pings).
                    // Reply with a reset message as per https://datatracker.ietf.org/doc/html/rfc7252#section-4.2.
                    // The Reset message MUST echo the Message ID of the Confirmable message and MUST be Empty.
                    //
                    // NOTE: The easiest way to test this is to:
                    // 1. Keep a terminal with coap-server-notls -v 9 open.
                    // 2. Execute printf "\x40\x00\x00\x00" | nc -u -w 2 <IP> <PORT>
                    Coap::Message msg(Coap::MessageType::RST, Coap::MessageCode::EMPTY, incomingMessage.getId());
                    this->sendMessage(msg, (this->_udp)->remoteIP(), (this->_udp)->remotePort());
                    continue; // Move to the next incoming message.
                }

                if (code == MessageCode::GET ||
                    code == MessageCode::POST ||
                    code == MessageCode::PUT ||
                    code == MessageCode::DELETE)
                {
                    // ANCHOR This is a request message.
                    // https://datatracker.ietf.org/doc/html/rfc7252#section-5.1

                    // SECTION Extract the URI path from the message options.
                    // Empty URI path by default.
                    uriPath = "";
                    // Build the URI path from the Uri-Path option(s), if present.
                    OptionIterator it = incomingMessage.getOptionIterator();
                    Option opt;
                    while (it.next(opt) == ErrorCode::OK)
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
                    // !SECTION End of URI path extraction.

                    // Match the message URI to the registered handlers.
                    // uriPath.c_str() will give the C-style string pointer.
                    Callback handler;
                    err = this->_serverRegistry.find(uriPath.c_str(), handler);
                    if (err != ErrorCode::OK)
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

        return ErrorCode::OK;
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