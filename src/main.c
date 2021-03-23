/* TTGO Demo example for 159236

*/
#include <driver/gpio.h>

#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/touch_pad.h>

#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"
#include "image_wave.h"
#include "demos.h"

#define PAD_START 3
#define PAD_END 5

// #define SHOW_PADS
/*
 This code has been modified from the espressif spi_master demo code
 it displays some demo graphics on the 240x135 LCD on a TTGO T-Display board.
*/


const int TOUCH_PADS[4]={2,3,8,9};

// for button inputs
QueueHandle_t inputQueue;
uint64_t lastkeytime=0;

// interrupt handler for button presses on GPIO0 and GPIO35
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static int pval[2]={1,1};
    uint32_t gpio_num = (uint32_t) arg;
    int gpio_index=(gpio_num==35);
    int val=(1-pval[gpio_index]);

    uint64_t time=esp_timer_get_time();
    uint64_t timesince=time-lastkeytime;
    // ets_printf("gpio_isr_handler %d %d %lld\n",gpio_num,val, timesince);
    // the buttons can be very bouncy so debounce by checking that it's been .5ms since the last
    // change and that it's pressed down
    if(timesince>500 && val==0) {
        xQueueSendFromISR(inputQueue,&gpio_num,0);

    }
    pval[gpio_index]=val;
    lastkeytime=time;
    gpio_set_intr_type(gpio_num,val==0?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);

}

// get a button press, returns -1 if no button has been pressed
// otherwise the gpio of the button.
int get_input() {
    int key;
    if(xQueueReceive(inputQueue,&key,0)==pdFALSE)
        return -1;
    return key;
}

typedef struct { int x; int y; int z;} vec3;
typedef struct { float x; float y; float z;} vec3f;

int offsetx;
int offsety;

void point3d_to_xy(vec3 *p, int16_t *xx, int16_t *yy) {
    int x=p->x>>16;
    int y=p->y>>16;
    int z=p->z>>16;
    x=x+((z*x)>>9)+offsetx;
    y=y+((z*y)>>9)+offsety;
    *xx=x;
    *yy=y;
}

void draw_line_3d(vec3 p0, vec3 p1, uint16_t colour) {
    int16_t x[2],y[2];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    draw_line(x[0],y[0],x[1],y[1], colour);
}

void draw_triangle_3d(vec3 p0, vec3 p1, vec3 p2, uint16_t colour) {
    int16_t x[3],y[3];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    point3d_to_xy(&p2,&x[2],&y[2]);
    draw_triangle(x[0],y[0],x[1],y[1],x[2],y[2], colour);
}

inline vec3 sub3d(vec3 p0, vec3 p1) {
    vec3 p={p0.x-p1.x, p0.y-p1.y, p0.z-p1.z};
    return p;
}

inline vec3 cross3d(vec3 p0, vec3 p1) {
    vec3 p;
    p.x=(p0.y>>16)*(p1.z>>16)-(p0.z>>16)*(p1.y>>16);
    p.y=(p0.z>>16)*(p1.x>>16)-(p0.x>>16)*(p1.z>>16);
    p.z=(p0.x>>16)*(p1.y>>16)-(p0.y>>16)*(p1.x>>16);
    return p;
}

inline float Q_rsqrt( float number )
{	
	const float x2 = number * 0.5F;
	const float threehalfs = 1.5F;

	union {
		float f;
		uint32_t i;
	} conv  = { .f = number };
	conv.i  = 0x5f3759df - ( conv.i >> 1 );
	conv.f  *= threehalfs - ( x2 * conv.f * conv.f );
	return conv.f;
}

vec3 normalise(vec3 p) {
    float mag;//=Q_rsqrt(p.x*p.x+p.y*p.y+p.z*p.z);

    mag=1.0/sqrtf((float)(p.x*p.x+p.y*p.y+p.z*p.z));
    vec3 p1={(p.x*256)*mag, (p.y*256)*mag, (p.z*256)*mag};
    return p1;
}

