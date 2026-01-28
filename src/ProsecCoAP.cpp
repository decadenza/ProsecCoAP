#include "Arduino.h"
#include "ProsecCoAP.h"
#include "utility/helpers.h"

namespace Coap
{
    Message::Message()
    {
        // Initialize message with default CoAP header values.
        this->_message[0] = (COAP_VERSION << 6) | (static_cast<uint8_t>(MessageType::Con) << 4); // Version 1, Type 0 (CON), Token Length 0
        this->_message[1] = static_cast<uint8_t>(MessageCode::Empty);                            // Code
        uint16_t messageId = getNextId();
        this->_message[2] = (messageId >> 8) & 0xFF; // Message ID high byte.
        this->_message[3] = messageId & 0xFF;        // Message ID low byte.
        this->_messageLength = COAP_HEADER_SIZE;     // Keep track of the current message size.
    }

    ErrorCode Message::_insert(size_t startPosition, const uint8_t *data, size_t length)
    {
        // Check if insertion would exceed maximum message size.
        if (this->_messageLength + length > COAP_MAX_MESSAGE_SIZE)
        {
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
        // Check if removal is valid.
        if (startPosition + length > this->_messageLength)
        {
            return ErrorCode::InvalidArgument;
        }

        // Shift bytes to remove the specified data.
        memmove(this->_message + startPosition,          // Destination.
                this->_message + startPosition + length, // Source.
                length);                                 // Amount of bytes to move.

        // Update current message length.
        this->_messageLength -= length;
        return ErrorCode::None;
    }

    uint16_t Message::getNextId()
    {
        // Message ID is a simple sequential identifier.
        // However, to avoid collisions after a reset, start with a random value.
        static uint16_t id = (uint16_t)random(0, 0xFFFF);
        return id++;
    }

    ErrorCode Message::addToken(size_t length)
    {

        if (length > COAP_MAX_TOKEN_LENGTH)
        {
            return ErrorCode::InvalidArgument;
        }

        // SECTION Generate a random token.
        uint8_t tokenBuffer[COAP_MAX_TOKEN_LENGTH] = {0};
        // The random token is generated in chunks of 4 bytes.
        for (size_t chunk; chunk <= length / 4U; chunk++)
        {
            // Taking the full range of random values, including negative ones, and casting to uint32_t.
            // NOTE: It is more efficient to generate 4 bytes at a time.
            // The endianness is irrelevant for a random value.
            uint32_t r = random(0xF0000000, 0x0FFFFFFF);
            memcpy(tokenBuffer + (chunk * 4U), &r, 4U);
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

    ErrorCode Message::getToken(uint8_t *&buffer, size_t &length)
    {
        // Extract token length from the first byte of the message.
        length = this->_message[0] & 0x0F;
        if (length == 0)
        {
            // Invalid case: no token present.
            length = 0;
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
        // Copy the token into the provided buffer.
        // When present, the token starts immediately after the 4-byte header.
        memcpy(buffer, this->_message + COAP_HEADER_SIZE, length);
        return ErrorCode::None;
    }
}