/**
 * @file Observers.h
 *
 * @brief Manage observers for the observe mechanism of CoAP.
 *
 * It extends the main ProsecCoAP library with helpers objects to support the Observe mechanism as per RFC 7641.
 */
#ifndef OBSERVERS_H_INCLUDED
#define OBSERVERS_H_INCLUDED

#include <Arduino.h>
#include "Definitions.h"

namespace Coap
{
    /**
     * @brief A remote CoAP observer, actively observing a resource.
     *
     * It tracks a remote observer registered for notifications.
     */
    class Observer
    {
    private:
        /**
         * @brief Indicates whether the observer is currently active.
         */
        bool _active : 8;

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
        /**
         * @brief Last seen timestamp in milliseconds.
         */
        unsigned long _lastSeen = 0;

    public:
        /**
         * @brief Default constructor that creates an inactive observer.
         */
        Observer() : _active(false) {}

        /**
         * @brief Constructor that creates an active observer with the given parameters.
         *
         * @param ip The IP address of the observer.
         * @param port The port of the observer.
         * @param token The token used by the observer.
         * @param tokenLength The length of the token in bytes.
         */
        Observer(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength)
            : _ip(ip), _port(port), _tokenLength(tokenLength)
        {
            // Copy the token value, ensuring that we do not exceed the maximum token length.
            memcpy(this->_token, token, tokenLength > COAP_MAX_TOKEN_LENGTH ? COAP_MAX_TOKEN_LENGTH : tokenLength);
            this->setActive(true);
        }

        /**
         * @brief Check if the observer is currently active.
         * @return True if the observer is active, false otherwise.
         *
         * Inactive observers are ignored by the registry and their slot can be reused to store new observers.
         */
        bool isActive() const
        {
            return this->_active;
        }

        /**
         * @brief Check if the observer is considered stale.
         * @return True if the observer is stale, false otherwise.
         *
         * An observer is considered stale if it has not been seen for more than 24 hours.
         * @see https://datatracker.ietf.org/doc/html/rfc7641#section-4.5
         */
        bool isStale() const;

        /**
         * @brief Set the observer as active or inactive.
         * @param active True to set the observer as active, false to set it as inactive.
         *
         * Setting an observer as active also updates its internal last seen value.
         * Inactive observers are ignored by the registry and their slot can be reused to store new observers.
         */
        void setActive(bool active);

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

        /**
         * @brief The sequential number for notifications, as per specifications.
         *
         * As per https://datatracker.ietf.org/doc/html/rfc7641#section-4.4,
         * the sequence number MAY start at any value and MUST NOT increase so fast
         * that it increases by more than 2^23 within less than 256 seconds.
         *
         * As per implementation notes, using the local clock (e.g. millis()) is a
         * simple way to meet this requirement.
         *
         * @note Only the least significant 24 bits are used.
         */
        uint32_t getNextSequentialNumber();
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
     */
    template <size_t N>
    class ObserverRegistry
    {
        // Ensure N is at least 1.
        static_assert(N >= 1, "ObserverRegistry Error: N must be 1 or greater!");

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
         * @brief Get the maximum number of observers that can be stored in the registry.
         *
         * @return The maximum number of entries.
         */
        size_t length() const
        {
            return N;
        }

        /**
         * @brief Get the number of active observers currently stored in the registry.
         *
         * @return The number of active observers.
         */
        size_t countActive() const
        {
            size_t count = 0;
            for (size_t i = 0; i < N; i++)
            {
                if (this->_observers[i].isActive())
                    count++;
            }
            return count;
        }

        /**
         * @brief Add a new observer to the registry.
         *
         * If the observer already exists and is active, it will not be added again.
         *
         * @param ip The IP address of the observer.
         * @param port The port of the observer.
         * @param token The token used by the observer.
         * @param tokenLength The length of the token in bytes.
         *
         * @see add(Observer observer) for the version that accepts an Observer object.
         */
        ErrorCode add(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength)
        {
            Observer new_observer(ip, port, token, tokenLength);
            return this->add(new_observer);
        }

        /**
         * @brief Add a new observer to the registry.
         *
         * If the observer already exists and is active, it will not be added again.
         *
         * @param observer The observer to be added.
         * @return An error code indicating success or failure. It will return @ref ErrorCode::OK
         *         if the observer is successfully added (or was already present).
         *         It will return @ref ErrorCode::NOT_SUPPORTED if the registry is full
         *         and the observer cannot be added.
         */
        ErrorCode add(Observer observer)
        {
            if (!observer.isActive())
            {
                // Inactive observer cannot be added to the registry.
                return ErrorCode::INVALID_ARGUMENT;
            }

            // We MUST check if the observer is already present before trying to add it, to avoid duplicates.
            for (size_t i = 0; i < N; i++)
            {
                if (!this->_observers[i].isActive())
                    continue; // Skip inactive observers.

                // An observer is considered the same if it matches the combination of IP address, port, token and token length.
                if (this->_observers[i].getIp() == observer.getIp() &&
                    this->_observers[i].getPort() == observer.getPort() &&
                    this->_observers[i].getTokenLength() == observer.getTokenLength() &&
                    memcmp(this->_observers[i].getToken(), observer.getToken(), observer.getTokenLength()) == 0)
                {
                    // Matching active observer found. No need to add it again.
                    return ErrorCode::OK;
                }
            }

            // New observer.
            // Find the first inactive observer slot and add the new observer there.
            for (size_t i = 0; i < N; i++)
            {
                if (!this->_observers[i].isActive())
                {
                    // Inactive slot found. Add the new observer here (active by default, see constructor).
                    this->_observers[i] = observer;
                    return ErrorCode::OK;
                }
            }

            return ErrorCode::NOT_SUPPORTED;
        }

