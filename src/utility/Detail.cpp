#include "Detail.h"
#include "Arduino.h"

namespace Coap::Detail
{
    unsigned long getRandomTimeout()
    {
        return (unsigned long)random(COAP_ACK_MIN_TIMEOUT_MS, COAP_ACK_MAX_TIMEOUT_MS);
    }

    ErrorCode UriRegistry::add(const char *path, Callback callback)
    {
        if (path == nullptr)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }
        // Note that "" (empty path) is a valid path.
        if (this->_count >= COAP_MAX_CALLBACKS)
        {
            return ErrorCode::NOT_SUPPORTED; // Registry full.
        }
        // Check for duplicates. If a duplicate exists, replace it.
        for (size_t i = 0; i < this->_count; i++)
        {
            if (strcmp(this->_path[i], path) == 0)
            {
                // Duplicate found. Replace the callback.
                this->_callback[i] = callback;
                return ErrorCode::NONE;
            }
        }
        // Else, add the new entry.
        this->_path[this->_count] = path;
        this->_callback[this->_count] = callback;
        this->_count++;
        return ErrorCode::NONE;
    }

    ErrorCode UriRegistry::find(const char *path, Callback &callback) const
    {
        if (path == nullptr)
        {
            return ErrorCode::INVALID_ARGUMENT;
        }
        for (size_t i = 0; i < this->_count; i++)
        {
            if (strcmp(this->_path[i], path) == 0)
            {
                // Found the entry.
                callback = this->_callback[i];
                return ErrorCode::NONE;
            }
        }
        // Not found.
        return ErrorCode::NOT_FOUND;
    }
}