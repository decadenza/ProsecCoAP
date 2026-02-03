/*
ProseCoAP library for Arduino.

This software is released under the MIT License.
Copyright (c) 2014 Hirotaka Niisato
Copyright (c) 2026 Pasquale Lafiosca

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef __PROSECCOAP_H__
#define __PROSECCOAP_H__

// Include common types.
#include "Types.h"

// Include internal implementation details.
#include "utility/Detail.h"

/**
 * @namespace Coap
 * @brief Namespace for the library.
 *
 * All library enums, classes and functions are defined within this namespace.
 */
namespace Coap
{

    // SECTION Functions.
    /**
     * @defgroup Functions
     * @brief CoAP helper functions.
     * @{
     */

    /** @} */ // End of Functions group
    // !SECTION End of Functions.

    /**
     * @brief A CoAP option.
     *
     * It references an option within a CoAP message.
     */
    struct Option
    {
        /**
         * @brief The option number.
         *
         * This is initialized to 0 to indicate an empty (invalid) option.
         */
        OptionNumber number;
        /**
         * @brief Pointer to the option value.
         *
         * The pointer is valid as long as the pointed buffer exists.
         */
        const uint8_t *value;
        /**
         * @brief Length of the option value in bytes.
         */
        size_t length;

        Option() : number(static_cast<OptionNumber>(0)), value(nullptr), length(0) {}
        /**
         * @brief Build a CoAP option.
         * @param number The option number.
         * @param value The option value.
         * @param length The length of the option value in bytes.
         */
        Option(OptionNumber number, const uint8_t *value, size_t length)
            : number(number), value(value), length(length) {}
    };

    /**
     * @brief The option iterator.
     *
     * It allows iterating through the options of a CoAP message,
     * in order of increasing option number.
     */
    class OptionIterator
    {

        // Give access to private members to Message.
        friend class Message;
        /**
         * @brief The message being iterated.
         */
        const Message *_message;
        /**
         * @brief Track the current byte position in the message.
         *
         * This points to the next byte to read.
         */
        size_t _currentByte;
        /**
         * @brief Track the current option number as raw value.
         */
        uint16_t _currentOptionNumber;

    public:
        OptionIterator(const Message *message);

        /**
         * @brief Get the next option in the message.
         *
         * Note that some options may be repeated.
         *
         * @param[out] option The next option.
         *
         * @return An error code indicating success or failure.
         *         It returns ErrorCode::None when an option is found.
         *         When there are no more options (either end of the message or beginning of payload),
         *         it returns ErrorCode::NotFound.
         *         If the message is malformed, it returns ErrorCode::MalformedMessage.
         *
         * @code{.cpp}
         * Coap::OptionIterator it = message.getOptionIterator();
         * Coap::Option option;
         * while((err = optIterator.next(opt)) == Coap::ErrorCode::None) {
         *         // Process option...
         * }
         * @endcode
         */
        ErrorCode next(Option &option);
    };

    /**
     * @brief A CoAP message.
     *
     * This is a view on the binary representation of a CoAP message.
     * The intended use is to build a CoAP message to be sent, or to parse
     * a received CoAP message.
     *
     * See https://datatracker.ietf.org/doc/html/rfc7252#section-3
     */
    class Message
    {
        friend class OptionIterator;

    private:
        /**
         * @brief Message binary data.
         */
        uint8_t _message[COAP_MAX_MESSAGE_SIZE];
        /**
         * @brief Current length of the message in bytes.
         */
        size_t _messageLength;
        /**
         * @brief Inserts data into the message.
         *
         * It inserts data at the specified position, shifting existing data.
         * It updates the resulting message length accordingly.
         *
         * @param startPosition The position in the message where the data should be inserted.
         * @param data Pointer to the data to insert.
         * @param length The length of the data to insert in bytes.
         * @return An error code indicating success or failure. Particularly,
         *         it returns @ref ErrorCode::MessageTooLarge if the insertion would exceed
         *         the maximum message size (@ref COAP_MAX_MESSAGE_SIZE).
         *         On failure, the message remains unmodified.
         */
        ErrorCode _insert(size_t startPosition, const uint8_t *data, size_t length);

