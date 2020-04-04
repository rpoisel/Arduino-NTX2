#include <Arduino.h>
#include <string.h>
#include <util/crc16.h>

using PinID = uint8_t;
using Length = uint8_t;
using Offset = uint8_t;
using Period = unsigned long; // millis(), micros(), ...
using RC = uint8_t;

constexpr static PinID const RADIOPIN = 2;

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

class Task
{
  public:
  Task() : active(false), lastRun(0), period(0) {}
  virtual ~Task() {}
  virtual void run() = 0;
  void execute();
  void schedule(Period when);

  private:
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  bool active;
  Period lastRun;
  Period period;
};

class TaskSend : public Task
{
  public:
  TaskSend() {}
  virtual void run();

  private:
  TaskSend(TaskSend const&) = delete;
  TaskSend& operator=(TaskSend const&) = delete;
};

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

void Task::execute()
{
  if (!active)
  {
    return;
  }
  auto curTime = millis();
  if (curTime - lastRun < period)
  {
    return;
  }

  lastRun = curTime;
  active = false;
  run();
}

void Task::schedule(Period when)
{
  period = when;
  active = true;
}

static TaskSend taskSend;
static Task* const tasks[] = {&taskSend};
static PayloadLayer payloadLayer;
static SignalLayer signalLayer(&payloadLayer);

void TaskSend::run()
{
  constexpr char const* const TEST_STR = "RTTY TEST BEACON";
  char payload[64];
  auto curTime = millis();
  snprintf(payload, sizeof(payload), "$$%s %lu", TEST_STR, curTime);
  auto checksum = gps_CRC16_checksum(payload);
  auto length = snprintf(payload, sizeof(payload), "$$%s %lu*%04X\n", TEST_STR, curTime, checksum);
  auto rc = payloadLayer.setPayload(reinterpret_cast<byte const*>(payload), length);

  this->schedule(rc == RC_OK ? 1000 : 100);
}

void setup()
{
  pinMode(RADIOPIN, OUTPUT);

  setupTimer();
  taskSend.schedule(1000);
}

void loop()
{
  for (auto task : tasks)
  {
    task->execute();
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

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0; // initialize counter value to 0
  OCR1A = (INTERNAL_CLOCK * TX_PERIOD_US) / (T1_PRESCALER * 1e6) - 1; // set compare match register
  TCCR1B |= (1 << WGM12);                                             // turn on CTC mode
  TCCR1B |= (1 << CS11);   // Set CS11 bit for 8 prescaler
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
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
