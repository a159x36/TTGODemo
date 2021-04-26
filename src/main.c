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
#include "graphics3d.h"

#define PAD_START 3
#define PAD_END 5

#define SHOW_PADS
/*
 This code has been modified from the espressif spi_master demo code
 it displays some demo graphics on the 240x135 LCD on a TTGO T-Display board.
*/

const int TOUCH_PADS[4]={2,3,8,9};

// for button inputs
QueueHandle_t inputQueue;
uint64_t lastkeytime=0;
int emulator=0;

extern image_header  bubble;

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
    if(timesince>500) {
        int v=gpio_num+val*100;
        xQueueSendFromISR(inputQueue,&v,0);
        lastkeytime=time;
    }
    pval[gpio_index]=val;
    
    gpio_set_intr_type(gpio_num,val==0?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);

}

// get a button press, returns -1 if no button has been pressed
// otherwise the gpio of the button.
key_type get_input() {
    int key;
    if(xQueueReceive(inputQueue,&key,0)==pdFALSE)
        return NO_KEY;
    switch(key) {
        case 0: return LEFT_DOWN;
        case 35: return RIGHT_DOWN;
        case 100: return LEFT_UP;
        case 135: return RIGHT_UP;
    }
    return NO_KEY;
}


// menu with a rotating teapot, because... why not.
int demo_menu(char * title, int nentries, char *entries[], int select) {
    // for fps calculation
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
    int bx=83,by=57;
    int vbx=1,vby=1;
    vec3f rotation={PI/2-0.2,0,0};
    float teapot_size=20;
    colourtype diffuse={20,220,40};
    vec2 pos;
    int frame=0;
    while(1) {
        cls(rgbToColour(100,20,20));
        setFont(FONT_DEJAVU18);
        setFontColour(255, 255, 255);
        draw_rectangle(0,3,display_width,24,rgbToColour(220,220,0));
        draw_rectangle(0,select*18+24+3,display_width,18,rgbToColour(0,180,180));
        pos=(vec2){display_width/2,display_height/2};
        if(get_orientation()) pos.y+=display_height/4;
        else pos.x+=display_width/4;
        draw_image(&bubble,bx,by);
        bx+=vbx;
        by+=vby;
        if(bx<bubble.width/2 || bx+bubble.width/2>display_width) {vbx=-vbx;bx+=vbx;}
        if(by<bubble.height/2 || by+bubble.height/2>display_height) {vby=-vby;by+=vby;}
        draw_teapot(pos,teapot_size,rotation,diffuse);
        rotation=add3d(rotation,(vec3f){0.011,0.019,0.017});
        if(rotation.x>2*PI) rotation.x-=2*PI;
        if(rotation.y>2*PI) rotation.y-=2*PI;
        if(rotation.z>2*PI) rotation.z-=2*PI;
        setFontColour(0, 0, 0);
        print_xy(title, 10, 8);
        setFontColour(255, 255, 255);
        setFont(FONT_UBUNTU16);
        for(int i=0;i<nentries;i++) {
            print_xy(entries[i],10,LASTY+((i==0)?21:18));
        }
        #ifdef SHOW_PADS
        for (int i = 0; i <4; i++) {
            uint16_t touch_value;
            touch_pad_read(TOUCH_PADS[i], &touch_value);
            if(touch_value<1000) {
                int x=(i%2*120);
                int y=i>1?0:130;
                if(get_orientation())  
                    draw_rectangle(130-y,x,5,120,rgbToColour(200,200,255));
                else 
                    draw_rectangle(x,y,120,5,rgbToColour(200,200,255)); 
            }
        }
        #endif
        flip_frame();
        current_time = esp_timer_get_time();
        if ((frame++ % 10) == 0) {
            printf("FPS:%f %d %d\n", 1.0e6 / (current_time - last_time),
                heap_caps_get_free_size(MALLOC_CAP_DMA),
                heap_caps_get_free_size(MALLOC_CAP_32BIT));
            vTaskDelay(1);
        }
        last_time = current_time;
        key_type key=get_input();
        if(key==LEFT_DOWN) select=(select+1)%nentries;
        if(key==RIGHT_DOWN) return select;
    }
}


void app_main() {
    // queue for button presses
    inputQueue = xQueueCreate(4,4);
    // Initialize NVS
    /*
    esp_err_t err = nvs_flash_init();
    printf("nvs_flash_init:%d\n",err);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    ESP_ERROR_CHECK( err );
    */
    // ===== Set time zone ======
    setenv("TZ", "	NZST-12", 0);
    tzset();
    uint32_t user2=*((uint32_t *)0x3FF64024);
    printf("user2=%x\n",user2);
    if(user2!=0x70000000) emulator=1; 
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
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for (int i = 0;i< 4;i++) {
        touch_pad_config(TOUCH_PADS[i], 0);
    }
#endif
    graphics_init();
    cls(0);
/*
#if USE_WIFI

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (tm_info->tm_year < (2016 - 1900) && !emulator) {
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
*/
    // Initialize the effect displayed
    if (DISPLAY_IMAGE_WAVE) image_wave_init();
    int sel=0;
    while(1) {
        char *entries[]={"Life","Image Wave",emulator?"Spaceship":"Wifi Scan",
                        "Teapots","Bubble Game",
                        get_orientation()?"Landscape":"Portrait"};
        sel=demo_menu("Demo",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                life_demo();
                break;
            case 1:
                image_wave_demo();
                break;
            case 2:
                if(emulator) spaceship_demo();
                else wifi_scan();
                break;
            case 3:
                teapots_demo();
                break;
            case 4:
                bubble_demo();
                break;
            case 5:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