        /**
         * @brief Removes bytes from the message, shifting existing data.
         *
         * It removes bytes starting from the specified position, shifting existing data.
         * It updates the resulting message length accordingly.
         *
         * @param startPosition The position in the message where the data should be removed.
         * @param length The length of the data to remove in bytes.
         * @return An error code indicating success or failure.
         *         On failure, the message remains unmodified.
         */
        ErrorCode _remove(size_t startPosition, size_t length);

        /**
         * @brief Get the next Message ID.
         *
         * Message ID are sequentially assigned, starting from a random value.
         * The role of the Message ID is only to detect duplicates.
         *
         * @return A 16-bit message ID.
         */
        static uint16_t _getNextId();

    public:
        /**
         * @brief Builds a default CoAP message.
         *
         * The version is set to @ref COAP_VERSION.
         * The type is set to @ref COAP_NONCON.
         * The code is set to @ref COAP_EMPTY.
         * The token length is set to 0.
         * The message ID is assigned automatically.
         */
        Message() : Message(MessageType::NonCon, MessageCode::Empty) {}

        /**
         * @brief Builds a CoAP message with the given type and code.
         *
         * The version is set to @ref COAP_VERSION.
         * The token length is set to 0.
         * The message ID is assigned automatically.
         *
         * @param type The message type.
         * @param code The message code.
         */
        Message(MessageType type, MessageCode code);

        /**
         * @brief Get the message length.
         * @return The length of the message in bytes.
         */
        size_t getLength() const
        {
            return this->_messageLength;
        }

        /**
         * @brief Set the message type.
         *
         * @param type The message type to set.
         */
        void setType(MessageType type);

        /**
         * @brief Get the message type.
         *
         * The type is always present in a CoAP message.
         */
        MessageType getType() const;

        /**
         * @brief Set the message code.
         *
         * @param code The message code to set.
         */
        void setCode(MessageCode code);
        /**
         * @brief Get the message code.
         *
         * The code is always present in a CoAP message.
         */
        MessageCode getCode() const;

        /**
         * @brief Get the message ID.
         *
         * The message ID is always present in a CoAP message.
         *
         * @return The 16-bit message ID.
         */
        uint16_t getId() const;

        /**
         * @brief Get the current token length.
         * @return The token length in bytes.
         */
        size_t getTokenLength() const;

        /**
         * @brief Add a token of the given length to the message.
         *
         * A random token is generated and added to the message as per specifications.
         * If present, the previous token is replaced.
         *
         * The token is an optional field in a CoAP message.
         * It is intended for use as a client-local identifier for
         * differentiating between concurrent requests.
         *
         * @param length The length (in bytes) of the token.
         *               The maximum length is @ref COAP_MAX_TOKEN_LENGTH bytes.
         * @return An error code. ErrorCode::None for success.
         *
         * Example:
         * @code{.cpp}
         * CoapMessage msg;
         * const uint8_t* token = msg.addToken(4);
         * @endcode
         */
        ErrorCode addToken(size_t length);

        /**
         * @brief Get the current token from the message.
         *
         * @param[out] buffer Pointer to the token within the message.
         *             @warning The pointer is valid **as long as the message exists**.
         * @param[out] length The token length.
         * @return An error code. ErrorCode::None for success.
         *
         * Example:
         * @code{.cpp}
         * Coap::Message msg;
         * const uint8_t *token;
         * size_t length;
         * msg.getToken(token, length);
         * @endcode
         */
        ErrorCode getToken(const uint8_t *&buffer, size_t &length) const;

