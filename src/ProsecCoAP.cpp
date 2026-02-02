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
        size_t currentByte = COAP_HEADER_SIZE + tokenLength;
        // Options are stored in order of increasing option number.
        // Iterate through existing options to find the insertion point.
        // In case of multiple options with the same number, the new option is added after existing ones.
        uint16_t lastOptionNumber = 0;
        while (currentByte < this->_messageLength && lastOptionNumber <= static_cast<uint16_t>(newNumber))
        {
            // Read the option header byte.
            uint8_t optionHeader = this->_message[currentByte];
            uint8_t delta = (optionHeader >> 4) & 0x0F;
            uint8_t length = optionHeader & 0x0F;

            // SECTION Process the delta special cases.
            if (delta == 13)
            {
                // Extended delta (8 bits).
                if (this->_messageLength < currentByte + 1)
                {
                    return ErrorCode::MalformedMessage;
                }
                currentByte++;
                delta = this->_message[currentByte] + 13;
            }
            else if (delta == 14)
            {
                // Extended delta (16 bits).
                if (this->_messageLength < currentByte + 2)
                {
                    return ErrorCode::MalformedMessage;
                }
                delta = ((static_cast<uint16_t>(this->_message[currentByte + 1]) << 8) |
                         static_cast<uint16_t>(this->_message[currentByte + 2])) +
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
            // Current option number is previous plus delta.
            lastOptionNumber += delta;
            // !SECTION End of delta processing.

            // SECTION Process the length special cases.
            if (length == 13)
            {
                // Extended length (8 bits).
                if (this->_messageLength < currentByte + 1)
                {
                    return ErrorCode::MalformedMessage;
                }
                currentByte++;
                length = this->_message[currentByte] + 13;
            }
            else if (length == 14)
            {
                // Extended length (16 bits).
                if (this->_messageLength < currentByte + 2)
                {
                    return ErrorCode::MalformedMessage;
                }
                length = ((static_cast<uint16_t>(this->_message[currentByte + 1]) << 8) |
                          static_cast<uint16_t>(this->_message[currentByte + 2])) +
                         269;
                currentByte += 2;
            }
            else if (length == 15)
            {
                return ErrorCode::MalformedMessage;
            }
            // !SECTION End of length processing.
            // Move to the beginning of the next option (or to the insertion point).
            currentByte += 1 + length;
        }
        // currentByte is now the insertion point for the new option.

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
            // Multi-instance option, allowed to add again.
            break;
        }

        // Build the new option header.
        uint8_t newOptionHeader[5] = {0}; // Max 5 bytes for option header (1 + 2 for delta + 2 for length).
        size_t newOptionHeaderLength = 1; // Initial header length.
        uint16_t newOptionDelta = static_cast<uint16_t>(newNumber) - lastOptionNumber;
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
            newOptionHeader[newOptionHeaderLength] = ((newLength - 269) >> 8) & 0xFF;
            newOptionHeader[newOptionHeaderLength + 1] = (newLength - 269) & 0xFF;
            newOptionHeader[0] |= 14; // Length = 14 on the 4 LSb.
            newOptionHeaderLength += 2;
        }
        else if (newLength > 12)
        {
            // Extended length (8 bits).
            newOptionHeader[newOptionHeaderLength] = (newLength - 13) & 0xFF;
            newOptionHeader[0] |= 13; // Length = 13 on the 4 LSb.
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
        this->_currentByte++;
        uint8_t delta = (optionHeader >> 4) & 0x0F;
        option.length = optionHeader & 0x0F;

        // SECTION Process the delta special cases.
        if (delta == 13)
        {
            // Extended delta (8 bits).
            if (this->_message->getLength() < this->_currentByte + 1)
            {
                return ErrorCode::MalformedMessage;
            }
            this->_currentByte++;
            delta = (this->_message)->_message[this->_currentByte] + 13;
        }
        else if (delta == 14)
        {
            // Extended delta (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MalformedMessage;
            }
            delta = ((static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 1]) << 8) |
                     static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 2])) +
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
            if (this->_message->getLength() < this->_currentByte + 1)
            {
                return ErrorCode::MalformedMessage;
            }
            this->_currentByte++;
            option.length = (this->_message)->_message[this->_currentByte] + 13;
        }
        else if (option.length == 14)
        {
            // Extended length (16 bits).
            if (this->_message->getLength() < this->_currentByte + 2)
            {
                return ErrorCode::MalformedMessage;
            }
            option.length = ((static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 1]) << 8) |
                             static_cast<uint16_t>((this->_message)->_message[this->_currentByte + 2])) +
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

        // Current option number is previous plus delta.
        this->_currentOptionNumber += delta;
        option.number = static_cast<OptionNumber>(this->_currentOptionNumber);

        // The option value starts after delta and length bytes.
        option.value = &(this->_message)->_message[this->_currentByte];
        this->_currentByte += option.length; // Update for next call.

        return ErrorCode::None;
    }
}