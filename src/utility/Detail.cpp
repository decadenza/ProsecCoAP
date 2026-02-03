#include "Detail.h"

namespace Coap::Detail
{

    ErrorCode UriRegistry::add(const char *path, Callback callback)
    {
        if (this->_count >= COAP_MAX_CALLBACKS)
        {
            return ErrorCode::NotSupported; // Registry full.
        }
        // Check for duplicates. If a duplicate exists, replace it.
        for (size_t i = 0; i < this->_count; i++)
        {
            if (strcmp(this->_path[i], path) == 0)
            {
                // Duplicate found. Replace the callback.
                this->_callback[i] = callback;
                return ErrorCode::None;
            }
        }
        // Else, add the new entry.
        this->_path[this->_count] = path;
        this->_callback[this->_count] = callback;
        this->_count++;
        return ErrorCode::None;
    }

    ErrorCode UriRegistry::find(const char *path, Callback &callback) const
    {
        for (size_t i = 0; i < this->_count; i++)
        {
            if (strcmp(this->_path[i], path) == 0)
            {
                // Found the entry.
                callback = this->_callback[i];
                return ErrorCode::None;
            }
        }
        // Not found.
        return ErrorCode::NotFound;
    }
}