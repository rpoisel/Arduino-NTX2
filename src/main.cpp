#include <interrupts.h>
#include <rtty.h>
#include <scheduling.h>
#include <util.h>

#include <Arduino.h>
#include <string.h>
#include <util/crc16.h>

constexpr static PinID const RADIOPIN = 2;

static void setupTimer();
static uint16_t gps_CRC16_checksum(char const* string);

class TaskSend : public Task
{
  public:
  TaskSend() {}
  virtual void run();

  private:
  TaskSend(TaskSend const&) = delete;
  TaskSend& operator=(TaskSend const&) = delete;
};

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
