

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
#include <esp_http_server.h>
#include "esp_eth.h"
#include "mqtt_client.h"
#include <driver/touch_pad.h>
#include "esp_wpa2.h"

#include "graphics3d.h"
#include "input_output.h"
#include "networking.h"



wifi_mode_type wifi_mode=0;

#define TAG "Wifi"


//void client_task(void *pvParameters);
//TaskHandle_t ctask=NULL;

 
#define DEFAULT_SCAN_LIST_SIZE 24

//int received=1;
//int communicating=0;


/*
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
        if(len==1)
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
    while(1) {
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if(sock<0) break;
        send_receive(sock);
        close(sock);
    }
    communicating=0;
    close(listen_sock);
    vTaskDelete(NULL);
}

void client_task(void *pvParameters) {
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(demo_netif,&ip_info);
    printf("Client %x %x\n",ip_info.gw.addr, ip_info.ip.addr);
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = ip_info.gw.addr;//inet_addr("192.168.4.1");//ip_info.gw.addr;//("192.168.4.1");
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(80);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    connect(sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    send_receive(sock);
    close(sock);
    vTaskDelete(NULL);
    ctask=NULL;
}
*/





void init_wifi(wifi_mode_type mode) {
   
    if(is_emulator) {
        init_eth();
        return;
    }
    network_event_group = xEventGroupCreate();
    wifi_mode=mode;
    if(network_interface!=NULL) {
        esp_event_loop_delete_default();
        esp_wifi_clear_default_wifi_driver_and_handlers(network_interface);
        esp_netif_destroy(network_interface);
    } else
        esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if(mode==ACCESS_POINT)
        network_interface = esp_netif_create_default_wifi_ap();
    else
        network_interface = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    uint8_t protocol=(WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N);//|WIFI_PROTOCOL_LR);
    if(mode==ACCESS_POINT) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        esp_wifi_set_protocol(ESP_IF_WIFI_AP,protocol);
        #define SSID "ESP32"
        wifi_config_t wifi_config = { .ap = {
                .ssid = SSID,
                .ssid_len = strlen(SSID),
                .channel = 13,
                .password = WIFI_PASSWORD,
                .max_connection = 8,
                .authmode = WIFI_AUTH_OPEN
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));        
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        esp_wifi_set_protocol(ESP_IF_WIFI_STA,protocol);
        char ssid[32];
        storage_read_string("ssid","",ssid,sizeof(ssid));
        char password[64];
        storage_read_string("password","", password, sizeof(password));
        char username[64];
        storage_read_string("username","", username, sizeof(username));
        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid,ssid,sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password,password,sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        if(strlen(username)!=0) {
            ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username)) );
            ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password)) );
            ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_enable() );
        }
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}

int ap_cmp(const void *ap1, const void *ap2) {
    wifi_ap_record_t *a1=(wifi_ap_record_t *)ap1;
    wifi_ap_record_t *a2=(wifi_ap_record_t *)ap2;
    int n=strcmp((char *)a1->ssid, (char *)a2->ssid);
    if(n==0) n= a1->primary - a2->primary;
    if(n==0) n= a2->rssi - a1->rssi;
    return n;
}



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
    print_xy(rssi_str,1,LASTY);
    setFontColour(128,255,255);
    print_xy(mode_str,25,LASTY);
    setFontColour(255,0,255);
    print_xy(channel_str,48,LASTY);
    setFontColour(255,255,255);
    print_xy((char *)(ap->ssid),65,LASTY);
    print_xy("",1,LASTY+10);
}

void wifi_ap(void) {
    cls(0);
    init_wifi(ACCESS_POINT);
    wifi_sta_list_t wifi_stations;

//    TaskHandle_t stask;
//    xTaskCreate(server_task,"st",2048,NULL,1,&stask);
    do {
        /*
        if(received==1) 
            cls(rgbToColour(128,0,0));
        else */
            cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(220,220,0));
        print_xy("Access Point\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        setFontColour(0,255,0);
//        if(communicating)
//            gprintf("Communicating\n");
        setFont(FONT_SMALL);
        esp_wifi_ap_get_sta_list(&wifi_stations);
        for(int i=0;i<wifi_stations.num;i++) {
            uint8_t *mac=wifi_stations.sta[i].mac;
            gprintf("%02x:%02x:%02x:%02x:%02x:%02x %d\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], wifi_stations.sta[i].rssi); 
        }
//        if(wifi_stations.num==0) communicating=0;
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
//    communicating=0;
//    vTaskDelete(stask);
//    esp_wifi_stop();
}


void wifi_connect(void) {
    cls(0);
    network_event[0]=0;
    init_wifi(STATION);

    do {
//        if(received==1) 
//            cls(rgbToColour(128,0,0));
//        else
            cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Wifi Station\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        setFontColour(0,255,0);
//        if(communicating)
//            gprintf("Communicating\n");
        if(xEventGroupGetBits(network_event_group) & CONNECTED_BIT) {
            wifi_ap_record_t ap;
            gprintf("Connected\n");
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(network_interface,&ip_info);
            gprintf(IPSTR"\n",IP2STR(&ip_info.ip));
            gprintf(IPSTR"\n",IP2STR(&ip_info.gw));
            esp_wifi_sta_get_ap_info(&ap);
            print_ap_info(&ap);
            
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
//    communicating=0;
//    esp_wifi_stop();
}

void edit_stored_string(char *name, char *prompt) {
    char val[64];
    storage_read_string(name,"",val,sizeof(val));
    get_string(prompt,val,sizeof(val));
    storage_write_string(name,val);
}

void edit_wifi_settings(int i) {
    if(i&2)
        edit_stored_string("username","Username");
    if(i&1)
        edit_stored_string("password","Password");
}
void wifi_scan(void) {
    cls(0);
    if(is_emulator) {
        setFont(FONT_UBUNTU16);
        do {
            cls(0);
            print_xy("Wifi Not Available",5,3);
            flip_frame();
        } while(get_input()!=RIGHT_DOWN);
        return;
    }
    init_wifi(SCAN);
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
    char c;
    int j;
    int highlight=0;
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
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_number); i++) {
            if(i==highlight) 
                draw_rectangle(0,18+i*10,display_width,10,rgbToColour(10,100,100));
            print_ap_info(ap_list+i);
        }
        vec2 tp=get_touchpads();
        if(ap_number!=0) {
            highlight= (highlight+tp.y)%ap_number;
        }
        flip_frame();
        c=get_input();
        if(c==LEFT_DOWN) {
            highlight= (highlight+1)%ap_number;
        }
        if(c==RIGHT_DOWN) {
            uint8_t *ap_name=ap_list[highlight].ssid;
            storage_write_string("ssid",(char *)ap_name);
            wifi_auth_mode_t auth=ap_list[highlight].authmode;
            printf("Wifi Authmode %d\n",auth);
            if(auth==WIFI_AUTH_WPA2_ENTERPRISE) {
                edit_wifi_settings(3);
            }
            if(auth==WIFI_AUTH_WPA2_PSK || auth==WIFI_AUTH_WPA_PSK || auth==WIFI_AUTH_WEP) {
                edit_wifi_settings(1);
            }
            return;
        }
    } while(true);
    esp_wifi_stop();
}
