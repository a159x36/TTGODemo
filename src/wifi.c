

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
#include <fonts.h>
#include <graphics.h>
#include <time.h>

#include "demos.h"

#if USE_WIFI
static EventGroupHandle_t wifi_event_group;
//#define DEFAULT_SCAN_LIST_SIZE 16
#define TAG "Wifi"
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;
static void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}
 
//-------------------------------
static esp_netif_t *sta_netif = NULL;
#define DEFAULT_SCAN_LIST_SIZE 24
void initialise_wifi(void) {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    esp_netif_init();
    wifi_event_group = xEventGroupCreate();
    //ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
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
 //   ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "AUTH \t\t%d %d %d",ap_info[i].authmode,ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
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

void init_wifi() {
    esp_netif_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    /*
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = WIFI_SSID,
                .password = WIFI_PASSWORD,
            },
    }; 
    ESP_LOGI(tag, "Setting WiFi configuration SSID %s...",
             wifi_config.sta.ssid);
             */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
 //   ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_scan(void) {
    cls(0);
    if(sta_netif==NULL) init_wifi();
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
   // uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    setFont(FONT_UBUNTU16);
    setFontColour(255,255,255);
    print_xy("Scanning...",5,3);
    flip_frame();
    do {
        cls(0);
        esp_wifi_scan_start(NULL, true);
        number=DEFAULT_SCAN_LIST_SIZE;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        setFont(FONT_UBUNTU16);
        setFontColour(0,0,66);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Access Points\n",5,3);
        setFont(FONT_SMALL);
        print_xy("",5,LASTY+8);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < number); i++) {
            char rssi_str[8];
            char channel_str[8];
            char mode_str[5]="    ";
            wifi_ap_record_t *ap=ap_info+i;
            int nmodes=0;
            if(ap->phy_11b) mode_str[nmodes++]='b';
            if(ap->phy_11g) mode_str[nmodes++]='g';
            if(ap->phy_11n) mode_str[nmodes++]='n';
            if(ap->phy_lr) mode_str[nmodes++]='l';
            mode_str[nmodes++]=0;
            snprintf(rssi_str,sizeof(rssi_str),"%d",ap->rssi);
            snprintf(channel_str,sizeof(channel_str),"%d",ap->primary);
            switch(ap_info->authmode) {
                case WIFI_AUTH_OPEN: setFontColour(0,255,0); break;
                case WIFI_AUTH_WEP: setFontColour(128,128,0); break;
                case WIFI_AUTH_WPA_PSK: setFontColour(255,128,0); break;
                case WIFI_AUTH_WPA2_PSK: setFontColour(255,0,0); break;
                default:
                    setFontColour(128,128,128); break;
            }
            print_xy(rssi_str,1,LASTY+10);
            setFontColour(128,255,255);
            print_xy(mode_str,25,LASTY);
            setFontColour(255,0,255);
            print_xy(channel_str,48,LASTY);
            setFontColour(255,255,255);
            print_xy((char *)(ap->ssid),65,LASTY);
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
}

//-------------------------------
static void initialize_sntp(void) {
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

//--------------------------
int obtain_time(void) {
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