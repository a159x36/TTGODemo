

#include "rgb_led.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stdio.h>
#include <string.h>  


const ledParams_t ledParamsAll[] = {  // Still must match order of `led_types`
  [LED_WS2812_V1]  = { .bytesPerPixel = 3, .T0H = 350, .T1H = 700, .T0L = 800, .T1L = 600, .TRS =  50000},
  [LED_WS2812B_V1] = { .bytesPerPixel = 3, .T0H = 350, .T1H = 900, .T0L = 900, .T1L = 350, .TRS =  50000}, // Older datasheet
  [LED_WS2812B_V2] = { .bytesPerPixel = 3, .T0H = 400, .T1H = 850, .T0L = 850, .T1L = 400, .TRS =  50000}, // 2016 datasheet
  [LED_WS2812B_V3] = { .bytesPerPixel = 3, .T0H = 450, .T1H = 850, .T0L = 850, .T1L = 450, .TRS =  50000}, // cplcpu test
  [LED_WS2813_V1]  = { .bytesPerPixel = 3, .T0H = 350, .T1H = 800, .T0L = 350, .T1L = 350, .TRS = 300000}, // Older datasheet
  [LED_WS2813_V2]  = { .bytesPerPixel = 3, .T0H = 270, .T1H = 800, .T0L = 800, .T1L = 270, .TRS = 300000}, // 2016 datasheet
  [LED_WS2813_V3]  = { .bytesPerPixel = 3, .T0H = 270, .T1H = 630, .T0L = 630, .T1L = 270, .TRS = 300000}, // 2017-05 WS datasheet
  [LED_SK6812_V1]  = { .bytesPerPixel = 3, .T0H = 300, .T1H = 600, .T0L = 900, .T1L = 600, .TRS =  80000},
  [LED_SK6812W_V1] = { .bytesPerPixel = 4, .T0H = 300, .T1H = 600, .T0L = 900, .T1L = 600, .TRS =  80000},
};

static uint32_t t0h_ticks = 0;
static uint32_t t1h_ticks = 0;
static uint32_t t0l_ticks = 0;
static uint32_t t1l_ticks = 0;
static uint32_t reset_ticks = 0;
static uint8_t max_value=255;



static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num) {
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    rmt_item32_t bit0 = {{{ t0h_ticks, 1, t0l_ticks, 0 }}}; //Logical 0
    rmt_item32_t bit1 = {{{ t1h_ticks, 1, t1l_ticks, 0 }}}; //Logical 1

    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num) {
        uint8_t byte=*psrc;
        if(byte>max_value) byte=max_value;
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (byte & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        if(size==src_size-1) { // stretch out 1 for last pulse
          (pdest-1)->duration1=reset_ticks;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

int digitalLeds_initStrands(strand_t strands [], int numStrands) {
  for (int i = 0; i < numStrands; i++) {
    strand_t * pStrand = &(strands[i]);
    ledParams_t ledParams = ledParamsAll[pStrand->ledType];

    pStrand->pixels = (pixelColor_t*)(malloc(pStrand->numPixels * sizeof(pixelColor_t)));
    if (pStrand->pixels == 0) {
      return -1;
    }
    pStrand->rmt_config = (rmt_config_t)RMT_DEFAULT_CONFIG_TX(pStrand->gpioNum, pStrand->rmtChannel);
    pStrand->rmt_config.clk_div = 4;
    ESP_ERROR_CHECK(rmt_config(&pStrand->rmt_config));
    rmt_driver_install(pStrand->rmt_config.channel, 0, 0);
    uint32_t counter_clk_hz = 0;
    rmt_get_counter_clock(pStrand->rmt_config.channel, &counter_clk_hz);
    float ratio = (float)counter_clk_hz / 1e9;
    
    t0h_ticks = ledParams.T0H * ratio;
    t0l_ticks = ledParams.T0L * ratio;
    t1h_ticks = ledParams.T1H * ratio;
    t1l_ticks = ledParams.T1L * ratio;
    reset_ticks = ledParams.TRS * ratio;
    max_value = pStrand->brightLimit;
    rmt_translator_init(pStrand->rmt_config.channel, ws2812_rmt_adapter);
  }
  for (int i = 0; i < numStrands; i++) {
    strand_t * pStrand = &strands[i];
    digitalLeds_resetPixels(pStrand);
  }

  return 0; 
}

void digitalLeds_resetPixels(strand_t * pStrand) {
  memset(pStrand->pixels, 0, pStrand->numPixels * sizeof(pixelColor_t));
  digitalLeds_updatePixels(pStrand);
}

int IRAM_ATTR digitalLeds_updatePixels(strand_t * pStrand) {
  esp_err_t err=rmt_write_sample(pStrand->rmt_config.channel, (uint8_t *)(pStrand->pixels), pStrand->numPixels * 3, true);
  return err;
}

void digitalLeds_free(strand_t * pStrand) {
  free(pStrand->pixels);
}