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

    uint16_t Message::getId()
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

    MessageType Message::getType()
    {
        // Extract the type bits from the first byte.
        uint8_t t = (this->_message[0] >> 4) & 0b00000011;
        return static_cast<MessageType>(t);
    }

    void Message::setCode(MessageCode code)
    {
        this->_message[1] = static_cast<uint8_t>(code);
    }

    MessageCode Message::getCode()
    {
        return static_cast<MessageCode>(this->_message[1]);
    }

    ErrorCode Message::addToken(size_t length)
    {

        if (length > COAP_MAX_TOKEN_LENGTH)
        {
            return ErrorCode::InvalidArgument;
        }

        // SECTION Remove any existing token.
        size_t existingTokenLength = this->_message[0] & 0x0F;
        this->_remove(COAP_HEADER_SIZE, existingTokenLength); // No-op if existingTokenLength is 0.

        // SECTION Generate a random token.
        uint8_t tokenBuffer[COAP_MAX_TOKEN_LENGTH];

        // The random token is generated in chunks of sizeof(long) bytes.
        // Although long should be 4 bytes, the code is platform-independent.
        size_t chunkSize = sizeof(long);
        long min = 1 << ((chunkSize * 8) - 1); // Minimum value for long.
        long max = ~min;                       // Maximum value for long.
        for (size_t n = 0; chunkSize * n < length; n++)
        {
            // Taking the full range of random values, including negative ones, and casting to uint32_t.
            // NOTE: It is more efficient to generate 4 bytes at a time.
            // Random takes long data types, so negative values are possible.
            // NOTE: The endianness is irrelevant for a random value.
            long r = random(min, max);
            memcpy(tokenBuffer + (chunkSize * n), &r, chunkSize);
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

    ErrorCode Message::getToken(const uint8_t *&buffer, size_t &length)
    {
        // Extract token length from the first byte of the message.
        length = this->_message[0] & 0x0F;
        if (length == 0)
        {
            // Invalid case: no token present.
            buffer = nullptr;
            return ErrorCode::None; // No token present. Not an error.
        }
        if (this->_messageLength < COAP_HEADER_SIZE + length)
        {
            // The message is malformed: it indicates the presence of a token
            // whose length would be greater than the actual message size.
            length = 0;
            buffer = nullptr;
            return ErrorCode::MalformedMessage;
        }
        // Give access by reference to the token within the message.
        buffer = this->_message + COAP_HEADER_SIZE;
        return ErrorCode::None;
    }
}