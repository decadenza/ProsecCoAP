/**
 * @file Observe.h
 *
 * @brief Header file for the Observe functionality for ProsecCoAP.
 *
 * It extends the main ProsecCoAP library with support for the Observe mechanism as per RFC 7641.
 */
#ifndef OBSERVE_H_INCLUDED
#define OBSERVE_H_INCLUDED

#include "../Types.h"

namespace Coap
{

    /**
     * @namespace ObserveValue
     * @brief Represents Observe option values.
     */
    namespace ObserveValue
    {
        /**
         * @brief The register value for the Observe option.
         *
         * @note On an observe request, the client includes the Observe option
         *       with either the register or deregister value.
         *       On a notification, the server includes the Observe option with a
         *       sequential number value. Such value is a 24 bit unsigned integer
         *       and shall not be confused with the Observe option value in the request.
         *
         * @see https://datatracker.ietf.org/doc/html/rfc7641#section-2
         */
        constexpr uint8_t REGISTER = 0;
        /**
         * @brief The deregister value for the Observe option.
         * @see @ref REGISTER.
         */
        constexpr uint8_t DEREGISTER = 1;
    }

    /**
     * @defgroup Observe-related functions.
     * @brief Functions related to the Observe mechanism.
     * @{
     */

    /**
     * @brief Extract the Observe option value from a CoAP message, if present.
     *
     * On a request, the observe value is either the register or deregister value, @ref ObserveValue.
     * On a notification, the observe value is a 24-bit sequential number that is incremented for each
     * notification sent to an observer.
     *
     * @param message The CoAP message to extract the Observe option value from.
     * @param observeValue The output parameter to store the Observe option value.
     *                     The Observe option value is a 24-bit unsigned integer, but
     *                     it is stored in a 32-bit variable.
     *                     The value is valid only if the function returns @ref ErrorCode::OK.
     * @return An error code indicating success or failure.
     *         It returns @ref ErrorCode::OK if the Observe option is present and the value is successfully extracted.
     *         It returns @ref ErrorCode::NOT_FOUND if the Observe option is not present in the message.
     *         Other error codes may be returned.
     */
    ErrorCode getObserveValue(const Message &message, uint32_t &observeValue);

    /**
     * @brief Check if the message is an Observe register GET request.
     *
     * As per https://datatracker.ietf.org/doc/html/rfc7641#section-3.1
     * an Observe register request is a GET request that includes the Observe option
     * with the value @ref ObserveValue::REGISTER.
     *
     * The caller has the responsibility to register the observer to the specific resource.
     *
     * @param message The CoAP message to check.
     * @return True if the message is an Observe register request, false in all other cases.
     */
    bool isObserveRegister(const Message &message);

    /**
     * @brief Check if the message is an Observe deregister GET request.
     *
     * A client may explicitly cancel an observation relationship by sending a
     * GET request with the Observe option set to @ref ObserveValue::DEREGISTER.
     *
     * The caller has the responsibility to cancel the observer if all other conditions
     * are met (e.g. Uri-Path, token, etc.).
     *
     * @param message The CoAP message to check.
     * @return True if the message is an Observe deregister request, false in all other cases.
     */
    bool isObserveDeregister(const Message &message);

    /** @} */ // End of Functions group

    /**
     * @brief A remote CoAP observer, activelly observing a resource.
     *
     * It tracks a remote observer registered for notifications.
     *
     * @todo Add support to clean up older observers.
     */
    class Observer
    {
    private:
        /**
         * @brief Indicates whether the observer is currently active.
         */
        bool _active : 8;

        /**
         * @brief The sequential number for notifications, as per specifications.
         *
         * The sequential number must be incremented by 1 for each notification
         * sent to the observer.
         * It will wrap around to zero after reaching the maximum value of 24 bits (0xFFFFFF).
         *
         * @note Only the least significant 24 bits are used.
         *       https://datatracker.ietf.org/doc/html/rfc7641#section-4.4
         */
        uint32_t _observationSequentialNumber : 24;

        /**
         * @brief The IP address of the observer.
         */
        IPAddress _ip;
        /**
         * @brief The port of the observer.
         */
        uint16_t _port = 0;
        /**
         * @brief The token used by the observer.
         *
         * The token is used by the remote node to match
         * the notifications to the original request.
         */
        uint8_t _token[COAP_MAX_TOKEN_LENGTH] = {0};
        /**
         * @brief The length of the token.
         *
         * All notifications sent to the observer will use the same token and token length.
         * Note that a value of zero may still be valid.
         */
        uint8_t _tokenLength = 0;

    public:
        /**
         * @brief Default constructor.
         */
        Observer() : _observationSequentialNumber(0) {}

        /**
         * @brief Get the IP address of the observer.
         * @return The IP address.
         */
        IPAddress getIp() const
        {
            return this->_ip;
        }

        /**
         * @brief Get the port of the observer.
         * @return The port number.
         */
        uint16_t getPort() const
        {
            return this->_port;
        }

        /**
         * @brief Get the token pointer used by the observer.
         * @return Pointer to the token.
         */
        const uint8_t *getToken() const
        {
            return this->_token;
        }

        /**
         * @brief Get the token length used by the observer.
         * @return The token length in bytes.
         */
        uint8_t getTokenLength() const
        {
            return this->_tokenLength;
        }
    };

    /**
     * @brief A resource observer table.
     *
     * It tracks the observers registered for a specific resource.
     *
     * @todo Add support to clean up older observers.
     */
    template <size_t N>
    class ObserverRegistry
    {
    private:
        /**
         * @brief Array of observers.
         */
        Observer _observers[N];

    public:
        /**
         * @brief Get the observer at the given index.
         * @param index
         * @return Reference to the observer.
         */
        Observer &operator[](size_t index)
        {
            return _observers[index];
        }

        /**
         * @brief Get the observer at the given index.
         * @param index
         * @return Const reference to the observer.
         */
        const Observer &operator[](size_t index) const
        {
            return _observers[index];
        }

        /**
         * @brief Add a new observer to the registry.
         *
         * @param ip The IP address of the observer.
         * @param port The port of the observer.
         * @param token The token used by the observer.
         * @param tokenLength The length of the token in bytes.
         * @return An error code indicating success or failure. It will return @ref ErrorCode::OK
         *         if the observer is successfully added.
         *         It will return @ref ErrorCode::NOT_SUPPORTED if the registry is full
         *         and the observer cannot be added.
         */
        ErrorCode add(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength);

        ErrorCode remove(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength);

        ErrorCode remove(const Observer &observer);
    };
}
#endif // OBSERVE_H_INCLUDED