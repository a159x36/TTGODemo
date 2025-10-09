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
#include <driver/gpio.h>
#include "fonts.h"
#include "image_wave.h"
#include "graphics3d.h"
#include <graphics.h>
#include "demos.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <nvs_flash.h>
#include <time.h>
#include "input_output.h"
#include "rgb_led.h"
#include "networking.h"
#include <mqtt_client.h>

const char *tag="T Display";


void showfps() {
    static uint64_t last_time=0;
    static int frame=0;
    uint64_t current_time = esp_timer_get_time();
    if ((frame++ % 20) == 1) {
        printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
        vTaskDelay(0);
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

static float last_star_time=0;
void initstars(pos *stars) {
    for(int i=0;i<NSTARS;i++) {
        stars[i]=(pos){rand()%display_width,rand()%display_height,(rand()%512+64)/256.0,rand()};
    }
    last_star_time=esp_timer_get_time();
}
float drawstars(pos *stars) {
    float dt;
    uint64_t time=esp_timer_get_time();
    dt=(time-last_star_time)/10000.0;
    last_star_time=time;
    for(int i=0;i<NSTARS;i++) {
        draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
        stars[i].y += stars[i].speed*dt;
        if(stars[i].y>=display_height) {
            stars[i]=(pos){rand()%display_width,0,(rand()%512+64)/256.0,rand()};
        }
    }
    return dt;
}
// simple spaceship and starfield demo
void spaceship_demo() {
    // x and it's derivatives
    float x[]={display_width/2,0.2f,0.076f,0.003f};
    float xmin[]={0,-3,-1};
    float xmax[]={display_width-1,3,1};
    float y=display_height-spaceship_image.height/2;
    pos * stars=malloc(sizeof(pos)*NSTARS);
    initstars(stars);
    while(1) {
        cls(0);
        drawstars(stars);
        draw_image(&spaceship_image, x[0],y);
        for(int i=0;i<(sizeof(x)/sizeof(x[0]))-1;i++) {
            x[i]+=x[i+1];
            if(x[i]<xmin[i] || x[i]>xmax[i]) {
                x[i+1]=-x[i+1];
                x[i]+=x[i+1];
            }
        }
        flip_frame();
        showfps();
        key_type key=get_input();
        if(key==LEFT_DOWN) {
            free(stars);
            return;
        }
    }
}
// waving image demo showing time
void image_wave_demo() {
    char buff[128];
    int frame = 0;
    // voltage reference calibration for Battery ADC
    uint32_t vref;  
    adc_oneshot_unit_handle_t adc1_handle;
    if (DISPLAY_VOLTAGE) {
        gpio_set_direction(VOLTAGE_GPIO, GPIO_MODE_INPUT);
        // Configure ADC
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_12,
            .atten = ADC_ATTEN_DB_12,
        };
        adc_oneshot_config_channel(adc1_handle, VOLTAGE_ADC, &config);
        vref = 1100;//adc_chars.vref;
    }
    while (1) {
        frame++;
        if (DISPLAY_IMAGE_WAVE)
            image_wave_calc_lines(frame_buffer, 0, frame, display_height);
        setFont(FONT_DEJAVU24);
        if (DISPLAY_VOLTAGE) {
            setFontColour(20, 0, 200);
            int raw;
            adc_oneshot_read(adc1_handle,VOLTAGE_ADC,&raw);
            float battery_voltage = ((float)raw / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
            snprintf(buff, 128, "%.2fV %d", battery_voltage, raw);
            setFontColour(0, 0, 0);
            print_xy(buff, 12, 12); 
            setFontColour(20, 200, 200);
            print_xy(buff, 10, 10);
        }
        time_t time_now;
        struct tm *tm_info;
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
        if(key==LEFT_DOWN) {
            if(DISPLAY_VOLTAGE)
                adc_oneshot_del_unit(adc1_handle);
            return;
        }
    }
}

typedef struct {
    vec3f rot;
    vec2f pos;
    int size;
    vec3f spin;
    colourtype col;
} teapot_data;

void teapots_demo() {
    int nteapots=10;
//    srand(0); //uncomment for the same teapots each time
    teapot_data tpd[nteapots];
    
    for(int i=0;i<nteapots; i++) {
        tpd[i].rot=(vec3f){(rand()%31415)/5000.0,
                (rand()%31415)/5000.0,
                (rand()%31415)/5000.0};
        tpd[i].pos=(vec2f){rand()%(display_width-20)+10,
                rand()%(display_height-20)+10};
        tpd[i].col=(colourtype){rand()%100+155,
                rand()%100+155,
                rand()%100+155};
        tpd[i].spin=(vec3f){(rand()%10000-5000)/100000.0,
                (rand()%10000-5000)/100000.0,
                (rand()%10000-5000)/100000.0};
        int m=rand()%8;
        if(m==7) m=0;
        if(m&1) tpd[i].col.r/=8;
        if(m&2) tpd[i].col.g/=8;
        if(m&4) tpd[i].col.b/=8;
        tpd[i].size=rand()%20+10;
    }
    while(1) {
        cls(0);
        uint64_t times1=0,times2=0;
        for(int i=0;i<nteapots; i++) {
            uint64_t time1,time2;
            draw_teapot(tpd[i].pos,tpd[i].size,tpd[i].rot,tpd[i].col,false,&time1,&time2);
            times1+=time1;
            times2+=time2-time1;
            tpd[i].rot=add3d(tpd[i].rot,tpd[i].spin);
        }
        draw_rectangle(0,0,88,16,rgbToColour(40,0,0));
        gprintf("%.2f %.2f",times1/1000.0f,times2/1000.0f);
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
    pos * stars=malloc(sizeof(pos)*NSTARS);
    char score_str[256];
    set_orientation(PORTRAIT);
    initstars(stars);
    obj ball;
    ball.x=display_width/2.0;
    ball.y=display_height/2.0;
    ball.w=bubble.width;
    ball.h=bubble.height;
    ball.yvel=0.6;
    ball.xvel=0.7;
    obj bat;
    bat.x=display_width/2-10;
    bat.y=display_height-5;
    bat.w=20;
    bat.h=5;
    int score=0;
    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    int keys[2]={1,1};
    while(1) {
        cls(rgbToColour(0,0,0));
        float dt=drawstars(stars);
        draw_rectangle(bat.x,bat.y,bat.w,bat.h,-1);
        draw_image(&bubble,ball.x,ball.y);
        setFontColour(0, 255, 0);
        gprintf("Score: %d\n",score);
        setFontColour(100, 100, 155);
        gprintf("HiScore: %d\n",high_score);
        ball.yvel-=2.0/16;
        ball.x+=ball.xvel*dt;
        ball.y-=ball.yvel*dt;
        if((ball.x)>(display_width-ball.w/2) || ball.x<ball.w/2) {
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

        if(ball.y>display_height+10) {
            break;
        }
        if(keys[0]==0) {
            bat.x-=2*dt;
            if(bat.x<0) bat.x=0;
        }
        if(keys[1]==0) {
            bat.x+=2*dt;
            if(bat.x>display_width-bat.w-1) bat.x=(display_width-1-20);
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
        vTaskDelay(100/portTICK_PERIOD_MS);
    free(stars);
}

int colR=1,colG=128,colB=1;
int colR1=128,colG1=1,colB1=1;
int mode=0;
int delay=1000;

#define TOPICROOT "/tree"
static void my_mqtt_led_callback(int event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    if(event_id==MQTT_EVENT_CONNECTED) {
        esp_mqtt_client_handle_t client = event->client;
        esp_mqtt_client_subscribe(client, TOPICROOT"/colour1", 0);
        esp_mqtt_client_subscribe(client, TOPICROOT"/colour2", 0);
        esp_mqtt_client_subscribe(client, TOPICROOT"/mode", 0);
        esp_mqtt_client_subscribe(client, TOPICROOT"/delay", 0);
    } else if(event_id==MQTT_EVENT_DATA) {    
        event->data[event->data_len]=0;
        int r,g,b;
        if(!strncmp(event->topic,TOPICROOT"/mode",event->topic_len))
            sscanf(event->data,"%d",&mode);
        if(!strncmp(event->topic,TOPICROOT"/colour1",event->topic_len)) {
            if(sscanf(event->data,"%d,%d,%d",&r,&g,&b)>=3) {
                colR=r;colG=g;colB=b;
            }
        }
        if(!strncmp(event->topic,TOPICROOT"/colour2",event->topic_len)) {
            if(sscanf(event->data,"%d,%d,%d",&r,&g,&b)>=3) {
                colR1=r;colG1=g;colB1=b;
            }
        }
        if(!strncmp(event->topic,TOPICROOT"/delay",event->topic_len))
            sscanf(event->data,"%d",&delay);
    }
}


void mqtt_leds() {
    strand_t STRAND = {.rmtChannel = 0,.gpioNum = 17,.ledType = LED_WS2812B_V3,.brightLimit = 255,.numPixels = 256, };
    gpio_set_direction(17, GPIO_MODE_OUTPUT);
    digitalLeds_initStrands(&STRAND, 1);
    digitalLeds_resetPixels(&STRAND);
    int v=0;
    int it=0;
    mqtt_connect(my_mqtt_led_callback);

     while (1) {
         
         cls(bg_col);
         setFont(FONT_DEJAVU18);
         setFontColour(0,0,0);
         draw_rectangle(3,0,display_width,18,rgbToColour(220,220,0));
         print_xy("MQTT LEDs\n",5,3);
         setFont(FONT_UBUNTU16);
         setFontColour(255,255,255);
         gprintf("go to http://mqtt.webhop.org\n");
         gprintf(network_event);
         setFontColour(0,255,0);
         gprintf("Mode %d\nCol1 (%d,%d,%d)\nCol2 (%d,%d,%d)\nDelay %d\n",
                    mode,colR,colG,colB,colR1,colG1,colB1,delay);
         flip_frame();
         if(get_input()==LEFT_DOWN) break;
         int r,g,b;
         switch(mode) {
             case 0: // 1/30 shift
                 for (uint16_t i = STRAND.numPixels-1; i>0; i--) {
                     STRAND.pixels[i] = STRAND.pixels[i-1];
                 }
                 if(v==0) {
                     it++;
                     if(it%2==0) {
                         if(colR==0) r=rand()&255; else r=colR;
                         if(colG==0) g=rand()&255; else g=colG;
                         if(colB==0) b=rand()&255; else b=colB;
                     } else {
                         if(colR1==0) r=rand()&255; else r=colR1;
                         if(colG1==0) g=rand()&255; else g=colG1;
                         if(colB1==0) b=rand()&255; else b=colB1;
                     }
                     STRAND.pixels[0]=pixelFromRGB(r,g,b);
                 }
                     else STRAND.pixels[0] = pixelFromRGB(0,0,0);
                 break;
             case 1: // 1/30 shift other way
                 for (uint16_t i = 0;i<STRAND.numPixels-1; i++) {
                     STRAND.pixels[i] = STRAND.pixels[i+1];
                 }
                 if(v==0) {
                     it++;
                     if(it%2==0) {
                         if(colR==0) r=rand()&255; else r=colR;
                         if(colG==0) g=rand()&255; else g=colG;
                         if(colB==0) b=rand()&255; else b=colB;
                     } else {
                         if(colR1==0) r=rand()&255; else r=colR1;
                         if(colG1==0) g=rand()&255; else g=colG1;
                         if(colB1==0) b=rand()&255; else b=colB1;
                     }
                     STRAND.pixels[STRAND.numPixels-1]=pixelFromRGB(r,g,b);
                 }
                     else STRAND.pixels[STRAND.numPixels-1] = pixelFromRGB(0,0,0);
                 break;
             case 2: // all on 
                 it++;
                 if(it>50) {
                     if(colR1==0) r=rand()&255; else r=colR1;
                     if(colG1==0) g=rand()&255; else g=colG1;
                     if(colB1==0) b=rand()&255; else b=colB1;
                 } else {
                     if(colR==0) r=rand()&255; else r=colR;
                     if(colG==0) g=rand()&255; else g=colG;
                     if(colB==0) b=rand()&255; else b=colB;
                 }
                 if(it>100)
                     it=0;
                 for (uint16_t i = 0;i<STRAND.numPixels; i++)
                     STRAND.pixels[i] =  pixelFromRGB(r>>4,g>>4,b>>4);
                
                 break;
             case 3:
                 for (uint16_t i = 0;i<STRAND.numPixels; i++)
                     STRAND.pixels[i] =  pixelFromRGB(rand()&255/16,rand()&255/16,rand()&255/16);
                 break;
             case 4:
                 for (uint16_t i = 0;i<STRAND.numPixels; i++) {
                     
                     float d=((sinf((float)(i+it)/15.0f))+1.0f)/2.0f;
                    // ets_printf("%d %f\n",i,d);
                     r=d*colR+(1.0f-d)*colR1;
                     g=d*colG+(1.0f-d)*colG1;
                     b=d*colB+(1.0f-d)*colB1;
                     STRAND.pixels[i] =  pixelFromRGB(r,g,b);
                 }
                 it++;
                 break;
            case 5:
                for (int i = 0;i<STRAND.numPixels; i++) {
                    STRAND.pixels[i] =  pixelFromRGB(i,0,0);
                }
                break;
         }
         v=(v+1)%30;
         digitalLeds_updatePixels(&STRAND);
//         vTaskDelay(delay);
         delay_us(delay*100);
     //    if(!gpio_get_level(0)) delay--;
     //    if(!gpio_get_level(35)) delay++;
         if(delay<0) delay=0;
     }
     digitalLeds_free(&STRAND);
     mqtt_disconnect();
}
void led_instructions() {
    cls(bg_col);
    setFont(FONT_DEJAVU18);
    setFontColour(0,0,0);
    draw_rectangle(0,0,display_width,18,rgbToColour(220,220,0));
    gprintf("LED Demo\n");
    setFont(FONT_UBUNTU16);
    setFontColour(255,255,255);
    gprintf("Connect LEDs to GPIO 17\nor switch to the\nrgbled window\non the emulator\n");
    flip_frame();
}
void led_numbers(void) {
    strand_t STRANDS[] = { {.rmtChannel = 0, .gpioNum = 17, .ledType = LED_WS2812B_V3, .brightLimit = 128, .numPixels = 256},};
    strand_t *pStrand= STRANDS;
    gpio_set_direction(17, GPIO_MODE_OUTPUT);
    digitalLeds_initStrands(pStrand, 1);
    digitalLeds_resetPixels(pStrand);
    int n=0;
    char str[2]="0";
    int delay=1000;
    led_instructions();
    while(get_input()!=LEFT_DOWN) {
        draw_rectangle(0,0,16,16,0);
        setFont(FONT_DEJAVU18);
        setFontColour((200+n)&255,n&255,(100+n)&255);
        str[0]=n%10+'0';
        n++;
        print_xy(str,2,1);
        for(int i=0;i<16;i++)
            for(int j=0;j<16;j++) {
                uint16_t pixel=frame_buffer[j+i*display_width];
                uint8_t r=(pixel>>11)<<3;
                uint8_t g=(pixel>>5)<<2;
                uint8_t b=pixel<<3;
                pStrand->pixels[((i&1)?j:(15-j))+i*16]=pixelFromRGB(r/2, g/2, b/2);
            }
        digitalLeds_updatePixels(pStrand);
        delay_us(delay*100);
    }
    digitalLeds_free(pStrand);
}

void led_cube(void) {
    strand_t STRANDS[] = { {.rmtChannel = 0, .gpioNum = 17, .ledType = LED_WS2812B_V3, .brightLimit = 128, .numPixels = 256},};
    strand_t *pStrand= STRANDS;
    vec3f rot=(vec3f){(rand()%31415)/5000.0,(rand()%31415)/5000.0,(rand()%31415)/5000.0};;
    vec2f pos=(vec2f){16,16};
    float size=8;
    gpio_set_direction(17, GPIO_MODE_OUTPUT);
    digitalLeds_initStrands(pStrand, 1);
    digitalLeds_resetPixels(pStrand);
    int delay=100;
    led_instructions();
    while(get_input()!=LEFT_DOWN) {
        draw_rectangle(0,0,32,32,rgbToColour(20,20,20));
        draw_cube(pos,size,rot);
        rot=add3d(rot,(vec3f){0.0523,0.0354,0.0714});
        for(int i=0;i<16;i++)
            for(int j=0;j<16;j++) {
                uint16_t rr=0,gg=0,bb=0;
                for(int ii=0;ii<2;ii++)
                    for(int jj=0;jj<2;jj++) {
                        uint16_t pixel=frame_buffer[j*2+jj+(i*2+ii)*display_width];
                        uint8_t r=(pixel>>11)<<3;
                        uint8_t g=(pixel>>5)<<2;
                        uint8_t b=pixel<<3;
                        rr+=r;gg+=g;bb+=b;
                    }
                pStrand->pixels[((i&1)?j:(15-j))+i*16]=pixelFromRGB(rr/8, gg/8, bb/8);
            }
        digitalLeds_updatePixels(pStrand);
        delay_us(delay*100);
        showfps();
    }
    digitalLeds_free(pStrand);
}

void led_circles(void) {
    float xo=7.5,yo=7.5;
    strand_t STRANDS[] = { {.rmtChannel = 0, .gpioNum = 17, .ledType = LED_WS2812B_V3, .brightLimit = 16, .numPixels = 256},};
    strand_t *pStrand= &STRANDS[0];
    gpio_set_direction(17, GPIO_MODE_OUTPUT);
    digitalLeds_initStrands(pStrand, 1);
    digitalLeds_resetPixels(pStrand);
    float bright=64;
    float offset=0;
    int delay=100;
    led_instructions();
    while(get_input()!=LEFT_DOWN) {
        for (uint16_t i = 0; i < pStrand->numPixels; i++) {
            int y1 = i / 16;
            int x1 = i % 16;
            if (y1 % 2) x1 = 15 - x1;
            y1 = 15 - y1;
            float d =
                (sqrt((y1 - yo) * (y1 - yo) + (x1 - xo) * (x1 - xo)) +
                offset) /
                4.0;
            int red=(int)round(bright*sin(1.0299*d)+bright);
            int green=(int)round(bright*cos(3.2235*d)+bright);
            int blue=(int)round(bright*sin(5.1234*d)+bright);
            pStrand->pixels[i] = pixelFromRGB(red/2, green/2, blue/2);
        }
        digitalLeds_updatePixels(pStrand);
        offset-=0.1f;
        delay_us(delay*100);
        if(!gpio_get_level(0)) delay--;
        if(!gpio_get_level(35)) delay++;
        if(delay<0) delay=0;
        showfps();
    }
    digitalLeds_free(pStrand);
}