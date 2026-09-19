#ifndef Pins_Arduino_h
#define Pins_Arduino_h
#include "soc/soc_caps.h"
#include <stdint.h>
#define USB_VID 0x303A
#define USB_PID 0x1001
static const uint8_t TX = 43;
static const uint8_t RX = 44;
static const uint8_t SDA = 15;
static const uint8_t SCL = 14;
static const uint8_t SS = 5;
static const uint8_t MOSI = 7;
static const uint8_t MISO = 8;
static const uint8_t SCK = 6;
#define HAS_SCREEN 1
#endif
