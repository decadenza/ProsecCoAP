/**
 * @file Observe.h
 *
 * @brief Header file that allows to manage observers for the Observe mechanism in ProsecCoAP.
 *
 * It extends the main ProsecCoAP library with helpers objects to support the Observe mechanism as per RFC 7641.
 */
#ifndef OBSERVERS_H_INCLUDED
#define OBSERVERS_H_INCLUDED

#include "../Types.h"

namespace Coap
{

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
        Observer() : _active(false), _observationSequentialNumber(0) {}

        /**
         * @brief Check if the observer is currently active.
         * @return True if the observer is active, false otherwise.
         */
        bool isActive() const
        {
            return this->_active;
        }

        /**
         * @brief Set the observer as active or inactive.
         * @param active True to set the observer as active, false to set it as inactive.
         */
        void setActive(bool active)
        {
            this->_active = active;
        }

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
     * @brief A resource observer registry.
     *
     * It tracks the observers registered to a specific resource.
     *
     * Example:
     * @code{.cpp}
     * Coap::ObserverRegistry<5> myRegistry; // Create a registry with a maximum of 5 observers.
     * myRegistry.add(ip1, port1, token1, tokenLength1); // Add an observer to the registry.
     * Observer &observer = myRegistry[0]; // Get the first observer in the registry.
     * @endcode
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
#endif // OBSERVERS_H_INCLUDED