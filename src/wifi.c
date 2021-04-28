

#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <driver/gpio.h>
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
#include "esp_smartconfig.h"
#include "demos.h"
#include "lwip/sockets.h"

#if USE_WIFI
static EventGroupHandle_t wifi_event_group;
typedef enum {
    SCAN,
    STATION,
    ACCESS_POINT,
} wifi_mode_type;

wifi_mode_type wifi_mode=0;

//#define DEFAULT_SCAN_LIST_SIZE 16
#define TAG "Wifi"
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;
char wifi_event[64];
void client_task(void *pvParameters);
TaskHandle_t ctask=NULL;
static void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    ESP_LOGI(tag, "WiFi event %s %d\n",event_base,event_id);
    snprintf(wifi_event,64,"WiFi event %s %d\n",event_base,event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if(wifi_mode==STATION)
            esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && ((event_id == IP_EVENT_STA_GOT_IP) || 
    (event_id == IP_EVENT_AP_STAIPASSIGNED))) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        if(wifi_mode==STATION) xTaskCreate(client_task,"ct",2048,NULL,1,&ctask);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        esp_wifi_scan_start(NULL,false);
    }
}
 
//-------------------------------
static esp_netif_t *sta_netif = NULL;
#define DEFAULT_SCAN_LIST_SIZE 24

int received=1;
int communicating=0;

void send_receive(int sock) {
    int err=fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    printf("Send_rec %d\n",err);
    int gpio=1;
    communicating=1;
    while (communicating) {
        char rx;
        int len = recv(sock, &rx, 1, 0);
        if(len!=-1)
            printf("Received %d %d\n",len,rx);
        if(len==-1 && errno!=EWOULDBLOCK) {
            printf("Error: %d\n",errno);
            break;
        }
        if(gpio_get_level(0)!=gpio) {
            gpio=gpio_get_level(0);
            len=send(sock,&gpio,1,0);
            printf("Sent %d\n",gpio);
        }
        received=rx;
        vTaskDelay(1);
    }
    communicating=0;
    printf("End Send Receive\n");
}

void server_task(void *pvParameters) {
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(80);
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(listen_sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    listen(listen_sock, 1);
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    send_receive(sock);
    close(sock);
    close(listen_sock);
    vTaskDelete(NULL);
}

void client_task(void *pvParameters) {
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = inet_addr("192.168.4.1");
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(80);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    connect(sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    send_receive(sock);
    close(sock);
    vTaskDelete(NULL);
    ctask=NULL;
}

void init_wifi(wifi_mode_type mode) {
    wifi_mode=mode;
    if(sta_netif!=NULL) {
        esp_event_loop_delete_default();
        esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
        esp_netif_destroy(sta_netif);
    } else
        esp_netif_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(mode==ACCESS_POINT)
        sta_netif = esp_netif_create_default_wifi_ap();
    else
        sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    uint8_t protocol=WIFI_PROTOCOL_LR;//(WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR);
    if(mode==ACCESS_POINT) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        esp_wifi_set_protocol(ESP_IF_WIFI_AP,protocol);
    }
    else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        esp_wifi_set_protocol(ESP_IF_WIFI_STA,protocol);
    }
}

int ap_cmp(const void *ap1, const void *ap2) {
    wifi_ap_record_t *a1=(wifi_ap_record_t *)ap1;
    wifi_ap_record_t *a2=(wifi_ap_record_t *)ap2;
    int n=strcmp((char *)a1->ssid, (char *)a2->ssid);
    if(n==0) n= a1->primary - a2->primary;
    if(n==0) n= a2->rssi - a1->rssi;
    return n;
}

#define EXAMPLE_ESP_WIFI_SSID "ESP32 AP"
wifi_sta_list_t wifi_stations;

