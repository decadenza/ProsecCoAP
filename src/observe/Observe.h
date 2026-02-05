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
     * @brief Coap Observe option values.
     *
     * @note On an observe request, the client includes the Observe option
     *       with either the register or deregister value.
     *       On a notification, the server includes the Observe option with a
     *       sequential number value. Such value is a 24 bit unsigned integer
     *       and shall not be confused with the Observe option value in the request.
     *
     * @see https://datatracker.ietf.org/doc/html/rfc7641#section-2
     */
    enum class ObserveValue : uint8_t
    {
        REGISTER = 0,
        DEREGISTER = 1
    };

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

}
#endif // OBSERVE_H_INCLUDED