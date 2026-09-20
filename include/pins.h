#ifndef PINS_H
#define PINS_H

// CYD - ST7789 320x240 (FIX01; Pinbelegung aus FIX00 beibehalten)
#define TFT_MOSI       13
#define TFT_MISO       12
#define TFT_SCK        14
#define TFT_CS         15
#define TFT_DC          2
#define TFT_RST        -1
#define TFT_BACKLIGHT  21      // CYD: HIGH-aktiv

#define SCREEN_W       320
#define SCREEN_H       240

// XPT2046 Touch - eigener SPI-Bus (funktionierendes CYD-Projekt)
#define TOUCH_SCK      25
#define TOUCH_MOSI     32
#define TOUCH_MISO     39
#define TOUCH_CS       33
#define TOUCH_IRQ      36

// Onboard CdS/LDR des CYD
#define LDR_PIN        34

#endif
