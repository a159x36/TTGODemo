#ifndef ESP32_DIGITAL_LED_LIB_H
#define ESP32_DIGITAL_LED_LIB_H

#include <stdint.h>
#define CONFIG_RMT_SUPPRESS_DEPRECATE_WARN 1
#include <driver/rmt.h>

typedef struct __attribute__ ((packed)) {
  uint8_t g, r, b;
} pixelColor_t;

inline pixelColor_t pixelFromRGB(uint8_t r, uint8_t g, uint8_t b) {
  return (pixelColor_t){g,r,b};
}
typedef struct {
  int bytesPerPixel;
  uint32_t T0H;
  uint32_t T1H;
  uint32_t T0L;
  uint32_t T1L;
  uint32_t TRS;
} ledParams_t;

typedef struct {
  int rmtChannel;
  int gpioNum;
  int ledType;
  int brightLimit;
  int numPixels;

  pixelColor_t * pixels;
  rmt_config_t rmt_config;
} strand_t;

enum led_types {
  LED_WS2812_V1,
  LED_WS2812B_V1,
  LED_WS2812B_V2,
  LED_WS2812B_V3,
  LED_WS2813_V1,
  LED_WS2813_V2,
  LED_WS2813_V3,
  LED_SK6812_V1,
  LED_SK6812W_V1,
};

extern const ledParams_t ledParamsAll[];

extern int digitalLeds_initStrands(strand_t strands [], int numStrands);
extern int digitalLeds_updatePixels(strand_t * strand);
extern void digitalLeds_resetPixels(strand_t * pStrand);
extern void digitalLeds_free(strand_t * pStrand);

#endif /* ESP32_DIGITAL_LED_LIB_H */

