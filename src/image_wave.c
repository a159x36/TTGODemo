/*
   This code generates an effect that should pass the 'fancy graphics'
   qualification as set in the comment in the spi_master code.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "image_wave.h"

#include <math.h>
#include <string.h>

#include <graphics.h>

uint16_t **pixels;

static int image_w;
static int image_h;

// This variable is used to detect the next frame.
static int prev_frame = -1;

// Instead of calculating the offsets for each pixel we grab, we pre-calculate
// the valueswhenever a frame changes, then re-use these as we go through all
// the pixels in the frame. This is much, much faster.


// Calculate the pixel data for a set of lines (with implied line size of 320).
// Pixels go in dest, line is the Y-coordinate of the first line to be
// calculated, linect is the amount of lines to calculate. Frame increases by
// one every time the entire image is displayed; this is used to go to the next
// frame of animation.
void image_wave_calc_lines(uint16_t *dest, int line, int frame, int linect) {
    int16_t xofs[display_width], yofs[display_width];
    int16_t xcomp[display_width], ycomp[display_width];
    int mag = (sin(frame * 0.02) + 1.8) * 256;
    if (frame != prev_frame) {
        // We need to calculate a new set of offset coefficients. Take some
        // random sines as offsets to make everything look pretty and fluid-y.
        for (int x = 0; x < display_width; x++) {
            xofs[x] = sin(frame * 0.15 + x * 0.06) * 6;
            xcomp[x] = sin(frame * 0.11 + x * 0.12) * 6;
        }
        for (int y = 0; y < display_height; y++) {
            yofs[y] = sin(frame * 0.1 + y * 0.05) * 6;
            ycomp[y] = sin(frame * 0.07 + y * 0.15) * 6;
        }
        prev_frame = frame;
    }
    for (int y = line; y < line + linect; y++) {
        int y1 = (((y - display_height / 2) * mag) >> 8) + display_height / 2;
        for (int x = 0; x < display_width; x++) {
            int x1 = (((x - display_width / 2) * mag) >> 8) + display_width / 2;
            int x2 = x1 + yofs[y] + xcomp[x] + image_w / 2 - display_width / 2;
            int y2 = y1 + xofs[x] + ycomp[y] + image_h / 2 - display_height / 2;
            if (x2 < 0 || x2 > image_w - 1 || y2 < 0 || y2 > image_h - 1)
                *dest++ = 0;
            else
                *dest++ = pixels[y2][x2];
        }
    }
}
extern image_header  albany_image;

esp_err_t image_wave_init() {    
    image_w=albany_image.width;
    image_h=albany_image.height;
    pixels=calloc(image_h, sizeof(uint16_t *));
    for(int i=0;i<image_h;i++) {
        uint16_t *line_start=(uint16_t *)&albany_image+6+image_w*i;
        pixels[i]=line_start;
    }
    return 0;
}
