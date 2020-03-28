#include <Arduino.h>
#include <string.h>
#include <util/crc16.h>

static constexpr uint8_t const RADIOPIN = 2;

static char datastring[80];

static uint16_t gps_CRC16_checksum(char const* string);

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
  static constexpr unsigned long PERIOD_50_BAUD = 20150; // 50 baud

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
      if (micros() - timestamp >= period - 50)
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

  TxByte() : bitType(START), dataOffset(0), txBit(TxBit::PERIOD_50_BAUD)
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

static TxString txString;

void setup()
{
  pinMode(RADIOPIN, OUTPUT);
  snprintf(datastring, sizeof(datastring), "RTTY TEST BEACON RTTY TEST BEACON");
  unsigned int CHECKSUM = gps_CRC16_checksum(datastring);
  char checksum_str[6];
  snprintf(checksum_str, sizeof(checksum_str), "*%04X\n", CHECKSUM);
  strncat(datastring, checksum_str, sizeof(datastring) - 1);
  datastring[sizeof(datastring) - 1] = '\0';
  txString.set(&datastring[0]);
}

void loop()
{
  static auto timestamp = millis();
  static bool activate = false;
  if (txString.send())
  {
    timestamp = millis();
    activate = true;
  }
  auto curTime = millis();
  if (activate && curTime - timestamp > 2000)
  {
    txString.set(&datastring[0]);
    activate = false;
  }
}