int dot(vec3 p0,vec3 p1) {
    return ((p0.x*p1.x)>>8)+((p0.y*p1.y)>>8)+((p0.z*p1.z)>>8);
}

inline int clamp(int x,int min,int max) {
    if(x<min) return min;
    if(x>max) return max;
    return x;
}

vec3 lightpos={0,0,255};

vec3f teapot_v[]={ 
    {  0.2000,  0.0000, 2.70000 }, {  0.2000, -0.1120, 2.70000 },
    {  0.1120, -0.2000, 2.70000 }, {  0.0000, -0.2000, 2.70000 },
    {  1.3375,  0.0000, 2.53125 }, {  1.3375, -0.7490, 2.53125 },
    {  0.7490, -1.3375, 2.53125 }, {  0.0000, -1.3375, 2.53125 },
    {  1.4375,  0.0000, 2.53125 }, {  1.4375, -0.8050, 2.53125 },
    {  0.8050, -1.4375, 2.53125 }, {  0.0000, -1.4375, 2.53125 },
    {  1.5000,  0.0000, 2.40000 }, {  1.5000, -0.8400, 2.40000 },
    {  0.8400, -1.5000, 2.40000 }, {  0.0000, -1.5000, 2.40000 },
    {  1.7500,  0.0000, 1.87500 }, {  1.7500, -0.9800, 1.87500 },
    {  0.9800, -1.7500, 1.87500 }, {  0.0000, -1.7500, 1.87500 },
    {  2.0000,  0.0000, 1.35000 }, {  2.0000, -1.1200, 1.35000 },
    {  1.1200, -2.0000, 1.35000 }, {  0.0000, -2.0000, 1.35000 },
    {  2.0000,  0.0000, 0.90000 }, {  2.0000, -1.1200, 0.90000 },
    {  1.1200, -2.0000, 0.90000 }, {  0.0000, -2.0000, 0.90000 },
    { -2.0000,  0.0000, 0.90000 }, {  2.0000,  0.0000, 0.45000 },
    {  2.0000, -1.1200, 0.45000 }, {  1.1200, -2.0000, 0.45000 },
    {  0.0000, -2.0000, 0.45000 }, {  1.5000,  0.0000, 0.22500 },
    {  1.5000, -0.8400, 0.22500 }, {  0.8400, -1.5000, 0.22500 },
    {  0.0000, -1.5000, 0.22500 }, {  1.5000,  0.0000, 0.15000 },
    {  1.5000, -0.8400, 0.15000 }, {  0.8400, -1.5000, 0.15000 },
    {  0.0000, -1.5000, 0.15000 }, { -1.6000,  0.0000, 2.02500 },
    { -1.6000, -0.3000, 2.02500 }, { -1.5000, -0.3000, 2.25000 },
    { -1.5000,  0.0000, 2.25000 }, { -2.3000,  0.0000, 2.02500 },
    { -2.3000, -0.3000, 2.02500 }, { -2.5000, -0.3000, 2.25000 },
    { -2.5000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 2.02500 },
    { -2.7000, -0.3000, 2.02500 }, { -3.0000, -0.3000, 2.25000 },
    { -3.0000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 1.80000 },
    { -2.7000, -0.3000, 1.80000 }, { -3.0000, -0.3000, 1.80000 },
    { -3.0000,  0.0000, 1.80000 }, { -2.7000,  0.0000, 1.57500 },
    { -2.7000, -0.3000, 1.57500 }, { -3.0000, -0.3000, 1.35000 },
    { -3.0000,  0.0000, 1.35000 }, { -2.5000,  0.0000, 1.12500 },
    { -2.5000, -0.3000, 1.12500 }, { -2.6500, -0.3000, 0.93750 },
    { -2.6500,  0.0000, 0.93750 }, { -2.0000, -0.3000, 0.90000 },
    { -1.9000, -0.3000, 0.60000 }, { -1.9000,  0.0000, 0.60000 },
    {  1.7000,  0.0000, 1.42500 }, {  1.7000, -0.6600, 1.42500 },
    {  1.7000, -0.6600, 0.60000 }, {  1.7000,  0.0000, 0.60000 },
    {  2.6000,  0.0000, 1.42500 }, {  2.6000, -0.6600, 1.42500 },
    {  3.1000, -0.6600, 0.82500 }, {  3.1000,  0.0000, 0.82500 },
    {  2.3000,  0.0000, 2.10000 }, {  2.3000, -0.2500, 2.10000 },
    {  2.4000, -0.2500, 2.02500 }, {  2.4000,  0.0000, 2.02500 },
    {  2.7000,  0.0000, 2.40000 }, {  2.7000, -0.2500, 2.40000 },
    {  3.3000, -0.2500, 2.40000 }, {  3.3000,  0.0000, 2.40000 },
    {  2.8000,  0.0000, 2.47500 }, {  2.8000, -0.2500, 2.47500 },
    {  3.5250, -0.2500, 2.49375 }, {  3.5250,  0.0000, 2.49375 },
    {  2.9000,  0.0000, 2.47500 }, {  2.9000, -0.1500, 2.47500 },
    {  3.4500, -0.1500, 2.51250 }, {  3.4500,  0.0000, 2.51250 },
    {  2.8000,  0.0000, 2.40000 }, {  2.8000, -0.1500, 2.40000 },
    {  3.2000, -0.1500, 2.40000 }, {  3.2000,  0.0000, 2.40000 },
    {  0.0000,  0.0000, 3.15000 }, {  0.8000,  0.0000, 3.15000 },
    {  0.8000, -0.4500, 3.15000 }, {  0.4500, -0.8000, 3.15000 },
    {  0.0000, -0.8000, 3.15000 }, {  0.0000,  0.0000, 2.85000 },
    {  1.4000,  0.0000, 2.40000 }, {  1.4000, -0.7840, 2.40000 },
    {  0.7840, -1.4000, 2.40000 }, {  0.0000, -1.4000, 2.40000 },
    {  0.4000,  0.0000, 2.55000 }, {  0.4000, -0.2240, 2.55000 },
    {  0.2240, -0.4000, 2.55000 }, {  0.0000, -0.4000, 2.55000 },
    {  1.3000,  0.0000, 2.55000 }, {  1.3000, -0.7280, 2.55000 },
    {  0.7280, -1.3000, 2.55000 }, {  0.0000, -1.3000, 2.55000 },
    {  1.3000,  0.0000, 2.40000 }, {  1.3000, -0.7280, 2.40000 },
    {  0.7280, -1.3000, 2.40000 }, {  0.0000, -1.3000, 2.40000 },
};


