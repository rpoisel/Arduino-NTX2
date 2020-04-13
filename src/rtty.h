#ifndef RTTY_H
#define RTTY_H

#include <util.h>

#include <Arduino.h>

using Length = uint8_t;
using Offset = uint8_t;

class PayloadLayer
{
  public:
  PayloadLayer() : payload{}, payloadLength(0), offset(0) {}
  RC getByte(byte& val);
  RC setPayload(byte const* buf, Length len);

  private:
  constexpr static size_t const MAX_PAYLOAD_SIZE = 64;
  byte payload[MAX_PAYLOAD_SIZE];
  Length payloadLength;
  Offset offset;
};

// encodes to RTTY (7 bits ASCII and includes start-/stop-bits)
class SignalLayer
{
  public:
  SignalLayer(PayloadLayer* payloadLayer) : payloadLayer(payloadLayer), offset(0), curByte(0) {}
  RC getSignal(bool& value);

  private:
  constexpr static Offset const OFFSET_START = 0;
  constexpr static Offset const OFFSET_DATA0 = 1;
  constexpr static Offset const OFFSET_STOP1 = 8;
  constexpr static Offset const OFFSET_STOP2 = 9;

  PayloadLayer* payloadLayer;
  Offset offset;
  byte curByte;
};

#endif /* RTTY_H */
