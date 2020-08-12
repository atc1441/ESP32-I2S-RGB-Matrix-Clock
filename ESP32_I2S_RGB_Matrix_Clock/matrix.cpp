/*
 * This code is a modified version of this Library:
 * 
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA
 * 
 * 
 * License is MIT
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA/blob/master/LICENSE.txt
 */

#include "matrix.h"

bool RGB64x32MatrixPanel_I2S_DMA::allocateDMAmemory()
{
  int    _num_frame_buffers                   = (double_buffering_enabled) ? 2 : 1;
  size_t _frame_buffer_memory_required        = sizeof(frameStruct) * _num_frame_buffers;
  size_t _dma_linked_list_memory_required     = 0;
  size_t _total_dma_capable_memory_reserved   = 0;
  if ( heap_caps_get_largest_free_block(MALLOC_CAP_DMA) < _frame_buffer_memory_required  ) return false;
  matrix_framebuffer_malloc_1 = (frameStruct *)heap_caps_malloc(_frame_buffer_memory_required, MALLOC_CAP_DMA);
  if ( matrix_framebuffer_malloc_1 == NULL ) return false;
  _total_dma_capable_memory_reserved += _frame_buffer_memory_required;
  int numDMAdescriptorsPerRow = 0;
  lsbMsbTransitionBit = 0;
  while (1) {
    numDMAdescriptorsPerRow = 1;
    for (int i = lsbMsbTransitionBit + 1; i < PIXEL_COLOR_DEPTH_BITS; i++) {
      numDMAdescriptorsPerRow += 1 << (i - lsbMsbTransitionBit - 1);
    }
    int ramrequired = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
    int largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    if (ramrequired < largestblockfree)
      break;

    if (lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
      lsbMsbTransitionBit++;
    else
      break;
  }
  while (1) {
    int psPerClock = 1000000000000UL / ESP32_I2S_CLOCK_SPEED;
    int nsPerLatch = ((PIXELS_PER_ROW + CLKS_DURING_LATCH) * psPerClock) / 1000;
    int nsPerRow = PIXEL_COLOR_DEPTH_BITS * nsPerLatch;
    for (int i = lsbMsbTransitionBit + 1; i < PIXEL_COLOR_DEPTH_BITS; i++)
      nsPerRow += (1 << (i - lsbMsbTransitionBit - 1)) * (PIXEL_COLOR_DEPTH_BITS - i) * nsPerLatch;
    int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
    int actualRefreshRate = 1000000000UL / (nsPerFrame);
    calculated_refresh_rate = actualRefreshRate;
    if (actualRefreshRate > min_refresh_rate) break;
    if (lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
      lsbMsbTransitionBit++;
    else
      break;
  }
  numDMAdescriptorsPerRow = 1;
  for (int i = lsbMsbTransitionBit + 1; i < PIXEL_COLOR_DEPTH_BITS; i++) {
    numDMAdescriptorsPerRow += 1 << (i - lsbMsbTransitionBit - 1);
  }
  if ( sizeof(rowColorDepthStruct) > DMA_MAX ) numDMAdescriptorsPerRow += PIXEL_COLOR_DEPTH_BITS - 1;
  _dma_linked_list_memory_required = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
  _total_dma_capable_memory_reserved += _dma_linked_list_memory_required;
  if (_dma_linked_list_memory_required > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)) return false;
  desccount = numDMAdescriptorsPerRow * ROWS_PER_FRAME;
  dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
  assert("Can't allocate descriptor framebuffer a");
  if (!dmadesc_a) return false;
  if (double_buffering_enabled) // reserve space for second framebuffer linked list
  {
    dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Could not malloc descriptor framebuffer b.");
    if (!dmadesc_b) return false;
  }
  everything_OK = true;
  return true;
}

void RGB64x32MatrixPanel_I2S_DMA::configureDMA(int r1_pin, int  g1_pin, int  b1_pin, int  r2_pin, int  g2_pin, int  b2_pin, int  a_pin, int   b_pin, int  c_pin, int  d_pin, int  e_pin, int  lat_pin, int   oe_pin, int clk_pin)
{
  lldesc_t *previous_dmadesc_a     = 0;
  lldesc_t *previous_dmadesc_b     = 0;
  int current_dmadescriptor_offset = 0;
  int num_dma_payload_color_depths = PIXEL_COLOR_DEPTH_BITS;
  if ( sizeof(rowColorDepthStruct) > DMA_MAX ) {
    num_dma_payload_color_depths = 1;
  }
  for (int j = 0; j < ROWS_PER_FRAME; j++) {
    frameStruct *fb_malloc_ptr = matrix_framebuffer_malloc_1;
    int fb_malloc_j = j;
    link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowdata[fb_malloc_j].rowbits[0].data), sizeof(rowBitStruct) * num_dma_payload_color_depths);
    previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];
    if (double_buffering_enabled) {
      link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowdata[fb_malloc_j].rowbits[0].data), sizeof(rowBitStruct) * num_dma_payload_color_depths);
      previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset];
    }
    current_dmadescriptor_offset++;
    if ( sizeof(rowColorDepthStruct) > DMA_MAX )
    {
      for (int cd = 1; cd < PIXEL_COLOR_DEPTH_BITS; cd++)
      {
        link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowdata[fb_malloc_j].rowbits[cd].data), sizeof(rowBitStruct) );
        previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];
        if (double_buffering_enabled) {
          link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowdata[fb_malloc_j].rowbits[cd].data), sizeof(rowBitStruct) );
          previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset];
        }
        current_dmadescriptor_offset++;
      }
    }
    for (int i = lsbMsbTransitionBit + 1; i < PIXEL_COLOR_DEPTH_BITS; i++)
    {
      for (int k = 0; k < 1 << (i - lsbMsbTransitionBit - 1); k++)
      {
        link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowdata[fb_malloc_j].rowbits[i].data), sizeof(rowBitStruct) * (PIXEL_COLOR_DEPTH_BITS - i));
        previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

        if (double_buffering_enabled) {
          link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowdata[fb_malloc_j].rowbits[i].data), sizeof(rowBitStruct) * (PIXEL_COLOR_DEPTH_BITS - i));
          previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset];
        }
        current_dmadescriptor_offset++;
      }
    }
  }
  dmadesc_a[desccount - 1].eof = 1;
  dmadesc_a[desccount - 1].qe.stqe_next = (lldesc_t*)&dmadesc_a[0];

  if (double_buffering_enabled) {
    dmadesc_b[desccount - 1].eof = 1;
    dmadesc_b[desccount - 1].qe.stqe_next = (lldesc_t*)&dmadesc_b[0];
  } else {
    dmadesc_b = dmadesc_a; // link to same 'a' buffer
  }

  i2s_parallel_config_t cfg = {
    .gpio_bus = {r1_pin, g1_pin, b1_pin, r2_pin, g2_pin, b2_pin, lat_pin, oe_pin, a_pin, b_pin, c_pin, d_pin, e_pin, -1, -1, -1},
    .gpio_clk = clk_pin,
    .clkspeed_hz = ESP32_I2S_CLOCK_SPEED, //ESP32_I2S_CLOCK_SPEED,  // formula used is 80000000L/(cfg->clkspeed_hz + 1), must result in >=2.  Acceptable values 26.67MHz, 20MHz, 16MHz, 13.34MHz...
    .bits = ESP32_I2S_DMA_MODE, //ESP32_I2S_DMA_MODE,
    .bufa = 0,
    .bufb = 0,
    desccount,
    desccount,
    dmadesc_a,
    dmadesc_b
  };
  i2s_parallel_setup_without_malloc(&I2S1, &cfg);
}