//typedef struct { int i[16] } patch;

int patches[9][16]={
//  Rim:
    { 102, 103, 104, 105,   4,   5,   6,   7,
        8,   9,  10,  11,  12,  13,  14,  15 },
//  Body:
    {  12,  13,  14,  15,  16,  17,  18,  19,
       20,  21,  22,  23,  24,  25,  26,  27 },
    {  24,  25,  26,  27,  29,  30,  31,  32,
       33,  34,  35,  36,  37,  38,  39,  40 },
//  Lid:
    {  96,  96,  96,  96,  97,  98,  99, 100,
      101, 101, 101, 101,   0,   1,   2,   3 },
    {   0,   1,   2,   3, 106, 107, 108, 109,
      110, 111, 112, 113, 114, 115, 116, 117 },
//  Handle:
    {  41,  42,  43,  44,  45,  46,  47,  48,
       49,  50,  51,  52,  53,  54,  55,  56 },
    {  53,  54,  55,  56,  57,  58,  59,  60,
       61,  62,  63,  64,  28,  65,  66,  67 },
//  Spout:
    {  68,  69,  70,  71,  72,  73,  74,  75,
       76,  77,  78,  79,  80,  81,  82,  83 },
    {  80,  81,  82,  83,  84,  85,  86,  87,
       88,  89,  90,  91,  92,  93,  94,  95 },
};

