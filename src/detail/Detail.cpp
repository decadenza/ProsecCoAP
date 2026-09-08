#include "Detail.h"
#include "../ProsecCoAP.h"
#include "Arduino.h"

namespace Coap
{
    namespace Detail
    {
        ErrorCode sendUdp(UDP *udp, const uint8_t *data, size_t length, IPAddress ip, uint16_t port)
        {
            udp->beginPacket(ip, port);
            size_t written = udp->write(data, length);
            if (written != length)
            {
                // Not all bytes were written to the UDP buffer.
                return ErrorCode::NETWORK;
            }
            if (udp->endPacket() == 1) // Returns 1 if the packet was sent successfully, 0 if there was an error.
            {
                return ErrorCode::OK;
            }
            else
            {
                return ErrorCode::NETWORK;
            }
        }

        unsigned long getRandomTimeout()
        {
            return (unsigned long)random(COAP_ACK_MIN_TIMEOUT_MS, COAP_ACK_MAX_TIMEOUT_MS);
        }

        size_t getMinOptionBytes(uint32_t value)
        {
            if (value == 0)
                return 0; // Zero can be represented with 0 bytes.

            uint32_t leadingZeros = __builtin_clz(value); // Count the leading zeros.
            // Calculate the minimum number of bytes to represent the value.
            // 0-7 leading zeros => 4 bytes
            // 8-15 => 3
            // 16-23 => 2
            // 24-31 => 1
            return (32 - leadingZeros + 7) / 8;
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
                    return ErrorCode::OK;
                }
            }
            // Else, add the new entry.
            this->_path[this->_count] = path;
            this->_callback[this->_count] = callback;
            this->_count++;
            return ErrorCode::OK;
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
                    return ErrorCode::OK;
                }
            }
            // Not found.
            return ErrorCode::NOT_FOUND;
        }

        ErrorCode UriRegistry::find(const Message &message, Callback &callback) const
        {
            struct Candidate
            {
                const char *path;
                size_t pathOffset;
                bool possible;
            };

            OptionIterator options = message.getOptionIterator();
            Option option;
            ErrorCode err;
            Candidate candidates[COAP_MAX_CALLBACKS];

            for (size_t i = 0; i < this->_count; i++)
            {
                candidates[i] = {this->_path[i], 0, true};
            }

            while (true)
            {
                do
                {
                    err = options.next(option);
                } while (err == ErrorCode::OK && option.number < OptionNumber::URI_PATH);

                if (err == ErrorCode::NOT_FOUND)
                {
                    break;
                }

                if (err != ErrorCode::OK)
                {
                    return err;
                }

                if (option.number != OptionNumber::URI_PATH)
                {
                    break;
                }

                // Compare this URI-Path option with every candidate that has
                // matched all preceding options.
                for (size_t i = 0; i < this->_count; i++)
                {
                    Candidate &candidate = candidates[i];
                    if (!candidate.possible)
                    {
                        continue;
                    }

                    if (candidate.path[candidate.pathOffset] == '\0')
                    {
                        // The message contains another segment after the
                        // registered path has already been consumed.
                        candidate.possible = false;
                        continue;
                    }

                    size_t segmentIndex = 0;
                    while (segmentIndex < option.length)
                    {
                        char pathByte = candidate.path[candidate.pathOffset];
                        char optionByte = static_cast<char>(option.value[segmentIndex]);
                        if (pathByte == '/' || pathByte != optionByte)
                        {
                            candidate.possible = false;
                            break;
                        }
                        candidate.pathOffset++;
                        segmentIndex++;
                    }

                    if (candidate.possible)
                    {
                        char nextPathByte = candidate.path[candidate.pathOffset];
                        if (nextPathByte == '/')
                        {
                            candidate.pathOffset++;
                        }
                        else if (nextPathByte != '\0')
                        {
                            candidate.possible = false;
                        }
                    }
                }
            }

            for (size_t i = 0; i < this->_count; i++)
            {
                Candidate &candidate = candidates[i];
                if (candidate.possible &&
                    candidate.path[candidate.pathOffset] == '\0')
                {
                    callback = this->_callback[i];
                    return ErrorCode::OK;
                }
            }

            return ErrorCode::NOT_FOUND;
        }
    }
}