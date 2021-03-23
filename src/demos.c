
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include "fonts.h"
#include "image_wave.h"
#include <graphics.h>
#include "demos.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <time.h>

const char *tag="T Display";
// voltage reference calibration for Battery ADC
uint32_t vref;
time_t time_now;
struct tm *tm_info;
// Simple game of life demo
void life() {
    uint16_t linebuffer[display_width*2];
    for(int i=0;i<(display_width*display_height)/2;i++) {
        int x=rand()%display_width;
        int y=rand()%display_height;
        draw_pixel(x,y,-1);//rand() | 1);
    }
    int speed=1;
    while (1) {
        for(int y=0;y<display_height;y++) {
            uint16_t *pline=frame_buffer+((y+display_height-1)%display_height)*display_width;
            uint16_t *nline=frame_buffer+((y+display_height+1)%display_height)*display_width;
            uint16_t *line=frame_buffer+y*display_width;
            uint16_t *lb=linebuffer+(y%2)*display_width;
            for(int x=1;x<display_width-1;x++) {
                int n=0;
                uint16_t v=line[x];
                n=(pline[x-1]&1)+(pline[x]&1)+(pline[x+1]&1)+
                (line[x-1]&1)+(line[x+1]&1)+
                (nline[x-1]&1)+(nline[x]&1)+(nline[x+1]&1);
                if(n>3 || n<2) v=0;
                if(n==3) v=-1;//rand() | 1;
                *lb++=v;
            }
            lb=linebuffer+((y+1)%2)*display_width;
            memcpy(pline+speed,lb,display_width*2-speed*2+2);
        }
        flip_frame();
        int key=get_input();
        if(key==35) speed++;
        if(key==0) return;
    }
}

extern image_header  spaceship_image;
typedef struct pos {
    int x;
    int y;
    int speed;
    int colour;
} pos;

// simple spaceship and starfield demo
void graphics_demo() {
    int x=(display_width/2)<<8;
    int y=display_height-spaceship_image.height/2;
    int dx=256;
    int ddx=10;
    pos stars[100];
    for(int i=0;i<100;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%(display_height*256);
        stars[i].speed=rand()%512+64;
        stars[i].colour=rand();
    }
    while(1) {
        cls(0);
        for(int i=0;i<100;i++) {
            draw_pixel(stars[i].x,stars[i].y>>8,stars[i].colour);
            stars[i].y += stars[i].speed;
            if(stars[i].y>=display_height*256) {
                stars[i].x=rand()%display_width;
                stars[i].y=0;
                stars[i].speed=rand()%512+64;
            }
        }
        draw_image(&spaceship_image, x>>8,y);
        x=x+dx;
        if(x<0 || x>=(display_width<<8)) {
            dx=-dx;
            x+=dx;
        }
        dx+=ddx;
        if(dx<-500 || dx>500) {
            ddx=-ddx;
            dx+=ddx;
        }
        flip_frame();
        int key=get_input();
        if(key==0) return;
    }
}
// waving image demo showing time
void display() {
    char buff[128];
    int frame = 0;
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
    if (DISPLAY_VOLTAGE) {
        gpio_set_direction(34, GPIO_MODE_INPUT);
        // Configure ADC
        adc1_config_width(ADC_WIDTH_BIT_12);
        // GPIO34 ADC1 CHANNEL 6
        adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
        esp_adc_cal_characteristics_t adc_chars;
        esp_adc_cal_characterize(
            (adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6,
            (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
        vref = adc_chars.vref;
    }
    while (1) {
        frame++;
        if (DISPLAY_IMAGE_WAVE)
            image_wave_calc_lines(frame_buffer, 0, frame, display_height);
        setFont(FONT_DEJAVU24);
        if (DISPLAY_VOLTAGE) {
            setFontColour(20, 0, 200);
            uint32_t raw = adc1_get_raw((adc1_channel_t)ADC1_CHANNEL_6);
            float battery_voltage =
                ((float)raw / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
            snprintf(buff, 128, "%.2fV", battery_voltage);
            setFontColour(0, 0, 0);
            print_xy(buff, 12, 12);
            setFontColour(20, 200, 200);
            print_xy(buff, 10, 10);
        }

        time(&time_now);
        tm_info = localtime(&time_now);
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        snprintf(buff, 128, "%2d:%02d:%02d:%03ld", tm_info->tm_hour,
                 tm_info->tm_min, tm_info->tm_sec, tv_now.tv_usec / 1000);
        setFontColour(0, 0, 0);
        print_xy(buff, 12, 102);
        setFontColour(200, 200, 200);
        print_xy(buff, 10, 100);
        flip_frame();
        current_time = esp_timer_get_time();
        if ((frame % 10) == 0) {
            printf("FPS:%f\n", 1.0e6 / (current_time - last_time));
            vTaskDelay(1);
        }
        last_time = current_time;
        int key=get_input();
        if(key==0) return;
    }
}

void sensors_demo() {
    
}