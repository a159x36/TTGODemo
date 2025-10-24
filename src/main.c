/* TTGO Demo example for 159236

*/
#include <driver/gpio.h>
#include <driver/i2c.h>
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

static void wifi_settings_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Choose AP","SSID","Username",
                         "Password", "Back"};
        sel=demo_menu("Wifi Menu",ARRAY_LENGTH(entries),entries,sel);
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

static void led_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"MQTT", "Circles", "Numbers",  "Cube", "Accelerometer", "Back"};
        sel=demo_menu("Leds/Accel Menu",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                mqtt_leds();
                break;
            case 1:
                led_circles();
                break;
            case 2:
                led_numbers();
                break;
            case 3:
                led_cube();
                break;
            case 4:
                accel_demo();
                break;
            case 5:
                return;
        }
    }
}

static void wifi_menu() {
    int sel=0;
    while(1) {
        int connected=wifi_connected();
        char *entries[]={"Scan",connected?"Disconnect":"Connect","Access Point",
                         "Settings", "Back"};
        sel=demo_menu("Wifi Menu",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan(0);
                break;
            case 1:
                if(connected)
                    wifi_disconnect();
                else 
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
static void graphics_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Fonts","Image Wave", "Spaceship", "Teapots","Back"};
        sel=demo_menu("Graphics Menu",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                fonts_demo();            
                break;
            case 1:
                image_wave_demo();
                break;
            case 2:
                fonts_demo();
                break;
            case 3:
                teapots_demo();
                break;
            case 4:
                return;
        }
    }
}

static void pwm_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"LEDC Backlight","LEDC Servo","MCPWM Servo","GPIO Backlight","GPIO Servo","Back"};
        sel=demo_menu("PWM Menu",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                ledc_backlight_demo();
                break;
            case 1:
                ledc_servo_demo();
                break;
            case 2:
                mcpwm_demo();
                break;
            case 3:
                gpio_backlight_demo();
                break;
            case 4:
                gpio_servo_demo();
                break;
            case 5:
                return;
        }
    }
}
static void network_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Wifi","MQTT","Time","Web Server","Web Client","Back"};
        sel=demo_menu("Network Menu",ARRAY_LENGTH(entries),entries,sel);
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
                break;
            case 5:
                return;
        }
    }
}

static void games_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Bubble Game","Life","Boids","Back"};
        sel=demo_menu("Game Menu",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                bubble_demo();
                break;
            case 1:
                life_demo();
                break;
            case 2:
                boids_demo();
                break;
            case 3:
                return;
        }
    }
}


void app_main() {
    // initialise button handling
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
    // ===== Set time zone to NZ time using custom daylight savings rule======
    // if you are anywhere else in the world, this will need to be changed
    setenv("TZ", "NZST-12:00:00NZDT-13:00:00,M9.5.0,M4.1.0", 0);
    tzset();
    // initialise graphics and lcd display
    graphics_init();
    cls(0);
    // Initialize the image wave
    if (DISPLAY_IMAGE_WAVE) image_wave_init();
    // main menu
    int sel=0;
    while(1) {
        char *entries[]={"Graphics","Networking","Leds/Accel",
                        "PWM","Games",
                        get_orientation()?"Landscape":"Portrait"};
        sel=demo_menu("Demo",ARRAY_LENGTH(entries),entries,sel);
        switch(sel) {
            case 0:
                graphics_menu();
                break;
            case 1:
                network_menu();
                break;
            case 2:
                led_menu();
                break;
            case 3:
                pwm_menu();
                break;
            case 4:
                games_menu();
                break;
            case 5:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
