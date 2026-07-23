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

        // As per RFC 7641 Section 4.5, an observer is stale if not seen for 24 hours.
        // Using unsigned arithmetic which handles millis() overflow correctly.
        const unsigned long staleThreshold = 24UL * 60UL * 60UL * 1000UL;
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - this->_lastSeen; // The unsigned subtraction correctly handles millis() overflow (every ~49.7 days).

        return elapsedTime >= staleThreshold;
    }
}