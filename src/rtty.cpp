#include <interrupts.h>
#include <rtty.h>

RC PayloadLayer::getByte(byte& val)
{
  if (offset == payloadLength)
  {
    return RC_FINISHED;
  }
  val = payload[offset];
  offset++;
  return RC_OK;
}

RC PayloadLayer::setPayload(byte const* buf, Length len)
{
  volatile InterruptGuard guard;
  if (offset != payloadLength)
  {
    return RC_CONTINUE;
  }
  payloadLength = (len < sizeof(payload) ? len : sizeof(payload));
  offset = 0;
  memcpy(payload, buf, payloadLength);
  return RC_OK;
}

RC SignalLayer::getSignal(bool& value)
{
  if (offset == OFFSET_START)
  {
    auto rc = payloadLayer->getByte(curByte);
    if (rc != RC_OK)
    {
      return rc;
    }
    value = false;
  }
  else if (offset >= OFFSET_DATA0 && offset < OFFSET_STOP1)
  {
    value = (curByte >> (offset - OFFSET_DATA0)) & 1;
  }
  else if (offset >= OFFSET_STOP1)
  {
    value = true;
  }
  offset = (offset < OFFSET_STOP2) ? offset + 1 : OFFSET_START;
  return RC_OK;
}