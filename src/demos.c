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
#include "graphics3d.h"
#include <graphics.h>
#include "demos.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <nvs_flash.h>
#include <time.h>
#include "input_output.h"

const char *tag="T Display";
// voltage reference calibration for Battery ADC
uint32_t vref;
time_t time_now;
struct tm *tm_info;

void showfps() {
    static uint64_t current_time=0;
    static uint64_t last_time=0;
    static int frame=0;
    current_time = esp_timer_get_time();
    if ((frame++ % 20) == 1) {
        printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
        vTaskDelay(1);
    }
    last_time=current_time;
}
// Simple game of life demo
void life_demo() {
    cls(0);
    for(int i=0;i<(display_width*display_height)/2;i++) {
        int x=rand()%(display_width);
        int y=rand()%(display_height-2)+1;
        draw_pixel(x,y,-1);//rand() | 1);
    }
    flip_frame();
    int speed=0;
    while (1) {
        for(int y=1;y<display_height-1;y++) {
            uint16_t *pfb=(frame_buffer==fb1)?fb2:fb1;
            pfb+=y*display_width+1-speed;
            uint16_t *pl=pfb-display_width;
            uint16_t *nl=pfb+display_width;
            uint16_t *cl=frame_buffer+y*display_width+1;
            for(int x=1;x<display_width-1;x++) {
                int n=0;
                uint16_t v=(*pfb & 1);
                n=(pl[-1]&1)+(*pl&1)+(pl[1]&1)+
                    (pfb[-1]&1)+(pfb[1]&1)+(nl[-1]&1)+(*nl&1)+(nl[1]&1);
                if(n>3 || n<2) v=0;
                if(n==3) v=-1;//rand() | 1;
                *cl++=v;
                pl++;
                pfb++;
                nl++;
            }
        
        }
        flip_frame();
        showfps();
        key_type key=get_input();
        if(key==RIGHT_DOWN) speed++;
        if(key==LEFT_DOWN) return;
    }
}
#define NSTARS 100

extern image_header  spaceship_image;
typedef struct pos {
    float x;
    float y;
    float speed;
    int colour;
} pos;


// simple spaceship and starfield demo
void spaceship_demo() {
    float x=display_width/2;
    float y=display_height-spaceship_image.height/2;
    float dx=.2;
    float ddx=.02;
    pos stars[NSTARS];
    for(int i=0;i<NSTARS;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%display_height;
        stars[i].speed=(rand()%512+64)/256.0;
        stars[i].colour=rand();
    }
    while(1) {
        cls(0);
        for(int i=0;i<NSTARS;i++) {
            draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
            stars[i].y += stars[i].speed;
            if(stars[i].y>=display_height) {
                stars[i].x=rand()%display_width;
                stars[i].y=0;
                stars[i].speed=(rand()%512+64)/256.0;
            }
        }
        draw_image(&spaceship_image, x,y);
        x=x+dx;
        if(x<0 || x>=display_width) {
            dx=-dx;
            x+=dx;
        }
        dx+=ddx;
        if(dx<-2 || dx>2) {
            ddx=-ddx;
            dx+=ddx;
        }
        flip_frame();
        showfps();
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
    }
}
// waving image demo showing time
void image_wave_demo() {
    char buff[128];
    int frame = 0;
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
        showfps();
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
    }
}

void teapots_demo() {
    int nteapots=5;
    vec3f rot[nteapots];
    vec2 pos[nteapots];
    int s[nteapots];
    colourtype col[nteapots];
    for(int i=0;i<nteapots; i++) {
        rot[i]=(vec3f){(rand()%31415)/5000.0,
                (rand()%31415)/5000.0,
                (rand()%31415)/5000.0};
        pos[i]=(vec2){rand()%(display_width-20)+10,
                rand()%(display_height-20)+10};
        col[i]=(colourtype){rand()%100+155,
                rand()%100+155,
                rand()%100+155};
        int m=rand()%8;
        if(m==7) m=0;
        if(m&1) col[i].r/=8;
        if(m&2) col[i].g/=8;
        if(m&4) col[i].b/=8;
        
        s[i]=rand()%20+20;
        //if((i&7)==0) col[i].g=100;
    }
    while(1) {
        cls(0);//rgbToColour(0x87,0xce,0xeb));
        for(int i=0;i<nteapots; i++) {
            draw_teapot(pos[i],s[i],rot[i],col[i]);
            rot[i]=add3d(rot[i],(vec3f){0.0523,0.0354,0.0714});
        }
        flip_frame();
        showfps();
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
    }
}
typedef struct obj {
    float x;
    float y;
    int w;
    int h;
    float xvel;
    float yvel;
    uint16_t colour;
} obj;

int overlap(obj *r1, obj *r2) {
    if(r1->x <= r2->x+r2->w && r1->x+r1->w >= r2->x
      && r1->y <= r2->y+r2->h && r1->y+r1->h >= r2->y)
      return 1;
    return 0;
}
extern image_header  bubble;

