/*
 * This code is a modified version of this Library:
 * 
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA
 * 
 * 
 * License is MIT
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA/blob/master/LICENSE.txt
 */

#pragma once

#define MATRIX_HEIGHT               64
#define MATRIX_WIDTH                64
#define PIXEL_COLOR_DEPTH_BITS      4
#define MATRIX_ROWS_IN_PARALLEL     2

#define G1_PIN_DEFAULT  13//R1
#define B1_PIN_DEFAULT  21//G1
#define R1_PIN_DEFAULT  12//B1
#define G2_PIN_DEFAULT  27//R2
#define B2_PIN_DEFAULT  17//G2
#define R2_PIN_DEFAULT  4//B2

#define A_PIN_DEFAULT   19
#define B_PIN_DEFAULT   23
#define C_PIN_DEFAULT   18
#define D_PIN_DEFAULT   5
#define E_PIN_DEFAULT   15

#define LAT_PIN_DEFAULT 22
#define OE_PIN_DEFAULT  2
#define CLK_PIN_DEFAULT 14

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "i2s_matrix.h"
#include "Adafruit_GFX.h"

#define BIT_R1  (1<<0)
#define BIT_G1  (1<<1)
#define BIT_B1  (1<<2)
#define BIT_R2  (1<<3)
#define BIT_G2  (1<<4)
#define BIT_B2  (1<<5)
#define BIT_LAT (1<<6)
#define BIT_OE  (1<<7)
#define BIT_A (1<<8)
#define BIT_B (1<<9)
#define BIT_C (1<<10)
#define BIT_D (1<<11)
#define BIT_E (1<<12)

#define COLOR_CHANNELS_PER_PIXEL 3
#define PIXELS_PER_ROW ((MATRIX_WIDTH * MATRIX_HEIGHT) / MATRIX_HEIGHT)
#define ROWS_PER_FRAME (MATRIX_HEIGHT/MATRIX_ROWS_IN_PARALLEL)

#define ESP32_I2S_DMA_MODE          I2S_PARALLEL_BITS_16
#define ESP32_I2S_DMA_STORAGE_TYPE  uint16_t
#define ESP32_I2S_CLOCK_SPEED       (40000000UL)
#define CLKS_DURING_LATCH            0

struct rowBitStruct {
  ESP32_I2S_DMA_STORAGE_TYPE data[PIXELS_PER_ROW + CLKS_DURING_LATCH];
};

struct rowColorDepthStruct {
  rowBitStruct rowbits[PIXEL_COLOR_DEPTH_BITS];
};

struct frameStruct {
  rowColorDepthStruct rowdata[ROWS_PER_FRAME];
};

typedef struct rgb_24 {
  rgb_24() : rgb_24(0, 0, 0) {}
  rgb_24(uint8_t r, uint8_t g, uint8_t b) {
    red = r; green = g; blue = b;
  }
  rgb_24& operator=(const rgb_24& col);
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} rgb_24;

