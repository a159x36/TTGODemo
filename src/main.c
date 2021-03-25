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
#include "teapot_data.h"

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


int offsetx;
int offsety;

inline void point3d_to_xy(vec3f *p, int16_t *xx, int16_t *yy) {
    int x=p->x;
    int y=p->y;
    x=x+((int)(p->z*p->x)>>8)+offsetx;
    y=y+((int)(p->z*p->y)>>8)+offsety;
    *xx=x;
    *yy=y;
}

void draw_line_3d(vec3f p0, vec3f p1, uint16_t colour) {
    int16_t x[2],y[2];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    draw_line(x[0],y[0],x[1],y[1], colour);
}

void draw_triangle_3d(vec3f p0, vec3f p1, vec3f p2, uint16_t colour) {
    int16_t x[3],y[3];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    point3d_to_xy(&p2,&x[2],&y[2]);
    draw_triangle(x[0],y[0],x[1],y[1],x[2],y[2], colour);
}

inline vec3f sub3d(vec3f p0, vec3f p1) {
    vec3f p={p0.x-p1.x, p0.y-p1.y, p0.z-p1.z};
    return p;
}

inline vec3f add3d(vec3f p0, vec3f p1) {
    vec3f p={p0.x+p1.x, p0.y+p1.y, p0.z+p1.z};
    return p;
}

inline vec3f mid3d(vec3f p0, vec3f p1) {
    vec3f p={(p0.x+p1.x)/2, (p0.y+p1.y)/2, (p0.z+p1.z)/2};
    return p;
}