void bubble_demo() {
    static int high_score=0;
    high_score=storage_read_int("highscore",0);
    char score_str[256];
    pos stars[NSTARS];
    set_orientation(PORTRAIT);
    for(int i=0;i<NSTARS;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%display_height;
        stars[i].speed=(rand()%512+64)/256.0;
        stars[i].colour=rand();
    }
    obj ball;
    ball.x=display_width/2.0;
    ball.y=display_height/2.0;
    ball.w=bubble.width;
    ball.h=bubble.height;
    ball.yvel=0.6;
    ball.xvel=0.7;
    obj bat;
    bat.x=110;
    bat.y=235;
    bat.w=20;
    bat.h=5;
    int score=0;
    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    int keys[2]={1,1};
    uint64_t last_time=esp_timer_get_time();
    while(1) {
        cls(rgbToColour(0,0,0));
        for(int i=0;i<NSTARS;i++) {
            draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
        }
        draw_rectangle(bat.x,bat.y,bat.w,bat.h,-1);
        draw_image(&bubble,ball.x,ball.y);
        setFontColour(0, 255, 0);
        gprintf("Score: %d\n",score);
        setFontColour(100, 100, 155);
        gprintf("HiScore: %d\n",high_score);
        float dt;
        uint64_t time=esp_timer_get_time();
        dt=(time-last_time)/10000.0; // hundredths of secs since boot;
        last_time=time;
        for(int i=0;i<NSTARS;i++) {
            stars[i].y += stars[i].speed*dt;
            if(stars[i].y>=display_height) {
                stars[i].x=rand()%display_width;
                stars[i].y=0;
                stars[i].speed=(rand()%512+64)/256.0;
            }
        }
        ball.yvel-=2.0/16;
        ball.x+=ball.xvel*dt;
        ball.y-=ball.yvel*dt;
        if((ball.x)>(134-ball.w/2) || ball.x<ball.w/2) {
            ball.xvel=-ball.xvel;
            ball.x+=ball.xvel*dt;
        }
        if(ball.y<ball.h/2) {
            ball.yvel=-ball.yvel;
            ball.y-=ball.yvel*dt;
        }
    
        if((ball.y+ball.h/2)>bat.y && (ball.x>bat.x) && (ball.x<bat.x+bat.w) && (ball.y-ball.h/2)<bat.y) {
            ball.yvel=-ball.yvel*1.03;
            ball.x-=-(ball.y-(bat.y-ball.h/2))*ball.xvel/ball.yvel;
            ball.y=bat.y-ball.h/2;
            score++;
        } else {

            float ldist=(ball.x-bat.x)*(ball.x-bat.x)+(ball.y-bat.y)*(ball.y-bat.y);
            float rdist=(ball.x-(bat.x+bat.w))*(ball.x-(bat.x+bat.w))+(ball.y-bat.y)*(ball.y-bat.y);

            if(ldist<(ball.w*ball.w/4) || rdist<(ball.w*ball.w/4)) {
                float v=sqrt(ball.xvel*ball.xvel+ball.yvel*ball.yvel); 
                float nx;
                if(ldist<(ball.w*ball.w/4))
                    nx=(ball.x-bat.x+ball.xvel);
                else
                    nx=(ball.x-(bat.x+bat.w)+ball.xvel);

                float ny=(ball.y-bat.y+ball.yvel);
                float nv=sqrt(nx*nx+ny*ny);
                nx=v*nx/nv;
                ny=-v*ny/nv;
                ball.xvel=nx;
                ball.yvel=ny;
                ball.y=bat.y-ball.h/2;
                ball.x+=ball.xvel;
                score++;
            } 
        }

        key_type key=get_input();
        switch(key) {
            case LEFT_DOWN: keys[0]=0;break;
            case LEFT_UP: keys[0]=1;break;
            case RIGHT_DOWN: keys[1]=0;break;
            case RIGHT_UP: keys[1]=1;break;
            case NO_KEY: break;
        }

        if(ball.y>343) {
            break;
        }
        if(keys[0]==0) {
            bat.x-=2*dt;
            if(bat.x<0) bat.x=0;
        }
        if(keys[1]==0) {
            bat.x+=2*dt;
            if(bat.x>134-bat.w) bat.x=(134-20);
        }
        flip_frame();
        showfps();
    }
    setFont(FONT_DEJAVU18);
    setFontColour(255, 0, 0);
    print_xy("Game Over", CENTER, CENTER);
    setFontColour(0, 255, 0);
    snprintf(score_str,64,"Score: %d",score);
    print_xy(score_str,CENTER, LASTY+18);
    flip_frame();
    if(score>high_score) {
        high_score=score;
        storage_write_int("highscore",score);
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
    while(get_input());
    while(get_input()!=RIGHT_DOWN)
        vTaskDelay(1);
}