#define SZ (20<<16)

float rmx[3][3];

inline void maketrotationmatrix(float alpha, float beta, float gamma) {
    float ca=cos(alpha);
    float cb=cos(beta);
    float cc=cos(gamma);
    float sa=sin(alpha);
    float sb=sin(beta);
    float sc=sin(gamma);

    rmx[0][0]=ca*cb;
    rmx[0][1]=ca*sb*sc-sa*cc;
    rmx[0][2]=ca*sb*cc+sa*sc;
    rmx[1][0]=sa*cb;
    rmx[1][1]=sa*sb*sc+ca*cc;
    rmx[1][2]=sa*sb*cc-ca*sc;
    rmx[2][0]=-sb;
    rmx[2][1]=cb*sc;
    rmx[2][2]=cb*cc;
}

void draw_quad_3d(vec3 p0, vec3 p1, vec3 p2, vec3 p3, uint16_t red, uint16_t green, uint16_t blue) {

    vec3 normal=normalise(cross3d(sub3d(p1,p0),sub3d(p3,p0)));
    if(normal.z<0) return;

    int light=clamp(dot(normal,lightpos)+32,0,255);

    uint16_t colour=rgbToColour((red*light)>>8, (green*light)>>8, (blue*light)>>8);

    draw_triangle_3d(p0, p1, p2, colour);
    draw_triangle_3d(p2, p3, p0, colour);

  //  draw_triangle_3d(p0, p3, p2, colour);
  //  draw_triangle_3d(p2, p1, p0, colour);

/*
    colour=rgbToColour(255,255,255);
    draw_line_3d(p0,p1,colour);
    draw_line_3d(p1,p2,colour);
    draw_line_3d(p2,p3,colour);
    draw_line_3d(p3,p0,colour);
    
    draw_line_3d(p0,p2,colour);
    draw_line_3d(p1,p3,colour);
*/
    /*
    int16_t x[4],y[4];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    point3d_to_xy(&p2,&x[2],&y[2]);
    point3d_to_xy(&p3,&x[3],&y[3]);
    uint16_t col=0xffff;
    draw_line(x[0],y[0],x[1],y[1],col);
    draw_line(x[1],y[1],x[2],y[2],col);
    draw_line(x[2],y[2],x[3],y[3],col);
    draw_line(x[3],y[3],x[0],y[0],col);
    */

}

void vrotate(vec3 *v1,vec3f v, float f0, float f1, float f2) {
    v.x=v.x*f0;
    v.y=v.y*f1;
    v.z=v.z*f2;
    v1->x=(int)(rmx[0][0]*v.x+rmx[0][1]*v.y+rmx[0][2]*v.z);
    v1->y=(int)(rmx[1][0]*v.x+rmx[1][1]*v.y+rmx[1][2]*v.z);
    v1->z=(int)(rmx[2][0]*v.x+rmx[2][1]*v.y+rmx[2][2]*v.z);
}

void draw_teapot() {

    for(int i=0;i<9;i++) {
        vec3 p[4][4];
        vec3 q[4][4];
        vec3 r[4][4];
        vec3 s[4][4];
        for(int j=0;j<4;j++) {
            for(int k=0;k<4; k++) {
                vrotate(&p[j][k],teapot_v[patches[i][j*4+k]],SZ,SZ,SZ);
                vrotate(&q[j][k],teapot_v[patches[i][j*4+3-k]],SZ,-SZ,SZ);
            }
            if(i<6) {
                for(int k=0;k<4; k++) {
                    vrotate(&r[j][k],teapot_v[patches[i][j*4+3-k]],-SZ,SZ,SZ);
                    vrotate(&s[j][k],teapot_v[patches[i][j*4+k]],-SZ,-SZ,SZ);
                }
            }
        }
        for(int j=1;j<4;j++) {
            for(int k=0;k<4;k++) {
                int l=(k+1)%4;
                draw_quad_3d(p[j][k],p[j][l],p[j-1][l],p[j-1][k],200,32,40);
            //    draw_quad_3d(q[j][k],q[j-1][k],q[j-1][l],q[j][l],200,32,40);
                if(i<6) {
            //        draw_quad_3d(r[j][k],r[j][l],r[j-1][l],r[j-1][k],200,32,40);
            //        draw_quad_3d(s[j][k],s[j][l],s[j-1][l],s[j-1][k],200,32,40);
                }
            }
        }
    }
}



