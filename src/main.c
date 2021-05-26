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


#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"
#include "image_wave.h"
#include "demos.h"
#include "graphics3d.h"
#include "input_output.h"

#define PAD_START 3
#define PAD_END 5

#define SHOW_PADS
/*
 This code has been modified from the espressif spi_master demo code
 it displays some demo graphics on the 240x135 LCD on a TTGO T-Display board.
*/

int is_emulator=0;


void wifi_menu() {
        int sel=0;
    while(1) {
        char *entries[]={"Scan","Connect","Access Point","MQTT","Time",
                       // get_orientation()?"Landscape":"Portrait",
                         "Web Server", "Back"};
        sel=demo_menu("Wifi Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan();
                break;
            case 1:
                wifi_connect();
                break;
            case 2:
                wifi_ap();
                break;
            case 3:
                mqtt();
                //set_orientation(1-get_orientation());
                break;
            case 4:
                time_demo();
                break;
            case 5:
                webserver();
                break;
            case 6:
                return;
        }
    }
}

void app_main() {

    input_output_init();
   
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    // ===== Set time zone to NZST======
    setenv("TZ", "	NZST-12", 0);
    tzset();
    uint32_t user2=*((uint32_t *)0x3FF64024);
    if(user2!=0x70000000) is_emulator=1; 
    // ==========================
    time(&time_now);
    tm_info = localtime(&time_now);


    graphics_init();
    cls(0);
    // Initialize the effect displayed
    if (DISPLAY_IMAGE_WAVE) image_wave_init();
    int sel=0;
    while(1) {
        char *entries[]={"Life","Image Wave","Networking",
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
//                if(emulator) spaceship_demo();
//                else wifi_menu();
                wifi_menu();
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
