#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x239A
#define USB_PID 0x8149
// Route main I2C bus to MEMENTO's AW9523 expander pins
#define GROVE_SDA 34
#define GROVE_SCL 33

// Enable 5-way navigation (Up, Down, Left, Right, OK)
#define HAS_5_BUTTONS
#define BTN_ALIAS "\"OK\""

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 34;
static const uint8_t SCL = 33;

static const uint8_t SS   = 48;
static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SCK  = 36;

static const uint8_t A0  = 17;
static const uint8_t A1  = 18;
static const uint8_t A2  = 3;
static const uint8_t A3  = 4;
static const uint8_t A4  = 5;
static const uint8_t A5  = 6;
static const uint8_t A6  = 7;
static const uint8_t A7  = 8;
static const uint8_t A8  = 9;
static const uint8_t A9  = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;

static const uint8_t T1  = 1;
static const uint8_t T2  = 2;
static const uint8_t T3  = 3;
static const uint8_t T4  = 4;
static const uint8_t T5  = 5;
static const uint8_t T6  = 6;
static const uint8_t T7  = 7;
static const uint8_t T8  = 8;
static const uint8_t T9  = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