inline vec3f cross3d(vec3f p0, vec3f p1) {
    vec3f p;
    p.x=(p0.y)*(p1.z)-(p0.z)*(p1.y);
    p.y=(p0.z)*(p1.x)-(p0.x)*(p1.z);
    p.z=(p0.x)*(p1.y)-(p0.y)*(p1.x);
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

inline vec3f normalise(vec3f p) {
    float mag=Q_rsqrt(p.x*p.x+p.y*p.y+p.z*p.z);
    vec3f p1={(p.x)*mag, (p.y)*mag, (p.z)*mag};
    return p1;
}

inline float dot(vec3f p0,vec3f p1) {
    return ((p0.x*p1.x))+((p0.y*p1.y))+((p0.z*p1.z));
}

inline int clamp(int x,int min,int max) {
    if(x<min) return min;
    if(x>max) return max;
    return x;
}

inline float clampf(float x,float min,float max) {
    if(x<min) return min;
    if(x>max) return max;
    return x;
}

vec3f lightpos={0,0,1.0};


float teapot_size=20;

float rmx[3][3];

#define PI 3.1415926
float alpha=PI/2-0.2;
float beta=0;
float gamm=0;

inline void maketrotationmatrix() {
    float ca=cos(alpha);
    float cb=cos(beta);
    float cc=cos(gamm);
    float sa=sin(alpha);
    float sb=sin(beta);
    float sc=sin(gamm);

    rmx[0][0]=cc*cb;
    rmx[0][1]=cc*sb*sa-sc*ca;
    rmx[0][2]=cc*sb*ca+sc*sa;
    rmx[1][0]=sc*cb;
    rmx[1][1]=sc*sb*sa+cc*ca;
    rmx[1][2]=sc*sb*ca-cc*sa;
    rmx[2][0]=-sb;
    rmx[2][1]=cb*sa;
    rmx[2][2]=cb*ca;
}
typedef struct {uint16_t r; uint16_t g; uint16_t b;} colourtype;
colourtype ambiant={64,0,64};
colourtype diffuse={20,220,40};


void draw_quad_3d(vec3f p0, vec3f p1, vec3f p2, vec3f p3) {

    vec3f normal=normalise(cross3d(sub3d(p2,p0),sub3d(p3,p0)));
    if(normal.z<=0) return;

    float light=clampf(dot(normal,lightpos),0,1.0);

    uint16_t colour=rgbToColour(clamp(((int)(diffuse.r*light))+ambiant.r,0,255),
    clamp((int)(diffuse.g*light)+ambiant.g,0,255),
    clamp((int)(diffuse.b*light)+ambiant.b,0,255));
    int16_t x[4],y[4];
    point3d_to_xy(&p0,&x[0],&y[0]);
    point3d_to_xy(&p1,&x[1],&y[1]);
    point3d_to_xy(&p2,&x[2],&y[2]);
    point3d_to_xy(&p3,&x[3],&y[3]);
    draw_triangle(x[0],y[0],x[1],y[1],x[2],y[2], colour);
    draw_triangle(x[2],y[2],x[3],y[3],x[0],y[0], colour);
//    draw_triangle_3d(p0, p1, p2, colour);
//   draw_triangle_3d(p2, p3, p0, colour);
}

void vrotate(vec3f *v1,vec3f v, float f0, float f1, float f2) {
    v.x=v.x*f0;
    v.y=v.y*f1;
    v.z=(v.z-2.0)*f2;
    v1->x=(rmx[0][0]*v.x+rmx[0][1]*v.y+rmx[0][2]*v.z);
    v1->y=(rmx[1][0]*v.x+rmx[1][1]*v.y+rmx[1][2]*v.z);
    v1->z=(rmx[2][0]*v.x+rmx[2][1]*v.y+rmx[2][2]*v.z);
}

void bezier(vec3f p[4][4], vec3f np[7][7]) {
    vec3f t[4][7];
    for(int i=0;i<4;i++) {
        t[i][0]=p[i][0];
        t[i][6]=p[i][3];
        vec3f m=mid3d(p[i][1],p[i][2]);
        t[i][1]=mid3d(p[i][0],p[i][1]);
        t[i][5]=mid3d(p[i][2],p[i][3]);
        t[i][2]=mid3d(t[i][1],m);
        t[i][4]=mid3d(t[i][5],m);
        t[i][3]=mid3d(t[i][2],t[i][4]);
    }
    for(int i=0;i<7;i++) {
        np[0][i]=t[0][i];
        np[6][i]=t[3][i];
        vec3f m=mid3d(t[1][i],t[2][i]);
        np[1][i]=mid3d(t[0][i],t[1][i]);
        np[5][i]=mid3d(t[2][i],t[3][i]);
        np[2][i]=mid3d(np[1][i],m);
        np[4][i]=mid3d(np[5][i],m);
        np[3][i]=mid3d(np[2][i],np[4][i]);
    }
}


void draw_teapot() {

    static vec3f np[7][7];
    static vec3f p[4][4];
    float z[32];
    int order[32];
    for(int i=0;i<32;i++) {
        order[i]=i;
        vec3f zv1,zv2;
        vrotate(&zv1,teapotVertices[teapotPatches[i][5]-1],teapot_size,teapot_size,teapot_size);
        vrotate(&zv2,teapotVertices[teapotPatches[i][10]-1],teapot_size,teapot_size,teapot_size);
        z[i]=mid3d(zv1,zv2).z;
    }
    for (int k = 2; k <= 32; k *= 2) 
        for (int j = k/2; j > 0; j /= 2) 
            for (int i = 0; i < 32; i++) {
                int l = (i ^ j);
                if (l > i)
                    if (  (((i & k) == 0) && (z[i] > z[l]))
                       || (((i & k) != 0) && (z[i] < z[l])) ) {
                           float t=z[i];
                           z[i]=z[l];
                           z[l]=t;
                           int ti=order[i];
                           order[i]=order[l];
                           order[l]=ti;
                       }
            }

    for(int i=0;i<32;i++) {
        int ii=order[i];
        
        for(int j=0;j<4;j++) {
            for(int k=0;k<4; k++) {
                vrotate(&p[j][k],teapotVertices[teapotPatches[ii][j*4+k]-1],teapot_size,teapot_size,teapot_size);
            }
        }
        bezier(p,np);
        
        if(np[0][0].z<np[6][6].z) {
            for(int j=1;j<7;j++) {
                for(int k=1;k<7;k++) {
                    draw_quad_3d(np[j-1][k-1],np[j-1][k],np[j][k],np[j][k-1]);
                }
            }
        } else {
            for(int j=6;j>0;j--) {
                for(int k=6;k>0;k--) {
                    draw_quad_3d(np[j-1][k-1],np[j-1][k],np[j][k],np[j][k-1]);
                }
            }
        }
    }
}

void teapots_demo() {
    int nteapots=10;
    float a[nteapots],b[nteapots];
    float x[nteapots],y[nteapots];
    int s[nteapots];
    int64_t current_time=0, last_time=0;
    int frame=0;
    colourtype col[nteapots];
    for(int i=0;i<nteapots; i++) {
        a[i]=(rand()%31415)/5000.0;
        b[i]=(rand()%31415)/5000.0;
        x[i]=rand()%(display_width-20)+10;
        y[i]=rand()%(display_height-20)+10;
        col[i].r=rand()%200+55;//50*(i&4);
        col[i].g=rand()%200+55;//100*(i&2);
        col[i].b=rand()%200+55;//200*(i&1);
        s[i]=rand()%10+10;
        //if((i&7)==0) col[i].g=100;
    }
    while(1) {
        cls(0);
        for(int i=0;i<nteapots; i++) {
            diffuse=col[i];
            //alpha=0;
            beta=a[i];
            gamm=b[i];
            offsetx=x[i];
            offsety=y[i];
            teapot_size=s[i];
            maketrotationmatrix();
            draw_teapot();
            a[i]+=0.05;
            if(a[i]>2*PI) a[i]=a[i]-2*PI;
            b[i]+=0.07;
        }
        flip_frame();
        current_time = esp_timer_get_time();
        if ((frame++ % 10) == 0) {
            printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
            vTaskDelay(1);
        }
        last_time=current_time;
        teapot_size=20;
        int key=get_input();
        if(key==0) return;
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
    /*
        vec3 vertex[8]={
            {-SZ,-SZ,-SZ},{-SZ,-SZ,SZ},{-SZ,SZ,-SZ},{-SZ,SZ,SZ},
            {SZ,-SZ,-SZ},{SZ,-SZ,SZ},{SZ,SZ,-SZ},{SZ,SZ,SZ}
        };
        */
        for(int frame=0;frame < 4000; frame++) {
            maketrotationmatrix();
            alpha+=0.01;
            beta+=0.02;
            if(beta>2*PI) beta=beta-2*PI;
            gamm+=0.017;
            cls(rgbToColour(100,20,20));
            setFont(FONT_DEJAVU18);
            setFontColour(255, 255, 255);
            draw_rectangle(0,5,display_width,24,rgbToColour(220,220,0));
            draw_rectangle(0,select*18+24+5,display_width,18,rgbToColour(0,180,180));
            offsetx=display_width/2;
            offsety=display_height/2;
            
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
            draw_teapot();
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
            print_xy("Teapots",10,LASTY+18);
            if(get_orientation())
                print_xy("Landscape",10,LASTY+18);
            else
                print_xy("Portrait",10,LASTY+18);
            send_frame();
            /*
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
            */
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
                teapots_demo();
                break;
            case 4:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