void print_ap_info(wifi_ap_record_t *ap) {
    setFont(FONT_SMALL);
    char rssi_str[8];
    char channel_str[8];
    char mode_str[5];
    int nmodes=0;
    if(ap->phy_11b) mode_str[nmodes++]='b';
    if(ap->phy_11g) mode_str[nmodes++]='g';
    if(ap->phy_11n) mode_str[nmodes++]='n';
    if(ap->phy_lr) mode_str[nmodes++]='l';
    mode_str[nmodes++]=0;
    snprintf(rssi_str,sizeof(rssi_str),"%d",ap->rssi);
    snprintf(channel_str,sizeof(channel_str),"%d",ap->primary);
    switch(ap->authmode) {
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

void wifi_ap(void) {
    cls(0);
    char station_info[32];
    init_wifi(ACCESS_POINT);
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = 13,
            .password = "",
            .max_connection = 8,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    TaskHandle_t stask;
    xTaskCreate(server_task,"st",2048,NULL,1,&stask);
    do {
        if(received==1) 
            cls(rgbToColour(128,0,0));
        else
            cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(220,220,0));
        print_xy("Access Point",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        print_xy("Last Wifi Event:",5,LASTY+16);
        print_xy(wifi_event,5,LASTY+16);
        setFontColour(0,255,0);
        if(communicating)
            print_xy("Communicating",5,LASTY+16);
        setFont(FONT_SMALL);
        esp_wifi_ap_get_sta_list(&wifi_stations);
        for(int i=0;i<wifi_stations.num;i++) {
            uint8_t *mac=wifi_stations.sta[i].mac;
            snprintf(station_info,32,"%02x:%02x:%02x:%02x:%02x:%02x %d",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], wifi_stations.sta[i].rssi); 
            print_xy(station_info,5,LASTY+16);
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    communicating=0;
    esp_wifi_stop();
}

void wifi_connect(void) {
    cls(0);
    init_wifi(STATION);
    wifi_config_t wifi_config = { .sta ={.ssid = EXAMPLE_ESP_WIFI_SSID,.password = WIFI_PASSWORD}}; 
    wifi_event[0]=0;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    do {
        if(received==1) 
            cls(rgbToColour(128,0,0));
        else
            cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Wifi Station",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        print_xy("Last Wifi Event:",5,LASTY+16);
        print_xy(wifi_event,5,LASTY+16);
        setFontColour(0,255,0);
        if(communicating)
            print_xy("Communicating",5,LASTY+16);
        if(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            wifi_ap_record_t ap;
            print_xy("Connected",5,LASTY+16);
            print_xy("",5,LASTY+6);
            esp_wifi_sta_get_ap_info(&ap);
            print_ap_info(&ap);
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    communicating=0;
    esp_wifi_stop();
    
}

void wifi_scan(void) {
    cls(0);
    init_wifi(SCAN);
    ESP_ERROR_CHECK(esp_wifi_start());
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    static wifi_ap_record_t ap_list[DEFAULT_SCAN_LIST_SIZE];
   // uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    setFont(FONT_UBUNTU16);
    setFontColour(255,255,255);
    print_xy("Scanning...",5,3);
    flip_frame();
    esp_wifi_scan_start(NULL, false);
    int ap_number=0;
    int j;
    do {
        cls(0);
        number=DEFAULT_SCAN_LIST_SIZE;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        for(int i=0;i<number;i++) {
            uint8_t *ssid=ap_info[i].ssid;
            uint8_t ch=ap_info[i].primary;
            for(j=0;j<ap_number;j++) {
                if(!strcmp((char *)ssid,(char *)ap_list[j].ssid) && ap_list[j].primary==ch) {
                    ap_list[j]=ap_info[i];
                    break;
                }
            }
            if(j==ap_number && ap_number<DEFAULT_SCAN_LIST_SIZE)
                ap_list[ap_number++]=ap_info[i];

        }
        qsort(ap_list,ap_number,sizeof(wifi_ap_record_t),ap_cmp);
        setFont(FONT_UBUNTU16);
        setFontColour(0,0,66);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Access Points\n",5,3);
        setFont(FONT_SMALL);
        print_xy("",5,LASTY+8);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_number); i++) {
            print_ap_info(ap_list+i);
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    esp_wifi_stop();
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