        /**
         * @brief Return an iterator over the message options.
         * @return An option iterator, @ref OptionIterator.
         */
        OptionIterator getOptionIterator() const;
        /**
         * @brief Add an option to the message.
         *
         * The option is added according to the CoAP option encoding rules.
         * For options that can be added at most once, this function follows a "first add wins" policy. Any
         * subsequent addition of the same option number will return @ref ErrorCode::NotSupported.
         * For options that can appear multiple times, this function appends the new option to the existing ones.
         *
         * If adding the option will result in exceeding the limits specified by RFC 7252 Section 5.10,
         * the error code @ref ErrorCode::NotSupported is returned.
         * For options that can appear multiple times, the option is *appended after* the existing ones.
         *
         * @warning This is a low-level method to add options.
         *          It does not perform validation of the option length.
         *          It is the caller's responsibility to ensure that the option is valid
         *          according to the CoAP specification.
         *
         *
         * Prefer using specialized methods for common options like @ref COAP_CONTENT_FORMAT,
         * @ref COAP_URI_PATH, or @ref COAP_URI_QUERY when available.
         *
         * See https://datatracker.ietf.org/doc/html/rfc7252#section-3.1
         *
         * @param number The option number, as defined in the CoAP specification.
         * @param value The pointer to the option value.
         * @param length The length of the option value.
         * @return An error code indicating success or failure. @ref ErrorCode::NotSupported is returned
         *        if adding the option would exceed the maximum number of allowed options for that
         *        number. An @ref ErrorCode::MessageTooLarge is returned if adding the option would exceed
         *        the maximum message size (@ref COAP_MAX_MESSAGE_SIZE).
         */
        ErrorCode addOption(OptionNumber number, const uint8_t *value, size_t length);

        /**
         * @brief Add an option to the message.
         * @see @ref addOption(OptionNumber number, const uint8_t *value, size_t length).
         */
        ErrorCode addOption(Option option)
        {
            return this->addOption(option.number, option.value, option.length);
        };

        /**
         * @brief Add the Uri-Host option to the message.
         *
         * It converts the given IP address to string format and adds it as the Uri-Host option,
         * as per RFC 7252 Section 6.4.
         * The Uri-Host option is not compulsory. As per Section 6.5, if not present, the
         * the destination IP address where the message is being sent will be used.
         *
         * @see addOption
         *
         * @param ip The IP address to set as the Uri-Host.
         * @return An error code indicating success or failure.
         */
        ErrorCode addHost(IPAddress ip);

        /**
         * @brief Add the Uri-Port option to the message.
         *
         * It adds the Uri-Port option as per RFC 7252 Section 6.4.
         * The Uri-Port option is not compulsory. As per Section 6.5, if not present, the
         * the default CoAP port @ref COAP_DEFAULT_PORT will be used.
         *
         * Any existing Uri-Port option is removed before adding the new one.
         *
         * @param port The port number to set as the Uri-Port.
         * @return An error code indicating success or failure.
         */
        ErrorCode addPort(uint16_t port);

        /**
         * @brief Add the URI path (and query) to the message.
         *
         * It follows section "Decomposing URIs into Options"
         * https://datatracker.ietf.org/doc/html/rfc7252#section-6.4
         * to encode the path into the necessary Uri-Path and Uri-Query options.
         *
         * Any existing Uri-Path and Uri-Query options are removed before adding the new ones.
         *
         * @param path The URI path + query associated with the recipient, null terminated.
         *             Initial slash is optional. Valid examples are:
         *             ``
         *             sensors/temp
         *             /sensors/temp?unit=celsius
         *             ```
         *             Individual Uri-Path and Uri-Query segments cannot exceed
         *             255 bytes, as per specification.
         * @return An error code indicating success or failure.
         */
        ErrorCode addPath(const char *path);

