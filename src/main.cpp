#include <Arduino.h>

static constexpr uint8_t const RADIOPIN = 2;

#include <string.h>
#include <util/crc16.h>

static char datastring[80];

static uint16_t gps_CRC16_checksum(char* string);
static void rtty_txstring(char* string);
static void rtty_txbyte(char c);
static void rtty_txbit(int bit);

void setup()
{
  pinMode(RADIOPIN, OUTPUT);
}

void loop()
{

  snprintf(datastring, sizeof(datastring), "RTTY TEST BEACON RTTY TEST BEACON");
  unsigned int CHECKSUM = gps_CRC16_checksum(datastring);
  char checksum_str[6];
  snprintf(checksum_str, sizeof(checksum_str), "*%04X\n", CHECKSUM);
  strncat(datastring, checksum_str, sizeof(datastring) - 1);
  datastring[sizeof(datastring) - 1] = '\0';

  rtty_txstring(datastring);
  delay(2000);
}

static void rtty_txstring(char* string)
{
  char c;

  c = *string++;

  while (c != '\0')
  {
    rtty_txbyte(c);
    c = *string++;
  }
}

static void rtty_txbyte(char c)
{
  /* Simple function to sent each bit of a char to
   ** rtty_txbit function.
   ** NB The bits are sent Least Significant Bit first
   **
   ** All chars should be preceded with a 0 and
   ** proceded with a 1. 0 = Start bit; 1 = Stop bit
   **
   */

  int i;

  rtty_txbit(0); // Start bit

  // Send bits for for char LSB first

  for (i = 0; i < 7; i++) // Change this here 7 or 8 for ASCII-7 / ASCII-8
  {
    if (c & 1)
      rtty_txbit(1);

    else
      rtty_txbit(0);

    c = c >> 1;
  }

  rtty_txbit(1); // Stop bit
  rtty_txbit(1); // Stop bit
}

static void rtty_txbit(int bit)
{
  if (bit)
  {
    // high
    digitalWrite(RADIOPIN, HIGH);
  }
  else
  {
    // low
    digitalWrite(RADIOPIN, LOW);
  }

  // delayMicroseconds(3370); // 300 baud

  // 50 Baud
  // You can't do 20150 it just doesn't work as the
  // largest value that will produce an accurate delay is 16383
  // See : http://arduino.cc/en/Reference/DelayMicroseconds
  delayMicroseconds(10000);
  delayMicroseconds(10150);
}

static uint16_t gps_CRC16_checksum(char* string)
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
