#include "Observers.h"

namespace Coap
{

    uint32_t Observer::getNextSequentialNumber()
    {
        unsigned long currentTime = millis();
        return currentTime & 0xFFFFFF; // Return only the least significant 24 bits.
    }

    void Observer::setActive(bool active)
    {
        this->_active = active;
        if (active)
        {
            // If the observer is set as active, update the last seen timestamp to the current time,
            // as we assume that the observer is set as active after the registration is confirmed.
            this->_lastSeen = millis();
        }
    }

    bool Observer::isStale() const
    {
        if (!this->_active)
        {
            // Inactive observers are not considered stale.
            return false;
        }

        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - this->_lastSeen; // Wrapping is safe.

        // Check if the elapsed time since the last seen timestamp exceeds 24 hours (in milliseconds).
        return elapsedTime > (24UL * 60UL * 60UL * 1000UL);
    }
}