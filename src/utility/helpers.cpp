#include "../ProsecCoAP.h"
#include "helpers.h"

// uint8_t helpers::encodeUintOption(uint32_t value, uint8_t out[3])
// {
//     if (value == 0)
//     {
//         // Special case: zero is encoded as a zero-length option.
//         // https://datatracker.ietf.org/doc/html/rfc7252#section-3.2
//         return 0;
//     }
//     if (value <= 0xFF)
//     {
//         out[0] = (uint8_t)value;
//         return 1;
//     }
//     if (value <= 0xFFFF)
//     {
//         out[0] = (uint8_t)(value >> 8);
//         out[1] = (uint8_t)(value & 0xFF);
//         return 2;
//     }
//     out[0] = (uint8_t)((value >> 16) & 0xFF);
//     out[1] = (uint8_t)((value >> 8) & 0xFF);
//     out[2] = (uint8_t)(value & 0xFF);
//     return 3;
// }

// bool helpers::pathEquals(const char *a, const char *b)
// {
//     if (a == NULL || b == NULL)
//         return false;
//     return strcmp(a, b) == 0;
// }

// bool helpers::tokenEquals(const uint8_t *a, uint8_t aLength, const uint8_t *b, uint8_t bLength)
// {
//     if (aLength != bLength)
//         return false;
//     if (aLength == 0) // Both lengths are zero.
//         return true;
//     if (a == NULL || b == NULL)
//         return false;
//     return memcmp(a, b, aLength) == 0;
// }