void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(int16_t x_coord, int16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
  if ( !everything_OK ) return;
  if ( x_coord < 0 || y_coord < 0 || x_coord >= MATRIX_WIDTH || y_coord >= MATRIX_HEIGHT) return;
  bool paint_top_half = true;
  if ( y_coord >= ROWS_PER_FRAME)
  {
    y_coord -= ROWS_PER_FRAME;
    paint_top_half = false;
  }
  for (int color_depth_idx = 0; color_depth_idx < PIXEL_COLOR_DEPTH_BITS; color_depth_idx++)
  {
    uint16_t mask = (1 << color_depth_idx);
    rowBitStruct *p = &matrix_framebuffer_malloc_1[back_buffer_id].rowdata[y_coord].rowbits[color_depth_idx];
    int v = 0;
    int gpioRowAddress = y_coord;
    if (color_depth_idx == 0) gpioRowAddress = y_coord - 1;
    if (gpioRowAddress & 0x01) v |= BIT_A; // 1
    if (gpioRowAddress & 0x02) v |= BIT_B; // 2
    if (gpioRowAddress & 0x04) v |= BIT_C; // 4
    if (gpioRowAddress & 0x08) v |= BIT_D; // 8
    if (gpioRowAddress & 0x10) v |= BIT_E; // 16
    if ((x_coord) == 0 ) v |= BIT_OE;
    if ((x_coord) == PIXELS_PER_ROW - 1) v |= BIT_LAT;
    if ((x_coord) == PIXELS_PER_ROW - 2) v |= BIT_OE;
    if ((color_depth_idx > lsbMsbTransitionBit || !color_depth_idx) && ((x_coord) >= brightness)) v |= BIT_OE;
    if (color_depth_idx && color_depth_idx <= lsbMsbTransitionBit) {
      int lsbBrightness = brightness >> (lsbMsbTransitionBit - color_depth_idx + 1);
      if ((x_coord) >= lsbBrightness) v |= BIT_OE;
    }
    int tmp_x_coord = x_coord;
    if (x_coord % 2)
    {
      tmp_x_coord -= 1;
    } else {
      tmp_x_coord += 1;
    }
    if (paint_top_half)
    {
      if (green & mask) v |= BIT_G1;
      if (blue & mask) v |= BIT_B1;
      if (red & mask) v |= BIT_R1;
      if (p->data[tmp_x_coord] & BIT_R2) v |= BIT_R2;
      if (p->data[tmp_x_coord] & BIT_G2) v |= BIT_G2;
      if (p->data[tmp_x_coord] & BIT_B2) v |= BIT_B2;
    }
    else
    {
      if (red & mask)   {
        v |= BIT_R2;
      }
      if (green & mask) {
        v |= BIT_G2;
      }
      if (blue & mask)  {
        v |= BIT_B2;
      }
      if (p->data[tmp_x_coord] & BIT_R1)
        v |= BIT_R1;
      if (p->data[tmp_x_coord] & BIT_G1)
        v |= BIT_G1;
      if (p->data[tmp_x_coord] & BIT_B1)
        v |= BIT_B1;
    }
    if (x_coord % 2) {
      p->data[(x_coord) - 1] = v;
    } else {
      p->data[(x_coord) + 1] = v;
    }
  }
}

