#include <Arduino.h>
#include <string.h>
#include <util/crc16.h>

static constexpr uint8_t const RADIOPIN = 2;

#if 0
enum State
{
  INVALID = 0,
  SETTING,
  SENDING,
  WAITING,
};

template <class T>
class CommLayer
{
  public:
  CommLayer() : value(), state(SETTING)
  {
  }
  virtual ~CommLayer()
  {
  }
  virtual void onSet()
  {
  }
  virtual void set(T value)
  {
    if (state != SETTING)
    {
      return;
    }
    this->value = value;
    onSet();
    state = SENDING;
  }

  protected:
  T value;
  State state;
};
class TxBit : public CommLayer<bool>
{
  public:
  static constexpr bool const BIT_START = 0;
  static constexpr bool const BIT_STOP = 1;
  static constexpr unsigned long PERIOD_50_BAUD_US = 20150;
  static constexpr unsigned long JITTER_50_BAUD_US = 50;

  TxBit(unsigned long period) : period(period), timestamp(0)
  {
  }

  bool send()
  {
    switch (state)
    {
    case SENDING:
      digitalWrite(RADIOPIN, value ? HIGH : LOW);
      state = WAITING;
      timestamp = micros();
      break;
    case WAITING:
      if (micros() - timestamp >= period - JITTER_50_BAUD_US)
      {
        state = SETTING;
        return true;
      }
      break;
    default:
      return false;
    }
    return false;
  }

  private:
  unsigned long const period;
  unsigned long timestamp;
};

class TxByte : public CommLayer<char>
{
  public:
  static constexpr size_t ASCII_7_LEN = 7;
  static constexpr size_t ASCII_8_LEN = 8;

  TxByte() : bitType(START), dataOffset(0), txBit(TxBit::PERIOD_50_BAUD_US)
  {
  }

  void onSet()
  {
    txBit.set(TxBit::BIT_START);
    bitType = START;
  }

  bool send()
  {
    if (!txBit.send())
    {
      return false;
    }

    // check for previous bit type
    switch (bitType)
    {
    case START:
      txBit.set(value & 1);
      bitType = DATA;
      dataOffset = 0;
      break;
    case DATA:
      dataOffset++;
      if (dataOffset < ASCII_7_LEN)
      {
        txBit.set((value >> dataOffset) & 1);
      }
      else
      {
        txBit.set(TxBit::BIT_STOP);
        bitType = STOP1;
      }
      break;
    case STOP1:
      txBit.set(TxBit::BIT_STOP);
      bitType = STOP2;
      break;
    case STOP2:
      state = SETTING;
      return true;
    default:
      break;
    }
    return false;
  }

  private:
  enum BitType
  {
    INVALID,
    START,
    DATA,
    STOP1,
    STOP2,
  } bitType;
  size_t dataOffset;
  TxBit txBit;
};

class TxString : public CommLayer<char const*>
{
  public:
  TxString()
  {
  }

  void onSet()
  {
    curPos = value;
    txByte.set(*curPos);
  }

  bool send()
  {
    if (!txByte.send())
    {
      return false;
    }
    curPos++;
    if (*curPos == '\0')
    {
      state = SETTING;
      return true;
    }
    txByte.set(*curPos);
    return false;
  }

  private:
  TxByte txByte;
  char const* curPos;
};

class Task
{
  public:
  virtual void run()
  {
  }
};

class TaskSend : public Task
{
  public:
  TaskSend() : datastring{}, txString(), timestamp(millis()), activate(false)
  {
    snprintf(datastring, sizeof(datastring), "RTTY TEST BEACON RTTY TEST BEACON");
    unsigned int CHECKSUM = gps_CRC16_checksum(datastring);
    char checksum_str[6];
    snprintf(checksum_str, sizeof(checksum_str), "*%04X\n", CHECKSUM);
    strncat(datastring, checksum_str, sizeof(datastring) - 1);
    datastring[sizeof(datastring) - 1] = '\0';
    txString.set(&datastring[0]);
  }

  void run()
  {
    if (txString.send())
    {
      timestamp = millis();
      activate = true;
    }
    if (activate && millis() - timestamp > WAIT_PERIOD_MS)
    {
      txString.set(&datastring[0]);
      activate = false;
    }
  }

  private:
  static constexpr unsigned long WAIT_PERIOD_MS = 2000;

  uint16_t gps_CRC16_checksum(char const* string)
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

  char datastring[80];
  TxString txString;
  unsigned long timestamp;
  bool activate;
};
#endif

constexpr static uint64_t const TX_PERIOD_US = UINT64_C(20150); // hz/baud
constexpr static uint64_t const T1_PRESCALER = UINT64_C(8);
constexpr static uint64_t const INTERNAL_CLOCK = F_CPU; // hz

static void setupTimer()
{
  noInterrupts();

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

  interrupts();
}

using Length = uint8_t;
using Offset = uint8_t;
using RC = uint8_t;
constexpr static RC const RC_OK = 0x00;
constexpr static RC const RC_FINISHED = 0x01;
constexpr static RC const RC_ERROR = 0x80;
constexpr bool isRcError(RC rc) { return rc & RC_ERROR; }
constexpr bool isRcOk(RC rc) { return !isRcError(rc); }

class PayloadProvider
{
  public:
  virtual ~PayloadProvider() {}
  virtual RC getPayload(byte* buf, Length& length) = 0;
};

class ConstantPayload : public PayloadProvider
{
  public:
  virtual ~ConstantPayload() {}
  virtual RC getPayload(byte* buf, Length& length)
  {
    length = (length < PAYLOAD_LENGTH ? length : PAYLOAD_LENGTH);
    memcpy(buf, PAYLOAD, length);
    return RC_OK;
  }

  private:
  static char const PAYLOAD[];
  static Length const PAYLOAD_LENGTH;
};

char const ConstantPayload::PAYLOAD[] = "Hello, World!";
Length const ConstantPayload::PAYLOAD_LENGTH = sizeof(ConstantPayload::PAYLOAD);

class PayloadLayer
{
  public:
  PayloadLayer(PayloadProvider* provider)
      : provider(provider), payload{}, payloadLength(0), offset(0)
  {}
  RC getByte(byte& val)
  {
    if (offset == payloadLength)
    {
      payloadLength = sizeof(payload);
      auto rc = provider->getPayload(&payload[0], payloadLength);
      if (rc != RC_OK)
      {
        return rc;
      }
      offset = 0;
      if (payloadLength == 0)
      {
        return RC_FINISHED;
      }
    }
    val = payload[offset];
    offset++;
    return RC_OK;
  }

  private:
  constexpr static size_t const MAX_PAYLOAD_SIZE = 64;
  PayloadProvider* provider;
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

#if 0
static TaskSend taskSend;
static Task* const tasks[] = {&taskSend};
#endif
static ConstantPayload payloadProvider;
static PayloadLayer payloadLayer(&payloadProvider);
static SignalLayer signalLayer(&payloadLayer);

void setup()
{
  pinMode(RADIOPIN, OUTPUT);

  setupTimer();
}

void loop()
{
#if 0
  for (auto task : tasks)
  {
    task->run();
  }
#endif
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
