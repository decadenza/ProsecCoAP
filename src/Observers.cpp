#include "Observers.h"

namespace Coap
{

    uint32_t Observer::getNextSequentialNumber()
    {
        unsigned long currentTime = millis();
        return currentTime & 0xFFFFFF; // Return only the least significant 24 bits.
    }

    void Observer::activate()
    {
        this->_active = true;
        // Update the last seen timestamp to the current time.
        this->_lastSeen = millis();
    }

    void Observer::deactivate()
    {
        this->_active = false;
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

        // Check if the elapsed time since the last seen timestamp is 24 hours or more (in milliseconds).
        return elapsedTime >= (24UL * 60UL * 60UL * 1000UL);
    }
}