        /**
         * @brief Get the payload from the message.
         *
         * The payload is a raw set of bytes. To interpret it, refer to the
         * Content-Format option, if present.
         *
         * @param[out] payload Pointer to the payload within the message.
         *             @warning The pointer is valid **as long as the message exists**.
         * @param[out] length The payload length.
         * @return An error code. ErrorCode::None for success.
         *
         * Example:
         * @code{.cpp}
         * Coap::Message msg;
         * const uint8_t *payload;
         * size_t length;
         * msg.getPayload(payload, length);
         * @endcode
         */
        ErrorCode getPayload(const uint8_t *&payload, size_t &length) const;

        /**
         * @brief Add a payload to the message.
         *
         * If a payload is already present, this function will return an error.
         *
         * @param payload Pointer to the payload data.
         * @param length Length of the payload data.
         * @param format The content format of the payload.
         * @return An error code indicating success or failure.
         *
         * @see addPayload(const uint8_t *payload, size_t length, ContentFormat format)
         *      for the variant that also adds the Content-Format option.
         */
        ErrorCode addPayload(const uint8_t *payload, size_t length);

        /**
         * @brief Add a payload and the Content-Format option to the message.
         *
         * If a payload or a Content-Format option are already present,
         * this function will return an error.
         *
         * @param payload Pointer to the payload data.
         * @param length Length of the payload data.
         * @param format The content format of the payload.
         * @return An error code indicating success or failure.
         *
         * @see addPayload(const uint8_t *payload, size_t length) for the variant without Content-Format option.
         */
        ErrorCode addPayload(const uint8_t *payload, size_t length, ContentFormat format);
    };

    /**
     * @brief The CoAP node that runs on this device.
     *
     * It uses an underlying UDP instance for communication.
     * It provides methods to send and receive CoAP messages, @see Message.
     */
    class Node
    {
    private:
        // The internal UDP instance used for communication.
        UDP *_udp;
        // The local UDP port used for communication.
        uint16_t _port;
        // The callback fuction for handling incoming response messages.
        Callback _responseHandler;
        // The registry of URI paths and their associated callbacks.
        Detail::UriRegistry _serverRegistry;

    public:
        /**
         * @brief Build a CoAP node using the given UDP instance.
         *
         * This instance will use the default CoAP port (@ref COAP_DEFAULT_PORT).
         *
         * @param udp The UDP instance to use for communication.
         */
        Node(UDP &udp) : Node(udp, COAP_DEFAULT_PORT) {}

        /**
         * @brief Build a CoAP node using the given UDP instance and port.
         *
         * @param udp The UDP instance to use for communication.
         * @param port The local UDP port to use for communication.
         */
        Node(UDP &udp, uint16_t port) : _udp(&udp), _port(port), _responseHandler(nullptr) {}

        /**
         * @brief Start the CoAP instance.
         *
         * It starts the underlying UDP instance, enabling communication.
         * The UDP instance is bound to the local port specified at construction time.
         *
         * @returns ErrorCode::None on success, or an error code on failure.
         */
        ErrorCode start();

        /**
         * @brief Set the response callback.
         *
         * The response handler is invoked when a message of type @ref COAP_ACK or @ref COAP_RESET
         * is received, allowing the application to handle the response.
         * The callback is unique for all the responses sent to this Coap instance.
         *
         * Note that transmission ACK are also received internally by the retrasmission queue,
         * so that retransmission can be stopped.
         *
         * Responses to different requests must be differentiated by matching the message ID.
         *
         * @param handler The callback function to handle acknowledgements.
         */
        void setResponseHandler(Callback handler) { _responseHandler = handler; }

        /**
         * @brief Serve a given URI path with the specified callback.
         * @param path The URI path to serve.
         * @param callback The callback function to handle requests to the given path.
         * @return An error code indicating success or failure.
         */
        ErrorCode serve(const char *path, Callback callback);

        /**
         * @brief Perform a single iteration of the CoAP event loop.
         *
         * **This method MUST be called in the main loop() of your application.**
         *
         * It is responsible for processing incoming messages,
         * handling retransmissions, and invoking the response handler for received messages.
         *
         * @return An error code indicating success or failure.
         */
        ErrorCode loop();
    };

} // End of namespace Coap

#endif