// menu with a rotating cube, because... why not.
int demo_menu(int select) {
    // for fps calculation
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
    int bx=83,by=57;
    int vbx=1,vby=1;
    while(1) {
    // cube vertices
        vec3 vertex[8]={
            {-SZ,-SZ,-SZ},{-SZ,-SZ,SZ},{-SZ,SZ,-SZ},{-SZ,SZ,SZ},
            {SZ,-SZ,-SZ},{SZ,-SZ,SZ},{SZ,SZ,-SZ},{SZ,SZ,SZ}
        };
        float alpha=0;
        float beta=0;
        float gamma=0;
        for(int frame=0;frame < 4000; frame++) {
            maketrotationmatrix(alpha,beta,gamma);
            alpha+=0.01;
            beta+=0.02;
            gamma+=0.017;
            cls(rgbToColour(100,20,20));
            setFont(FONT_DEJAVU18);
            setFontColour(255, 255, 255);
            draw_rectangle(0,5,display_width,24,rgbToColour(220,220,0));
            draw_rectangle(0,select*18+24+5,display_width,18,rgbToColour(0,180,180));
            offsetx=display_width/2;
            offsety=display_height/2;
            draw_teapot();
            if(get_orientation()) offsety+=display_height/4;
            else offsetx+=display_width/4;
            /*
            for(int i=0;i<8;i++) {
                for(int j=i+1;j<8;j++) {
                    int v=i ^ j; // bit difference
                    if(v && !(v&(v-1))) { // single bit different!
                        int x=vertex[i].x>>16;
                        int y=vertex[i].y>>16;
                        int z=vertex[i].z>>16;
                        x=x+((z*x)>>9)+offsetx;
                        y=y+((z*y)>>9)+offsety;
                        int x1=vertex[j].x>>16;
                        int y1=vertex[j].y>>16;
                        int z1=vertex[j].z>>16;
                        x1=x1+((z1*x1)>>9)+offsetx;
                        y1=y1+((z1*y1)>>9)+offsety;
                        draw_line(x,y,x1,y1,rgbToColour(255,255,255));
                    }
                }
            }
            */
           /*
            draw_quad_3d(vertex[0],vertex[1],vertex[3],vertex[2],0xc8,0x22,0xc8);
            draw_quad_3d(vertex[4],vertex[6],vertex[7],vertex[5],0x22,0xc8,0x22);
            draw_quad_3d(vertex[6],vertex[2],vertex[3],vertex[7],0x22,0x22,0xc8);
            draw_quad_3d(vertex[0],vertex[4],vertex[5],vertex[1],0xff,0x00,0x00);
            draw_quad_3d(vertex[0],vertex[2],vertex[6],vertex[4],0xc8,0x22,0x22);
            draw_quad_3d(vertex[5],vertex[7],vertex[3],vertex[1],0x00,0x00,0xff);
*/


           // draw_quad_3d(vertex[0],vertex[4],vertex[5],vertex[2],rgbToColour(0x88,0x44,0x22));
           // draw_quad_3d(vertex[0],vertex[1],vertex[3],vertex[2],rgbToColour(0x88,0x44,0x22));


            extern image_header  bubble;
            draw_image(&bubble,bx,by);
            bx+=vbx;
            by+=vby;
            if(bx<bubble.width/2 || bx+bubble.width/2>display_width) {vbx=-vbx;bx+=vbx;}
            if(by<bubble.height/2 || by+bubble.height/2>display_height) {vby=-vby;by+=vby;}

            setFontColour(0, 0, 0);
            print_xy("Demo Menu", 10, 10);
            setFontColour(255, 255, 255);
            setFont(FONT_UBUNTU16);
            print_xy("Life",10,LASTY+21);
            print_xy("Image Wave",10,LASTY+18);
            print_xy("Spaceship",10,LASTY+18);
            print_xy("Sensors",10,LASTY+18);
            if(get_orientation())
                print_xy("Landscape",10,LASTY+18);
            else
                print_xy("Portrait",10,LASTY+18);
            send_frame();
            // rotate cube
            for(int i=0;i<8;i++) {
                int x=vertex[i].x;
                int y=vertex[i].y;
                int z=vertex[i].z;
                x=x+(y>>6);
                y=y-(vertex[i].x>>6);
                y=y+(vertex[i].z>>7);
                z=z-(vertex[i].y>>7);
                x=x+(vertex[i].z>>8);
                z=z-(vertex[i].x>>8);
                vertex[i].x=x;
                vertex[i].y=y;
                vertex[i].z=z;
            }
            wait_frame();
            current_time = esp_timer_get_time();
            if ((frame % 10) == 0) {

                printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
                vTaskDelay(1);
            }
            #ifdef SHOW_PADS
               for (int i = 0; i <4; i++) {
                    uint16_t touch_value;
                    touch_pad_read(TOUCH_PADS[i], &touch_value);
                    printf("T%d:[%4d] ", i, touch_value);
                }
                printf("\n");
            #endif
            last_time = current_time;
            int key=get_input();
            if(key==0) select=(select+1)%5;
            if(key==35) return select;
        }
    }
}