class RGB64x32MatrixPanel_I2S_DMA : public Adafruit_GFX {
  public:
    RGB64x32MatrixPanel_I2S_DMA(bool _double_buffer = false)
      : Adafruit_GFX(MATRIX_WIDTH, MATRIX_HEIGHT), double_buffering_enabled(_double_buffer)  {}
    bool begin(int dma_r1_pin = R1_PIN_DEFAULT , int dma_g1_pin = G1_PIN_DEFAULT, int dma_b1_pin = B1_PIN_DEFAULT , int dma_r2_pin = R2_PIN_DEFAULT , int dma_g2_pin = G2_PIN_DEFAULT , int dma_b2_pin = B2_PIN_DEFAULT , int dma_a_pin  = A_PIN_DEFAULT  , int dma_b_pin = B_PIN_DEFAULT  , int dma_c_pin = C_PIN_DEFAULT , int dma_d_pin = D_PIN_DEFAULT  , int dma_e_pin = E_PIN_DEFAULT , int dma_lat_pin = LAT_PIN_DEFAULT, int dma_oe_pin = OE_PIN_DEFAULT , int dma_clk_pin = CLK_PIN_DEFAULT)
    {
      if ( !allocateDMAmemory() ) return false;
      clearScreen(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage
      flipDMABuffer(); // flip to backbuffer 1
      clearScreen(); // Must fill the DMA buffer with the initial output bit sequence or the panel will display garbage
      flipDMABuffer(); // backbuffer 0
      configureDMA(dma_r1_pin, dma_g1_pin, dma_b1_pin, dma_r2_pin, dma_g2_pin, dma_b2_pin, dma_a_pin,  dma_b_pin, dma_c_pin, dma_d_pin, dma_e_pin, dma_lat_pin,  dma_oe_pin,   dma_clk_pin ); //DMA and I2S configuration and setup
      showDMABuffer();
      return everything_OK;
    }
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color);
    void clearScreen() {
      fillScreen(0);
    }
    void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB24(int16_t x, int16_t y, rgb_24 color);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows);

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) {
      return color565(r * 17, g * 17, b * 17);
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
    uint16_t Color333(uint8_t r, uint8_t g, uint8_t b);
    inline void flipDMABuffer()
    {
      if ( !double_buffering_enabled) return;
      back_buffer_id ^= 1;
      while (!i2s_parallel_is_previous_buffer_free()) {}
    }

    inline void showDMABuffer()
    {
      if (!double_buffering_enabled) return;
      i2s_parallel_flip_to_buffer(&I2S1, back_buffer_id);
      while (!i2s_parallel_is_previous_buffer_free()) {}
    }

    inline void setPanelBrightness(int b)
    {
      brightness = b;
    }

    inline void setMinRefreshRate(int rr)
    {
      min_refresh_rate = rr;
    }

    int  calculated_refresh_rate  = 0;

  private:
    frameStruct *matrix_framebuffer_malloc_1;
    int desccount = 0;
    lldesc_t * dmadesc_a = {0};
    lldesc_t * dmadesc_b = {0};
    bool everything_OK              = false;
    bool double_buffering_enabled   = false;
    int  back_buffer_id             = 0;
    int  brightness           = 32;
    int  min_refresh_rate     = 120;
    int  lsbMsbTransitionBit  = 0;
    bool allocateDMAmemory();
    void configureDMA(int r1_pin, int  g1_pin, int  b1_pin, int  r2_pin, int  g2_pin, int  b2_pin, int  a_pin, int   b_pin, int  c_pin, int  d_pin, int  e_pin, int  lat_pin, int   oe_pin, int clk_pin);
    void updateMatrixDMABuffer(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
    void updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue);
};

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) // adafruit virtual void override
{
  drawPixelRGB565( x, y, color);
}

inline void RGB64x32MatrixPanel_I2S_DMA::fillScreen(uint16_t color)  // adafruit virtual void override
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  updateMatrixDMABuffer(r, g, b);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB565(int16_t x, int16_t y, uint16_t color)
{
  uint8_t r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color & 0x1F) * 527) + 23) >> 6;
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  updateMatrixDMABuffer( x, y, r, g, b);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawPixelRGB24(int16_t x, int16_t y, rgb_24 color)
{
  updateMatrixDMABuffer( x, y, color.red, color.green, color.blue);
}

inline uint16_t RGB64x32MatrixPanel_I2S_DMA::color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline uint16_t RGB64x32MatrixPanel_I2S_DMA::Color333(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0x7) << 13) | ((r & 0x6) << 10) | ((g & 0x7) << 8) | ((g & 0x7) << 5) | ((b & 0x7) << 2) | ((b & 0x6) >> 1);
}

inline void RGB64x32MatrixPanel_I2S_DMA::drawIcon (int *ico, int16_t x, int16_t y, int16_t cols, int16_t rows) {
  int i, j;
  for (i = 0; i < rows; i++) {
    for (j = 0; j < cols; j++) {
      drawPixelRGB565 (x + j, y + i, ico[i * cols + j]);
    }
  }
}
