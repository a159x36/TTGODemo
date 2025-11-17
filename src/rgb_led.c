#include "rgb_led.h"

#include <freertos/FreeRTOS.h>
#include "esp_log.h"
#include <stdio.h>
#include <string.h>  

#define RMT_LED_STRIP_RESOLUTION_HZ 20000000

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

static uint8_t max_value=255;

static rmt_symbol_word_t ws2812_zero = {
    .level0 = 1,
    .duration0 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
    .level1 = 0,
    .duration1 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=0.9us
};

static rmt_symbol_word_t ws2812_one = {
    .level0 = 1,
    .duration0 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=0.9us
    .level1 = 0,
    .duration1 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
};

static uint16_t reset_duration=0;

static size_t encoder_callback(const void *src, size_t data_size,
                               size_t symbols_written, size_t symbols_free,
                               rmt_symbol_word_t *symbols, bool *done, void *arg) {
    if (symbols_free < 8) {
        return 0;
    }
    size_t data_pos = symbols_written / 8;
    uint8_t *data_bytes = (uint8_t*)src;
    uint8_t byte=data_bytes[data_pos];
    if(byte>max_value) byte=max_value;
    // Encode a byte
    size_t symbol_pos = 0;
    for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
        if (byte&bitmask) {
            symbols[symbol_pos++] = ws2812_one;
        } else {
            symbols[symbol_pos++] = ws2812_zero;
        }
    }
    // stretch out the last low duration
    if(data_pos==data_size-1) {
      symbols[7].duration1=reset_duration;
      *done = 1;
    }
    // We're done; we should have written 8 symbols.
    return symbol_pos;
}

int digitalLeds_initStrands(strand_t strands [], int numStrands) {
  for (int i = 0; i < numStrands; i++) {
    strand_t * pStrand = &(strands[i]);
    ledParams_t ledParams = ledParamsAll[pStrand->ledType];
    max_value=pStrand->brightLimit;
    float ratio = (float)RMT_LED_STRIP_RESOLUTION_HZ / 1e9;
    ws2812_one.duration0=ledParams.T1H*ratio;
    ws2812_one.duration1=ledParams.T1L*ratio;
    ws2812_zero.duration0=ledParams.T0H*ratio;
    ws2812_zero.duration1=ledParams.T0L*ratio;
    reset_duration=ledParams.TRS*ratio;

    pStrand->pixels = (pixelColor_t*)(malloc(pStrand->numPixels * sizeof(pixelColor_t)));
    if (pStrand->pixels == 0) {
      return -1;
    }
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = pStrand->gpioNum,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &pStrand->rmt_channel));

    const rmt_simple_encoder_config_t simple_encoder_cfg = {
        .callback = encoder_callback,
        //Note we don't set min_chunk_size here as the default of 64 is good enough.
    };
    ESP_ERROR_CHECK(rmt_new_simple_encoder(&simple_encoder_cfg, &pStrand->rmt_encoder));

    ESP_ERROR_CHECK(rmt_enable(pStrand->rmt_channel));
    pStrand->rmt_config = (rmt_transmit_config_t){
        .loop_count = 0, // no transfer loop
    };
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
  ESP_ERROR_CHECK(rmt_transmit(pStrand->rmt_channel, pStrand->rmt_encoder, pStrand->pixels, pStrand->numPixels*3, &pStrand->rmt_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(pStrand->rmt_channel, portMAX_DELAY));
  return 0;
}

void digitalLeds_free(strand_t * pStrand) {
  free(pStrand->pixels);
  rmt_disable(pStrand->rmt_channel);
  rmt_del_encoder(pStrand->rmt_encoder);
  rmt_del_channel(pStrand->rmt_channel);
}