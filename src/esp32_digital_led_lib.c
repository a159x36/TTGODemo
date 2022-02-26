/* 
 * Library for driving digital RGB(W) LEDs using the ESP32's RMT peripheral
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com
 *
 */
/* 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "esp32_digital_led_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARDUINO)
  #include "esp32-hal.h"
  #include "esp_intr.h"
  #include "driver/gpio.h"
  #include "driver/rmt.h"
  #include "driver/periph_ctrl.h"
  #include "freertos/semphr.h"
  #include "soc/rmt_struct.h"
#elif defined(ESP_PLATFORM)
  #include <esp_intr_alloc.h>
  #include <driver/gpio.h>
  #include <driver/rmt.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
  #include <soc/dport_reg.h>
  #include <soc/gpio_sig_map.h>
  #include <soc/rmt_struct.h>
  #include <stdio.h>
  #include <string.h>  // memset, memcpy, etc. live here!
#endif

#ifdef __cplusplus
}
#endif

#if DEBUG_ESP32_DIGITAL_LED_LIB
extern char * digitalLeds_debugBuffer;
extern int digitalLeds_debugBufferSz;
#endif

static DRAM_ATTR const uint16_t MAX_PULSES = 32;  // A channel has a 64 "pulse" buffer - we use half per pass
static DRAM_ATTR const uint16_t DIVIDER    =  4;  // 8 still seems to work, but timings become marginal
static DRAM_ATTR const uint16_t RMT_DURATION_NS = 50;  // Minimum time of a single RMT duration based on clock ns * DIVIDER


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

// LUT for mapping bits in RMT.int_<op>.ch<n>_tx_thr_event
static DRAM_ATTR const uint32_t tx_thr_event_offsets [] = {
  (uint32_t)(1 << 24),
  (uint32_t)(1 << 25),
  (uint32_t)(1 << 26),
  (uint32_t)(1 << 27),
  (uint32_t)(1 << 28),
  (uint32_t)(1 << 29),
  (uint32_t)(1 << 30),
  (uint32_t)(1 << 31)
};

// LUT for mapping bits in RMT.int_<op>.ch<n>_tx_end
static DRAM_ATTR const uint32_t tx_end_offsets [] = {
  (uint32_t)(1 << 0),
  (uint32_t)(1 << 3),
  (uint32_t)(1 << 6),
  (uint32_t)(1 << 9),
  (uint32_t)(1 << 12),
  (uint32_t)(1 << 15),
  (uint32_t)(1 << 18),
  (uint32_t)(1 << 21)
};

typedef union {
  struct {
    uint32_t duration0:15;
    uint32_t level0:1;
    uint32_t duration1:15;
    uint32_t level1:1;
  };
  uint32_t val;
} rmtPulsePair;

typedef struct {
  uint8_t * buf_data;
  uint16_t buf_pos, buf_len, buf_half, buf_isDirty;
  //xSemaphoreHandle sem;
  rmtPulsePair pulsePairMap[2];
} digitalLeds_stateData;

static xSemaphoreHandle gRmtSem = 0;
static strand_t * localStrands;
static int localStrandCnt = 0;

static intr_handle_t rmt_intr_handle = 0;

// Forward declarations of local functions
static void copyToRmtBlock_half(strand_t * pStrand);
static void handleInterrupt(void *arg);


int digitalLeds_initStrands(strand_t strands [], int numStrands)
{
  #if DEBUG_ESP32_DIGITAL_LED_LIB
    snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
             "%sdigitalLeds_init numStrands = %d\n", digitalLeds_debugBuffer, numStrands);
  #endif

  localStrands = strands;
  localStrandCnt = numStrands;
  if (localStrandCnt < 1 || localStrandCnt > 8) {
    return -1;
  }
  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

  RMT.apb_conf.fifo_mask = 1;  // Enable memory access, instead of FIFO mode
  RMT.apb_conf.mem_tx_wrap_en = 1;  // Wrap around when hitting end of buffer

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    ledParams_t ledParams = ledParamsAll[pStrand->ledType];

    pStrand->pixels = (pixelColor_t*)(malloc(pStrand->numPixels * sizeof(pixelColor_t)));
    if (pStrand->pixels == 0) {
      return -1;
    }

    pStrand->_stateVars = (digitalLeds_stateData*)(malloc(sizeof(digitalLeds_stateData)));
    if (pStrand->_stateVars == 0) {
      return -1;
    }
    digitalLeds_stateData * pState = (digitalLeds_stateData*)(pStrand->_stateVars);

    pState->buf_len = (pStrand->numPixels * ledParams.bytesPerPixel);
    pState->buf_data = (uint8_t*)(malloc(pState->buf_len));
    if (pState->buf_data == 0) {
      return -1;
    }

    rmt_set_pin(
      (rmt_channel_t)(pStrand->rmtChannel),
      RMT_MODE_TX,
      (gpio_num_t)(pStrand->gpioNum));
  
    RMT.conf_ch[pStrand->rmtChannel].conf0.div_cnt = DIVIDER;
    RMT.conf_ch[pStrand->rmtChannel].conf0.mem_size = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf0.carrier_en = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf0.carrier_out_lv = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf0.mem_pd = 0;
  
    RMT.conf_ch[pStrand->rmtChannel].conf1.rx_en = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf1.mem_owner = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf1.tx_conti_mode = 0;  //loop back mode
    RMT.conf_ch[pStrand->rmtChannel].conf1.ref_always_on = 1;  // use apb clock: 80M
    RMT.conf_ch[pStrand->rmtChannel].conf1.idle_out_en = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf1.idle_out_lv = 0;
  
    RMT.tx_lim_ch[pStrand->rmtChannel].limit = MAX_PULSES;
  
    // RMT config for transmitting a '0' bit val to this LED strand
    pState->pulsePairMap[0].level0 = 1;
    pState->pulsePairMap[0].level1 = 0;
    pState->pulsePairMap[0].duration0 = ledParams.T0H / (RMT_DURATION_NS );
    pState->pulsePairMap[0].duration1 = ledParams.T0L / (RMT_DURATION_NS );
    
    // RMT config for transmitting a '0' bit val to this LED strand
    pState->pulsePairMap[1].level0 = 1;
    pState->pulsePairMap[1].level1 = 0;
    pState->pulsePairMap[1].duration0 = ledParams.T1H / (RMT_DURATION_NS );
    pState->pulsePairMap[1].duration1 = ledParams.T1L / (RMT_DURATION_NS );

    RMT.int_ena.val |= tx_thr_event_offsets[pStrand->rmtChannel];  // RMT.int_ena.ch<n>_tx_thr_event = 1;
    RMT.int_ena.val |= tx_end_offsets[pStrand->rmtChannel];  // RMT.int_ena.ch<n>_tx_end = 1;
  }
  if(!gRmtSem)
    gRmtSem = xSemaphoreCreateBinary();
  xSemaphoreGive(gRmtSem);
  esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, handleInterrupt, 0, &rmt_intr_handle);

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    digitalLeds_resetPixels(pStrand);
  }

  return 0;
}

void digitalLeds_resetPixels(strand_t * pStrand)
{
  memset(pStrand->pixels, 0, pStrand->numPixels * sizeof(pixelColor_t));
  digitalLeds_updatePixels(pStrand);
}

int IRAM_ATTR digitalLeds_updatePixels(strand_t * pStrand)
{
  digitalLeds_stateData * pState = (digitalLeds_stateData*)(pStrand->_stateVars);
  ledParams_t ledParams = ledParamsAll[pStrand->ledType];

  xSemaphoreTake(gRmtSem, portMAX_DELAY);

  // Pack pixels into transmission buffer
  if (ledParams.bytesPerPixel == 3) {
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      uint8_t r,g,b;
      r=pStrand->pixels[i].r;
      g=pStrand->pixels[i].g;
      b=pStrand->pixels[i].b;
      if(r>pStrand->brightLimit) r=pStrand->brightLimit;
      if(g>pStrand->brightLimit) g=pStrand->brightLimit;
      if(b>pStrand->brightLimit) b=pStrand->brightLimit;
      // Color order is translated from RGB to GRB
      pState->buf_data[0 + i * 3] = g;
      pState->buf_data[1 + i * 3] = r;
      pState->buf_data[2 + i * 3] = b;
    }
  }
  else if (ledParams.bytesPerPixel == 4) {
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      // Color order is translated from RGBW to GRBW
      pState->buf_data[0 + i * 4] = pStrand->pixels[i].g;
      pState->buf_data[1 + i * 4] = pStrand->pixels[i].r;
      pState->buf_data[2 + i * 4] = pStrand->pixels[i].b;
      pState->buf_data[3 + i * 4] = pStrand->pixels[i].w;
    }    
  }
  else {
    return -1;
  }

  pState->buf_pos = 0;
  pState->buf_half = 0;

  copyToRmtBlock_half(pStrand);

  if (pState->buf_pos < pState->buf_len) {
    // Fill the other half of the buffer block
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
               "%s# ", digitalLeds_debugBuffer);
    #endif
    copyToRmtBlock_half(pStrand);
  }

  

  RMT.conf_ch[pStrand->rmtChannel].conf1.mem_rd_rst = 1;
  RMT.conf_ch[pStrand->rmtChannel].conf1.tx_start = 1;

  xSemaphoreTake(gRmtSem, portMAX_DELAY);
  xSemaphoreGive(gRmtSem);

  return 0;
}

static IRAM_ATTR void copyToRmtBlock_half(strand_t * pStrand)
{
  // This fills half an RMT block
  // When wraparound is happening, we want to keep the inactive half of the RMT block filled

  digitalLeds_stateData * pState = (digitalLeds_stateData*)(pStrand->_stateVars);
  ledParams_t ledParams = ledParamsAll[pStrand->ledType];

  uint16_t i, j, offset, len, byteval;

  offset = pState->buf_half * MAX_PULSES;
  pState->buf_half = !pState->buf_half;

  len = pState->buf_len - pState->buf_pos;
  if (len > (MAX_PULSES / 8))
    len = (MAX_PULSES / 8);

  if (!len) {
    if (!pState->buf_isDirty) {
      return;
    }
    // Clear the channel's data block and return
    for (i = 0; i < MAX_PULSES; i++) {
      RMTMEM.chan[pStrand->rmtChannel].data32[i + offset].val = 0;
    }
    pState->buf_isDirty = 0;
    return;
  }
  pState->buf_isDirty = 1;

  for (i = 0; i < len; i++) {
    byteval = pState->buf_data[i + pState->buf_pos];

    #if DEBUG_ESP32_DIGITAL_LED_LIB
      snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
               "%s%d(", digitalLeds_debugBuffer, byteval);
    #endif

    // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to
    // the rmtPulsePair value corresponding to the buffered bit value
    for (j = 0; j < 8; j++, byteval <<= 1) {
      int bitval = (byteval >> 7) & 0x01;
      int data32_idx = i * 8 + offset + j;
      RMTMEM.chan[pStrand->rmtChannel].data32[data32_idx].val = pState->pulsePairMap[bitval].val;
      #if DEBUG_ESP32_DIGITAL_LED_LIB
        snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
                 "%s%d", digitalLeds_debugBuffer, bitval);
      #endif
    }
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
               "%s) ", digitalLeds_debugBuffer);
    #endif

    // Handle the reset bit by stretching duration1 for the final bit in the stream
    if (i + pState->buf_pos == pState->buf_len - 1) {
      RMTMEM.chan[pStrand->rmtChannel].data32[i * 8 + offset + 7].duration1 =
        ledParams.TRS / (RMT_DURATION_NS );
      #if DEBUG_ESP32_DIGITAL_LED_LIB
        snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
                 "%sRESET ", digitalLeds_debugBuffer);
      #endif
    }
  }

  // Clear the remainder of the channel's data not set above
  for (i *= 8; i < MAX_PULSES; i++) {
    RMTMEM.chan[pStrand->rmtChannel].data32[i + offset].val = 0;
  }
  
  pState->buf_pos += len;

  #if DEBUG_ESP32_DIGITAL_LED_LIB
    snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
             "%s ", digitalLeds_debugBuffer);
  #endif

  return;
}

static IRAM_ATTR void handleInterrupt(void *arg)
{
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

  #if DEBUG_ESP32_DIGITAL_LED_LIB
    snprintf(digitalLeds_debugBuffer, digitalLeds_debugBufferSz,
             "%sRMT.int_st.val = %08x\n", digitalLeds_debugBuffer, RMT.int_st.val);
  #endif

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    //digitalLeds_stateData * pState = (digitalLeds_stateData*)(pStrand->_stateVars);

    if (RMT.int_st.val & tx_thr_event_offsets[pStrand->rmtChannel])
    {  // tests RMT.int_st.ch<n>_tx_thr_event
      copyToRmtBlock_half(pStrand);
      RMT.int_clr.val |= tx_thr_event_offsets[pStrand->rmtChannel];  // set RMT.int_clr.ch<n>_tx_thr_event
    }
    else if (RMT.int_st.val & tx_end_offsets[pStrand->rmtChannel] && gRmtSem)
    {  // tests RMT.int_st.ch<n>_tx_end and semaphore
      xSemaphoreGiveFromISR(gRmtSem, &xHigherPriorityTaskWoken);
      RMT.int_clr.val |= tx_end_offsets[pStrand->rmtChannel];  // set RMT.int_clr.ch<n>_tx_end 
      if (xHigherPriorityTaskWoken == pdTRUE)
      {
          portYIELD_FROM_ISR();
      }
    }
  }

  return;
}