void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  if ( !everything_OK ) return;
  for (unsigned int matrix_frame_parallel_row = 0; matrix_frame_parallel_row < ROWS_PER_FRAME; matrix_frame_parallel_row++) // half height - 16 iterations
  {
    for (int color_depth_idx = 0; color_depth_idx < PIXEL_COLOR_DEPTH_BITS; color_depth_idx++) // color depth - 8 iterations
    {
      uint16_t mask = (1 << color_depth_idx);
      rowBitStruct *p = &matrix_framebuffer_malloc_1[back_buffer_id].rowdata[matrix_frame_parallel_row].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's
      for (int x_coord = 0; x_coord < MATRIX_WIDTH; x_coord++) // row pixel width 64 iterations
      {
        int v = 0;
        int gpioRowAddress = matrix_frame_parallel_row;
        if (color_depth_idx == 0) gpioRowAddress = matrix_frame_parallel_row - 1;
        if (gpioRowAddress & 0x01) v |= BIT_A; // 1
        if (gpioRowAddress & 0x02) v |= BIT_B; // 2
        if (gpioRowAddress & 0x04) v |= BIT_C; // 4
        if (gpioRowAddress & 0x08) v |= BIT_D; // 8
        if (gpioRowAddress & 0x10) v |= BIT_E; // 16
        if ((x_coord) == 0 ) v |= BIT_OE;
        if ((x_coord) == PIXELS_PER_ROW - 1) v |= BIT_LAT;
        if ((x_coord) == PIXELS_PER_ROW - 2) v |= BIT_OE;
        if ((color_depth_idx > lsbMsbTransitionBit || !color_depth_idx) && ((x_coord) >= brightness)) v |= BIT_OE;
        if (color_depth_idx && color_depth_idx <= lsbMsbTransitionBit) {
          int lsbBrightness = brightness >> (lsbMsbTransitionBit - color_depth_idx + 1);
          if ((x_coord) >= lsbBrightness) v |= BIT_OE;
        }
        if (green & mask) {
          v |= BIT_G1;
          v |= BIT_R2;
        }
        if (blue & mask)  {
          v |= BIT_B1;
          v |= BIT_G2;
        }
        if (red & mask)   {
          v |= BIT_R1;
          v |= BIT_B2;
        }
        if (x_coord % 2) {
          p->data[(x_coord) - 1] = v;
        } else {
          p->data[(x_coord) + 1] = v;
        }
      }
    }
  }
}
