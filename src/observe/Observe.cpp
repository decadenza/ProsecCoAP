#include "../ProsecCoAP.h"

namespace Coap
{

    ErrorCode getObserveValue(const Message &message, uint32_t &observeValue)
    {
        OptionIterator it = message.getOptionIterator();
        Option option;
        while (it.next(option) == ErrorCode::OK)
        {
            if (option.number < OptionNumber::OBSERVE)
                continue;
            if (option.number == OptionNumber::OBSERVE)
            {
                // The Observe option value may be 0-3 bytes long.
                // The protocol uses Network Byte Order (big-endian), so we need to shift the bytes accordingly.
                observeValue = 0;
                for (size_t i = 0; i < option.length && i < 3; i++)
                {
                    observeValue |= static_cast<uint32_t>(option.value[i]) << (8 * (2 - i));
                }
                return ErrorCode::OK;
            }
            else
            {
                // Since options are ordered by number, if we have passed the Observe option number,
                // it means that the Observe option is not present.
                break;
            }
        }
        return ErrorCode::NOT_FOUND;
    }

    bool isObserveRegister(const Message &message)
    {
        if (message.getCode() != MessageCode::GET)
        {
            return false;
        }
        uint32_t observeValue;
        if (getObserveValue(message, observeValue) == ErrorCode::OK)
        {
            return observeValue == ObserveValue::REGISTER;
        }
        return false;
    }

    bool isObserveDeregister(const Message &message)
    {
        if (message.getCode() != MessageCode::GET)
        {
            return false;
        }
        uint32_t observeValue;
        if (getObserveValue(message, observeValue) == ErrorCode::OK)
        {
            return observeValue == ObserveValue::DEREGISTER;
        }
        return false;
    }
}