        /**
         * @brief Remove an observer from the registry.
         *
         * The observer must match the provided combination of IP address, port, token and token length.
         *
         * @param ip The IP address of the observer that needs to be removed.
         * @param port The port of the observer.
         * @param token The token used by the observer.
         * @param tokenLength The length of the token in bytes.
         * @return An error code indicating success or failure.
         *         It will return @ref ErrorCode::OK if the observer is successfully removed.
         *         It will return @ref ErrorCode::NOT_FOUND if no matching observer is found
         */
        ErrorCode remove(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength)
        {
            // Find the observer matching the given parameters and remove it.
            for (size_t i = 0; i < N; i++)
            {
                if (!this->_observers[i].isActive())
                    continue; // Skip inactive observers.
                if (this->_observers[i].getIp() == ip &&
                    this->_observers[i].getPort() == port &&
                    this->_observers[i].getTokenLength() == tokenLength &&
                    memcmp(this->_observers[i].getToken(), token, tokenLength) == 0)
                {
                    // Matching observer found. Remove it by marking it as inactive.
                    this->_observers[i].setActive(false);
                    return ErrorCode::OK;
                }
            }
            return ErrorCode::NOT_FOUND;
        }

        /**
         * @brief Remove any observers with the given combination of IP and port.
         *
         * @param ip The IP address of the observer(s) to remove.
         * @param port The port of the observer(s).
         * @return An error code indicating success or failure.
         *         It will return @ref ErrorCode::OK if one or more observers have been removed.
         *         It will return @ref ErrorCode::NOT_FOUND if no matching observer is found
         *
         * @see remove(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength) for the token-specific version.
         */
        ErrorCode remove(IPAddress ip, uint16_t port)
        {
            ErrorCode result = ErrorCode::NOT_FOUND;
            // Find the observer matching the given parameters and remove it.
            for (size_t i = 0; i < N; i++)
            {
                if (!this->_observers[i].isActive())
                    continue; // Skip inactive observers.
                if (this->_observers[i].getIp() == ip &&
                    this->_observers[i].getPort() == port)
                {
                    // Matching observer found. Remove it by marking it as inactive.
                    this->_observers[i].setActive(false);
                    result = ErrorCode::OK;
                }
            }
            return result;
        }

        /**
         * @brief Remove any observers with the given IP.
         *
         * @param ip The IP address of the observer(s) to remove.
         * @return An error code indicating success or failure.
         *         It will return @ref ErrorCode::OK if one or more observers have been removed.
         *         It will return @ref ErrorCode::NOT_FOUND if no matching observer is found
         *
         * @see remove(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength) for the token-specific version.
         */
        ErrorCode remove(IPAddress ip)
        {
            ErrorCode result = ErrorCode::NOT_FOUND;
            // Find the observer matching the given parameters and remove it.
            for (size_t i = 0; i < N; i++)
            {
                if (!this->_observers[i].isActive())
                    continue; // Skip inactive observers.
                if (this->_observers[i].getIp() == ip)
                {
                    // Matching observer found. Remove it by marking it as inactive.
                    this->_observers[i].setActive(false);
                    result = ErrorCode::OK;
                }
            }
            return result;
        }

        /**
         * @brief Remove an observer from the registry.
         *
         * @param observer The observer to be removed.
         * @return An error code indicating success or failure.
         *
         * @see remove(IPAddress ip, uint16_t port, const uint8_t *token, uint8_t tokenLength) for more details.
         */
        ErrorCode remove(const Observer &observer)
        {
            return this->remove(observer.getIp(), observer.getPort(), observer.getToken(), observer.getTokenLength());
        }

        /**
         * @brief Process incoming responses to notifications sent to observers.
         * @return
         *
         * This is a maintenance function that must be called in the response handler
         * @ref setResponseHandler to keep observer state up to date.
         * It will update the last seen timestamp of observers that have sent a response.
         * It will also remove observers that have sent a RST response, as per specifications.
         * @see https://datatracker.ietf.org/doc/html/rfc7641#section-4.5
         *
         * @example
         */
        ErrorCode processNotificationResponse(Message &message, IPAddress ip, uint16_t port)
        {

            // TODO: Look for outstanding notifications in the _retransmissionQueue sent to observers and match the response with the observer.
            // Unclear how to do this, as the _retransmissionQueue is private to the ProsecCoAP class and not accessible from here.

            // If the response is a RST, remove the observer from the registry.
            // If the response is an ACK, update the last seen timestamp of the observer (by calling setActive(true)).
        }
    };
}

#endif // OBSERVERS_H_INCLUDED