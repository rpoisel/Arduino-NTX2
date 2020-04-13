#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

using PinID = uint8_t;
using RC = uint8_t;

constexpr static RC const RC_OK = 0x00;
constexpr static RC const RC_FINISHED = 0x01;
constexpr static RC const RC_CONTINUE = 0x02;
constexpr static RC const RC_ERROR = 0x80;

constexpr bool isRcError(RC rc) { return (rc & RC_ERROR) == RC_ERROR; }
constexpr bool isRcOk(RC rc) { return !isRcError(rc); }

#endif /* UTIL_H */
