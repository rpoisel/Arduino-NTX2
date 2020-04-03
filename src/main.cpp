#include <Arduino.h>
#include <string.h>
#include <util/crc16.h>

using Length = uint8_t;
using Offset = uint8_t;
using RC = uint8_t;

static constexpr uint8_t const RADIOPIN = 2;

constexpr static RC const RC_OK = 0x00;
constexpr static RC const RC_FINISHED = 0x01;
constexpr static RC const RC_CONTINUE = 0x02;
constexpr static RC const RC_ERROR = 0x80;

static void setupTimer();
static uint16_t gps_CRC16_checksum(char const* string);
constexpr bool isRcError(RC rc) { return (rc & RC_ERROR) == RC_ERROR; }
constexpr bool isRcOk(RC rc) { return !isRcError(rc); }

class InterruptGuard final
{
  public:
  InterruptGuard() { noInterrupts(); }
  ~InterruptGuard() { interrupts(); }

  private:
  InterruptGuard(InterruptGuard const&) = delete;
  InterruptGuard& operator=(InterruptGuard const&) = delete;
};

class PayloadLayer
{
  public:
  PayloadLayer() : payload{}, payloadLength(0), offset(0) {}
  RC getByte(byte& val)
  {
    if (offset == payloadLength)
    {
      return RC_FINISHED;
    }
    val = payload[offset];
    offset++;
    return RC_OK;
  }

  RC setPayload(byte const* buf, Length len)
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

  private:
  constexpr static size_t const MAX_PAYLOAD_SIZE = 64;
  byte payload[MAX_PAYLOAD_SIZE];
  Length payloadLength;
  Offset offset;
};

// encodes to 7 bits ASCII and includes start-/stop-bits
class SignalLayer
{
  public:
  SignalLayer(PayloadLayer* payloadLayer) : payloadLayer(payloadLayer), offset(0), curByte(0) {}
  RC getSignal(bool& value)
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

  private:
  constexpr static Offset const OFFSET_START = 0;
  constexpr static Offset const OFFSET_DATA0 = 1;
  constexpr static Offset const OFFSET_STOP1 = 8;
  constexpr static Offset const OFFSET_STOP2 = 9;

  PayloadLayer* payloadLayer;
  Offset offset;
  byte curByte;
};

class Task
{
  public:
  Task() {}
  virtual ~Task() {}
  virtual void run() = 0;

  private:
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;
};

class TaskSend : public Task
{
  public:
  TaskSend() : lastRun(0) {}
  virtual void run();

  private:
  constexpr static unsigned long const PERIOD_MS = 1000;
  unsigned long lastRun;
};

static TaskSend taskSend;
static Task* const tasks[] = {&taskSend};
static PayloadLayer payloadLayer;
static SignalLayer signalLayer(&payloadLayer);

void TaskSend::run()
{
  auto curTime = millis();
  if (curTime - lastRun < PERIOD_MS)
  {
    return;
  }
  lastRun = curTime;

  String payload("RTTY TEST BEACON RTTY TEST BEACON");
  unsigned int CHECKSUM = gps_CRC16_checksum(payload.c_str());
  char checksum_str[6];
  snprintf(checksum_str, sizeof(checksum_str), "*%04X\n", CHECKSUM);
  payload += checksum_str;
  payloadLayer.setPayload(reinterpret_cast<byte const*>(payload.c_str()), payload.length());
}

void setup()
{
  pinMode(RADIOPIN, OUTPUT);

  setupTimer();
}

void loop()
{
  for (auto task : tasks)
  {
    task->run();
  }
}

ISR(TIMER1_COMPA_vect)
{
  bool value;
  auto rc = signalLayer.getSignal(value);

  if (rc == RC_OK)
  {
    digitalWrite(RADIOPIN, value ? HIGH : LOW);
  }
}

static void setupTimer()
{
  constexpr static uint64_t const TX_PERIOD_US = UINT64_C(20150); // hz/baud
  constexpr static uint64_t const T1_PRESCALER = UINT64_C(8);
  constexpr static uint64_t const INTERNAL_CLOCK = F_CPU; // hz

  volatile InterruptGuard guard;

  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1 = 0;  // initialize counter value to 0
  // set compare match register for 50hz increments
  OCR1A = (INTERNAL_CLOCK * TX_PERIOD_US) / (T1_PRESCALER * 1e6) - 1;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS11 bit for 8 prescaler
  TCCR1B |= (1 << CS11);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
}

static uint16_t gps_CRC16_checksum(char const* string)
{
  size_t i;
  uint16_t crc;
  uint8_t c;

  crc = 0xFFFF;

  // Calculate checksum ignoring the first two $s
  for (i = 2; i < strlen(string); i++)
  {
    c = string[i];
    crc = _crc_xmodem_update(crc, c);
  }

  return crc;
}
