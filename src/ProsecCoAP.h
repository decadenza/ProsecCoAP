/**
 * @file ProsecCoAP.h
 * @brief Main header file for the ProsecCoAP library.
 *
 * Refer to LICENSE.txt for licensing information.
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
         * `_message->_message[_currentByte]` points to the next byte to read.
         */
        size_t _currentByte;
        /**
         * @brief Track the current option number as raw value.
         */
        uint16_t _currentOptionNumber;

    public:
        /**
         * @brief Initialize the option iterator for the given message.
         *
         * The first byte to be read will be the one after the header
         * and token (if present).
         *
         * @param message The pointer to the message to iterate.
         */
        OptionIterator(const Message *message);

        /**
         * @brief Get the next option in the message.
         *
         * Note that some options may be repeated.
         *
         * @param[out] option The next option.
         *
         * @return An error code indicating success or failure.
         *         It returns ErrorCode::NONE when an option is found.
         *         When there are no more options (either end of the message or beginning of payload),
         *         it returns ErrorCode::NOT_FOUND.
         *         If the message is malformed, it returns ErrorCode::MALFORMED_MESSAGE.
         *
         * @code{.cpp}
         * Coap::OptionIterator it = message.getOptionIterator();
         * Coap::Option option;
         * while((err = optIterator.next(opt)) == Coap::ErrorCode::NONE) {
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
     * As a minimum, a @ref Message instance has a valid header.
     *
     * See https://datatracker.ietf.org/doc/html/rfc7252#section-3
     */
    class Message
    {
    private:
        /**
         * @brief Message binary data.
         */
        uint8_t _message[COAP_MAX_MESSAGE_SIZE];
        /**
         * @brief Current length of the message in bytes.
         *
         * The minimum length is @ref COAP_HEADER_SIZE.
         * A shorter length means the message is invalid.
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
         *         it returns @ref ErrorCode::MESSAGE_TOO_LARGE if the insertion would exceed
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
         * The type is set to @ref COAP_NON.
         * The code is set to @ref COAP_EMPTY.
         * The token length is set to 0.
         * The message ID is assigned automatically.
         */
        Message() : Message(MessageType::NON, MessageCode::EMPTY) {}

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
        Message(MessageType type, MessageCode code) : Message(type, code, _getNextId()) {}

        /**
         * @brief Builds a CoAP message explictly specifying type, code and message ID.
         *
         * The version is set to @ref COAP_VERSION.
         * The token length is set to 0.
         *
         * @param type The message type.
         * @param code The message code.
         * @param id The message ID.
         */
        Message(MessageType type, MessageCode code, uint16_t id);

        /**
         * @brief Get the raw binary representation of the message.
         *
         * The returned pointer is valid as long as the message exists.
         *
         * @return Pointer to the raw message data.
         */
        const uint8_t *asRaw() const
        {
            return this->_message;
        }

        /**
         * @brief Build a CoAP message reading from a UDP instance.
         *
         * @param udp A pointer to the UDP instance containing the received message.
         * @param message The CoAP message to populate.
         *
         * @return Returns @ref ErrorCode::NONE on success.
         *         Return @ref ErrorCode::NOT_FOUND if no packet was found (a size < @ref COAP_HEADER_SIZE
         *         is considered as such).
         *         It returns @ref ErrorCode::NOT_SUPPORTED if the length
         *         of the received packet is > @ref COAP_MAX_MESSAGE_SIZE.
         *         It returns @ref ErrorCode::NETWORK if an error occurred while reading from UDP.
         *         It returns @ref ErrorCode::NOT_SUPPORTED if the message version is not @ref COAP_VERSION.
         */
        static ErrorCode fromUdp(UDP *udp, Message &message);

        /**
         * @brief Build a response message based on a request message.
         *
         * The message ID is copied from the request.
         * The type is set to @ref MessageType::ACK.
         * The response code is set to the given input code.
         * If the request has a token, the same token is copied to the response.
         * The rest of the message is initialized as per default constructor @ref Message().
         *
         * @param request The request message to base the response on.
         * @param code The response message code.
         * @param[out] response The response message to populate.
         * @return An error code indicating success or failure.
         *         It returns @ref ErrorCode::NONE on success.
         */
        static ErrorCode buildResponse(const Message *request, MessageCode code, Message &response);

        /**
         * @brief Get the CoAP version of this message.
         *
         * The version is always present in a CoAP message.
         *
         * @return The 2-bit CoAP version as uint8_t.
         */
        uint8_t getVersion() const
        {
            return (this->_message[0] >> 6) & 0x03;
        }

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
         * @return An error code. ErrorCode::NONE for success.
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
         * @return An error code. ErrorCode::NONE for success.
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
         * subsequent addition of the same option number will return @ref ErrorCode::NOT_SUPPORTED.
         * For options that can appear multiple times, this function appends the new option to the existing ones.
         *
         * If adding the option will result in exceeding the limits specified by RFC 7252 Section 5.10,
         * the error code @ref ErrorCode::NOT_SUPPORTED is returned.
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
         * @return An error code indicating success or failure. @ref ErrorCode::NOT_SUPPORTED is returned
         *        if adding the option would exceed the maximum number of allowed options for that
         *        number. An @ref ErrorCode::MESSAGE_TOO_LARGE is returned if adding the option would exceed
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
         * @warning Existing Uri-Path and Uri-Query options are not removed before adding the new ones.
         *          In general, this method should be called only once per message, as multiple
         *          calls may result in a malformed message.
         *
         * @param path The URI path + query associated with the recipient, null terminated.
         *             Initial and trailing slashes are ignored. Valid examples are:
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
         * @return An error code. ErrorCode::NONE for success.
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

    class Node; // Forward declaration.

    namespace Detail
    {
        /**
         * @brief An entry in the retransmission queue.
         *
         * @ref RetransmissionQueue uses this structure to store entries.
         *
         * This needs to be declared after @ref Message is fully declared.
         */
        struct RetransmissionEntry
        {
            // The CoAP message to be retransmitted.
            Coap::Message message;
            // Count of retransmission attempts done.
            // If attempts reach COAP_MAX_RETRANSMIT, the item is considered expired.
            unsigned short attempts = COAP_MAX_RETRANSMIT;
            // The base timeout interval (randomly assigned between COAP_ACK_MIN_TIMEOUT_MS and COAP_ACK_MAX_TIMEOUT_MS).
            // This will be doubled on each retransmission attempt.
            unsigned long timeoutBaseInterval = COAP_ACK_MAX_TIMEOUT_MS;
            // Timestamp for the next attempt.
            unsigned long nextAttemptDeadline = 0;
            // Destination IP address.
            IPAddress ip;
            // Destination port.
            uint16_t port = 0;

            // Default constructor.
            // It sets the entry as expired.
            RetransmissionEntry() = default;

            /**
             * @brief Set a retransmission entry.
             *
             * This copies the message by value into the entry, so it is available for retransmission.
             * Attempts are initialised as 0 to mark the entry as valid.
             * Timeout interval is initialised with a random value as per protocol specifications.
             * The next attempt deadline is initialised accordingly.
             *
             * @param message The CoAP message to be retransmitted, copied into the entry.
             * @param ip The destination IP address.
             * @param port The destination UDP port.
             */
            void set(Coap::Message message, IPAddress ip, uint16_t port);

            /**
             * @brief Check if the entry can be considered empty.
             *
             * An entry is considered empty when the number of attempts
             * reaches @ref COAP_MAX_RETRANSMIT.
             *
             * @return true if the entry has expired, false otherwise.
             */
            bool isEmpty() const
            {
                return this->attempts >= COAP_MAX_RETRANSMIT;
            }

            /**
             * @brief Retrieve the entry deadline.
             *
             * @return The timestamp (in milliseconds) of the next attempt deadline.
             */
            unsigned long getDeadline() const
            {
                return this->nextAttemptDeadline;
            }

            /**
             * @brief Mark the entry as completed.
             *
             * This sets the number of attempts to @ref COAP_MAX_RETRANSMIT.
             */
            void setAsCompleted()
            {
                this->attempts = COAP_MAX_RETRANSMIT;
            }

            /**
             * @brief Retransmit the message using the given Node instance.
             *
             * This sends the message and increments the attempts counter.
             * It also schedules the next timeout interval as per protocol specifications.
             *
             * @param udp The UDP instance used for retransmission.
             * @return An error code indicating success or failure.
             */
            ErrorCode retransmit(UDP *udp);
        };

        /**
         * @brief Class to track outgoing confirmable messages.
         *
         * This is used by @ref Node::loop to implement retransmission as per specifications.
         */
        class RetransmissionQueue
        {
        private:
            // Array of retransmission entries.
            RetransmissionEntry _entries[COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE];

        public:
            RetransmissionQueue() = default;

            /**
             * @brief Add a new entry to the retransmission queue.
             *
             * @param message The CoAP message to be retransmitted.
             * @param ip The destination IP address.
             * @param port The destination UDP port.
             * @return An error code indicating success or failure.
             *         It returns @ref ErrorCode::NONE on success.
             *         It returns @ref ErrorCode::NOT_SUPPORTED if the queue is full.
             *         Increase @ref COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE to allow more entries.
             */
            ErrorCode add(const Coap::Message &message, IPAddress ip, uint16_t port);

            /**
             * @brief Process the retransmission queue.
             *
             * @param udp The UDP instance used for retransmission.
             *
             * @return An error code indicating success or failure. It will return
             *        @ref ErrorCode::NONE on success (even if no retransmissions were necessary).
             */
            ErrorCode process(UDP *udp);

            /**
             * @brief Match a received response to an outstanding request.
             *
             * If the response is not of type @ref MessageType::ACK or @ref MessageType::RST,
             * this function exits immediately.
             * See https://datatracker.ietf.org/doc/html/rfc7252#section-2.1
             * If a matching entry is found, it is marked as completed and removed from the queue.
             *
             * @param response The received CoAP response message.
             */
            void matchResponse(const Coap::Message &response);
        };
    }

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
        // Retransmission queue for confirmable messages.
        Detail::RetransmissionQueue _retransmissionQueue;

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
        Node(UDP &udp, uint16_t port) : _udp(&udp), _port(port), _responseHandler(nullptr), _retransmissionQueue() {}

        /**
         * @brief Start the CoAP instance.
         *
         * It starts the underlying UDP instance, enabling communication.
         * The UDP instance is bound to the local port specified at construction time.
         *
         * @returns ErrorCode::NONE on success, or an error code on failure.
         */
        ErrorCode start();

        /**
         * @brief Set the response callback.
         *
         * The response handler is invoked when a message of type @ref MessageType::ACK or @ref MessageType::RST
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
         *
         * The callback is invoked when a request message targeting the given path is received.
         *
         * @param path The URI path to serve with **no leading slash**,
         *             **no trailing slash** and no other special characters.
         *             If the path already exists, the callback is updated.
         *             Paths are *case-sensitive.
         *             Examples of valid paths are:
         *                 - `test`
         *                 - `sensors/temp`
         * @param callback The callback function to handle requests to the given path.
         *
         * @return An error code indicating success or failure.
         *
         * @warning A path that does not respect the format won't work.
         * @warning The path string pointer must remain valid for the entire lifetime
         *          of the registry. It is recommended to use constant strings.
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

        /**
         * @brief Send a CoAP message to the specified IP address and port.
         *
         * If the message is of type @ref COAP_CONFIRMABLE, it is added to the retransmission queue.
         * The retransmission queue behaves as per RFC 7252 and is processed in the @ref loop() method.
         * Its behaviour can be configured via @ref COAP_CONFIRMABLE_MESSAGE_QUEUE_SIZE,
         * @ref COAP_MAX_RETRANSMIT and @ref COAP_ACK_TIMEOUT_MS.
         *
         * @param message The CoAP message to send.
         * @param ip The destination IP address.
         * @param port The destination UDP port. This may be different from the local or the default port.
         * @return An error code indicating success or failure.
         */
        ErrorCode sendMessage(const Message &message, IPAddress ip, uint16_t port);
    };

} // End of namespace Coap

#endif
