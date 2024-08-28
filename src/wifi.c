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
#include "mqtt_client.h"
#include <driver/touch_pad.h>
#include "esp_eap_client.h"

#include "graphics3d.h"
#include "input_output.h"
#include "networking.h"

typedef void (* wifi_tx_done_cb_t)(uint8_t ifidx, uint8_t *data, uint16_t *data_len, bool txStatus);
esp_err_t esp_wifi_set_tx_done_cb(wifi_tx_done_cb_t cb);

wifi_mode_type wifi_mode=0;

#define TAG "Wifi"
#define SNIFF 0
 
#define DEFAULT_SCAN_LIST_SIZE 24

void sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
    ets_printf("\nRx type: %d\n",type);
    uint8_t *ubuf=buf;
    wifi_pkt_rx_ctrl_t *pkt=buf;
    for(int i=0;i<pkt->sig_len+28;i++) {
        if((i%16)==0) ets_printf("\n%04x: ",i);
        ets_printf("%02x ",ubuf[i]);
    }
    ets_printf("\n");
}

void sniff_tx(uint8_t ifidx, uint8_t *data, uint16_t *data_len, bool txStatus) {
    ets_printf("\nTx type: %d\n",data[0]);
    uint8_t *ubuf=data;
    for(int i=0;i<*data_len;i++) {
        if((i%16)==0) ets_printf("\n%04x: ",i);
        ets_printf("%02x ",ubuf[i]);
    }
    ets_printf("\n");
}

int wifi_connected() {
    if(network_event_group)
        return xEventGroupGetBits(network_event_group) & CONNECTED_BIT;
    return 0;
}

void wifi_disconnect() {

    if(network_interface!=NULL) {
        if(wifi_connected()) {
            esp_wifi_disconnect();
            while(wifi_connected());
        }
        esp_wifi_stop();
     //   esp_wifi_deinit();
     //   esp_event_loop_delete_default();
     //   esp_wifi_clear_default_wifi_driver_and_handlers(network_interface);
     //   esp_netif_destroy(network_interface);
       // esp_netif_deinit();
       // network_interface=NULL;
    }
}
void init_wifi(wifi_mode_type mode) {
    if(wifi_mode==mode && network_interface!=NULL &&
            (xEventGroupGetBits(network_event_group) & CONNECTED_BIT))
        return;
    if(network_event_group==NULL)
        network_event_group = xEventGroupCreate();
    xEventGroupClearBits(network_event_group, AUTH_FAIL | CONNECTED_BIT);
    wifi_mode=mode;
    if(network_interface!=NULL) {
        esp_wifi_stop();
    }
    if(network_interface==NULL) {
        esp_netif_init();
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        network_interface_ap = esp_netif_create_default_wifi_ap();   
        network_interface = esp_netif_create_default_wifi_sta();    
    
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    }

    uint8_t protocol=(WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N);//|WIFI_PROTOCOL_LR);
    if(mode==ACCESS_POINT) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        esp_wifi_set_protocol(ESP_IF_WIFI_AP,protocol);
        #define SSID "ESP32"
        wifi_config_t wifi_config = { .ap = {
                .ssid = SSID,
                .ssid_len = strlen(SSID),
                .channel = 3,
                .password = "",
                .max_connection = 8,
                .authmode = WIFI_AUTH_OPEN
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config)); 
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        esp_wifi_set_protocol(ESP_IF_WIFI_STA,protocol);
        char ssid[32];
        storage_read_string("ssid","MasseyWifi",ssid,sizeof(ssid));
        char password[64];
        storage_read_string("password","", password, sizeof(password));
        char username[64];
        storage_read_string("username","", username, sizeof(username));
        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid,ssid,sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password,password,sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        if(strlen(username)!=0) {
            ESP_ERROR_CHECK( esp_eap_client_set_username((uint8_t *)username, strlen(username)) );
            ESP_ERROR_CHECK( esp_eap_client_set_password((uint8_t *)password, strlen(password)) );
            ESP_ERROR_CHECK( esp_wifi_sta_enterprise_enable() );
        }
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    if(SNIFF) {
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(sniff));
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
        wifi_promiscuous_filter_t pf;
        pf.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&pf));
        pf.filter_mask=WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_ctrl_filter(&pf));
        ESP_ERROR_CHECK(esp_wifi_set_tx_done_cb(sniff_tx));
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
    do {
        cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(220,220,0));
        print_xy("Access Point\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        setFontColour(0,255,0);
        setFont(FONT_SMALL);
        esp_wifi_ap_get_sta_list(&wifi_stations);
        for(int i=0;i<wifi_stations.num;i++) {
            uint8_t *mac=wifi_stations.sta[i].mac;
            gprintf("%02x:%02x:%02x:%02x:%02x:%02x %d\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], wifi_stations.sta[i].rssi); 
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
}


void wifi_connect(int onlyconnect) {
    cls(0);
    network_event[0]=0;
    init_wifi(STATION);
    do {
        cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,20,rgbToColour(220,220,0));
        print_xy("Connect\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        setFontColour(0,255,0);
        if(xEventGroupGetBits(network_event_group) & CONNECTED_BIT) {
            wifi_ap_record_t ap;
            gprintf("Connected\n");
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(network_interface,&ip_info);
            gprintf(IPSTR"\n",IP2STR(&ip_info.ip));
            gprintf(IPSTR"\n",IP2STR(&ip_info.gw));
            if(onlyconnect) {
                flip_frame();
                return;
            }
            esp_wifi_sta_get_ap_info(&ap);
            print_ap_info(&ap);
        }
        if(xEventGroupGetBits(network_event_group) & AUTH_FAIL) {
            gprintf("Authentication Failed\n");
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
}

void wifi_scan(int setap) {
    cls(0);
    init_wifi(SCAN);
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t *ap_info=malloc(sizeof(wifi_ap_record_t)*DEFAULT_SCAN_LIST_SIZE);
    wifi_ap_record_t *ap_list=malloc(sizeof(wifi_ap_record_t)*DEFAULT_SCAN_LIST_SIZE);
    memset(ap_info, 0, sizeof(wifi_ap_record_t)*DEFAULT_SCAN_LIST_SIZE);
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
        draw_rectangle(3,0,display_width,18,rgbToColour(220,220,0));
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
            if(setap) {
                uint8_t *ap_name=ap_list[highlight].ssid;
                storage_write_string("ssid",(char *)ap_name);
            }
            //esp_wifi_scan_stop();
            free(ap_list);
            free(ap_info);
            return;
        }
    } while(true);
}
