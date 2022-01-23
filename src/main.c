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

void wifi_settings_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Choose AP","SSID","Username",
                         "Password", "Back"};
        sel=demo_menu("Wifi Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan(1);
                break;
            case 1:
                edit_stored_string("ssid","SSID");
                break;
            case 2:
                edit_stored_string("username","Username");
                break;
            case 3:
                edit_stored_string("password","Password");
                break;
            case 4:
                return;
        }
    }
}
void wifi_menu() {
    int sel=0;
    if(is_emulator) {
        setFont(FONT_DEJAVU24);
        do {
            cls(0);
            print_xy("Wifi Not Available\nOn the Emulator",5,3);
            flip_frame();
        } while(get_input()!=RIGHT_DOWN);
        return;
    }
    while(1) {
        char *entries[]={"Scan","Connect","Access Point",
                         "Settings", "Back"};
        sel=demo_menu("Wifi Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan(0);
                break;
            case 1:
                wifi_connect(0);
                break;
            case 2:
                wifi_ap();
                break;
            case 3:
                wifi_settings_menu();
                break;
            case 4:
                return;
        }
    }
}
void graphics_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Boids", "Life","Image Wave", "Spaceship", get_orientation()?"Landscape":"Portrait","Back"};
        sel=demo_menu("Graphics Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                boids_demo();            
                break;
            case 1:
                life_demo();
                break;
            case 2:
                image_wave_demo();
                break;
            case 3:
                spaceship_demo();
                break;
            case 4:
                set_orientation(1-get_orientation());
                break;
            case 5:
                return;
        }
    }
}

void network_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Wifi","MQTT","Time","Web Server","Web Client", /*get_orientation()?"Landscape":"Portrait",*/"Back"};
        sel=demo_menu("Network Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_menu();
                break;
            case 1:
                mqtt();
                break;
            case 2:
                time_demo();
                break;
            case 3:
                webserver();
                break;
            case 4:
                web_client();
//                set_orientation(1-get_orientation());
                break;
            case 5:
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
    setenv("TZ", "NZST-12:00:00NZDT-13:00:00,M9.5.0,M4.1.0", 0);
    tzset();
    //uint32_t user2=*((uint32_t *)0x3FF64024);
    //if(user2!=0x70000000) is_emulator=1; 
    // ==========================
    time(&time_now);
    tm_info = localtime(&time_now);


    graphics_init();
    cls(0);
    // Initialize the effect displayed
    if (DISPLAY_IMAGE_WAVE) image_wave_init();
    int sel=0;
    while(1) {
        char *entries[]={"Graphics","Networking",
                        "Teapots","Bubble Game",
                        get_orientation()?"Landscape":"Portrait"};
        sel=demo_menu("Demo",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                graphics_menu();
                break;
            case 1:
                network_menu();
                break;
            case 2:
                teapots_demo();
                break;
            case 3:
                bubble_demo();
                break;
            case 4:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