void app_main() {
    // queue for button presses
    inputQueue = xQueueCreate(4,4);
    ESP_ERROR_CHECK(nvs_flash_init());
    // ===== Set time zone ======
    setenv("TZ", "	NZST-12", 0);
    tzset();
    // ==========================
    time(&time_now);
    tm_info = localtime(&time_now);
    // interrupts for button presses
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_intr_type(0, GPIO_INTR_LOW_LEVEL);
    gpio_set_intr_type(35, GPIO_INTR_LOW_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(0, gpio_isr_handler, (void*) 0);
    gpio_isr_handler_add(35, gpio_isr_handler, (void*) 35);

#ifdef SHOW_PADS
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for (int i = 0;i< 4;i++) {
        touch_pad_config(TOUCH_PADS[i], 0);
    }
#endif
    graphics_init();
    cls(0);

#if USE_WIFI

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (tm_info->tm_year < (2016 - 1900)) {
        ESP_LOGI(tag,
                 "Time is not set yet. Connecting to WiFi and getting time "
                 "over NTP.");
        setFontColour(0, 200, 200);
        setFont(FONT_UBUNTU16);
        print_xy("Time is not set yet", CENTER, CENTER);
        print_xy("Connecting to WiFi", CENTER, LASTY + getFontHeight() + 2);
        print_xy("Getting time over NTP", CENTER, LASTY + getFontHeight() + 2);
        setFontColour(200, 200, 0);
        print_xy("Wait", CENTER, LASTY + getFontHeight() + 2);
        flip_frame();
        if (obtain_time()) {
            cls(0);
            setFontColour(0, 200, 0);
            print_xy("System time is set.", CENTER, LASTY);
            flip_frame();
        } else {
            cls(0);
            setFontColour(200, 0, 0);
            print_xy("ERROR.", CENTER, LASTY);
            flip_frame();
        }
        time(&time_now);
        vTaskDelay(200);
        //	update_header(NULL, "");
        //	Wait(-2000);
    }
#endif
    // Initialize the effect displayed
    if (DISPLAY_IMAGE_WAVE) image_wave_init(COPY_IMAGE_TO_RAM);
    int sel=0;
    while(1) {
        sel=demo_menu(sel);
        switch(sel) {
            case 0:
                life();
                break;
            case 1:
                display();
                break;
            case 2:
                graphics_demo();
                break;
            case 3:
                sensors_demo();
                break;
            case 4:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
