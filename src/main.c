/* TTGO Demo example for 159236

*/
#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"
#include "image_wave.h"

// put your wifi ssid name and password in here
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define USE_WIFI 0
#define DISPLAY_VOLTAGE 1
#define DISPLAY_IMAGE_WAVE 1
// this makes it faster but uses so much extra memory
// that you can't use wifi at the same time
#define COPY_IMAGE_TO_RAM 1
/*
 This code has been modified from the espressif spi_master demo code
 it displays some demo graphics on the 240x135 LCD on a TTGO T-Display board. 
*/

// voltage reference calibration for Battery ADC
uint32_t vref;

const char *tag = "T Display";
static time_t time_now;
static struct tm *tm_info;

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
    //ets_printf("gpio_isr_handler %d %d %lld\n",gpio_num,val, timesince);
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


#define SZ (20<<16)
// menu with a rotating cube, because... why not.
int demo_menu(int select) {
    // for fps calculation
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
    while(1) {
    // cube vertices
        struct { int x; int y; int z;} vertex[8]={
            {-SZ,-SZ,-SZ},{-SZ,-SZ,SZ},{-SZ,SZ,-SZ},{-SZ,SZ,SZ},
            {SZ,-SZ,-SZ},{SZ,-SZ,SZ},{SZ,SZ,-SZ},{SZ,SZ,SZ}
        };
        for(int frame=0;frame < 4000; frame++) {
            cls(rgbToColour(100,20,20));
            setFont(FONT_DEJAVU18);
            setFontColour(255, 255, 255);
            draw_rectangle(0,5,display_width,24,rgbToColour(220,220,0));
            draw_rectangle(0,select*24+24+5,display_width,24,rgbToColour(0,180,180));
            int offsetx=display_width/2;
            int offsety=display_height/2;
            if(get_orientation()) offsety+=display_height/4;
            else offsetx+=display_width/4;
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
            print_xy("Demo Menu", 10, 10);
            print_xy("Life",10,LASTY+24);
            print_xy("Image Wave",10,LASTY+24);
            print_xy("Spaceship",10,LASTY+24);
            if(get_orientation())
                print_xy("Landscape",10,LASTY+24);
            else
                print_xy("Portrait",10,LASTY+24);
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
            last_time = current_time;
            int key=get_input();
            if(key==0) select=(select+1)%4;
            if(key==35) return select;
        }
    }
}
// waving image demo showing time
static void display() {
    char buff[128];
    int frame = 0;
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
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
#if USE_WIFI
static EventGroupHandle_t wifi_event_group;
#define DEFAULT_SCAN_LIST_SIZE 16
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    //    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    //    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    //    uint16_t ap_count = 0;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(tag, "Connected:%s", event->event_info.connected.ssid);
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

//-------------------------------

static void initialise_wifi(void) {
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = WIFI_SSID,
                .password = WIFI_PASSWORD,
            },
    }; 
    ESP_LOGI(tag, "Setting WiFi configuration SSID %s...",
             wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*
    esp_wifi_scan_start(NULL,true);
    #define DEFAULT_SCAN_LIST_SIZE 16
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_records(&number, ap_info);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(tag, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(tag, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(tag, "Channel \t\t%d\n", ap_info[i].primary);
    }*/
}

//-------------------------------
static void initialize_sntp(void) {
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

//--------------------------
static int obtain_time(void) {
    static char tmp_buff[64];
    int res = 1;
    ESP_LOGI(tag, "Wifi Init");
    initialise_wifi();
    ESP_LOGI(tag, "Wifi Initialised");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
                        portMAX_DELAY);

    initialize_sntp();
    ESP_LOGI(tag, "SNTP Initialised");

    // wait for time to be set
    int retry = 0;
    const int retry_count = 20;

    time(&time_now);
    tm_info = localtime(&time_now);

    while (tm_info->tm_year < (2016 - 1900) && ++retry < retry_count) {
        // ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry,
        // retry_count);
        sprintf(tmp_buff, "Wait %0d/%d", retry, retry_count);
        cls(0);
        print_xy(tmp_buff, CENTER, LASTY);
        flip_frame();
        vTaskDelay(500 / portTICK_RATE_MS);
        time(&time_now);
        tm_info = localtime(&time_now);
    }
    if (tm_info->tm_year < (2016 - 1900)) {
        ESP_LOGI(tag, "System time NOT set.");
        res = 0;
    } else {
        ESP_LOGI(tag, "System time is set.");
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    return res;
}
#endif

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
                set_orientation(1-get_orientation());
                break;
        }
    }
}
