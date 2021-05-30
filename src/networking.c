
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
#include "FreeSansBold24pt7b.h"
#include <driver/touch_pad.h>
#include "esp_wpa2.h"

#include "graphics3d.h"
#include "input_output.h"
#include "networking.h"
#include "esp_http_client.h"
#include "esp32/rom/tjpgd.h"

EventGroupHandle_t network_event_group;
char network_event[64];
#define TAG "Networking"
int bg_col=0;
esp_netif_t *network_interface = NULL;
esp_eth_handle_t eth_handle = NULL;
void *glue = NULL;


void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    ESP_LOGI(tag, "WiFi event: %s %d\n",event_base,event_id);
    snprintf(network_event,64,"%s %d\n",event_base,event_id);
    if (event_base == WIFI_EVENT) {
        system_event_sta_disconnected_t* disconnect_data;
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            xEventGroupClearBits(network_event_group, AUTH_FAIL | CONNECTED_BIT);
            if (wifi_mode == STATION)
                esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            disconnect_data=event_data;
            ESP_LOGI(tag, "WiFi DIsconnect: %d",disconnect_data->reason);
            xEventGroupClearBits(network_event_group, CONNECTED_BIT);
            if(disconnect_data->reason==WIFI_REASON_AUTH_FAIL ||
                disconnect_data->reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)
                xEventGroupSetBits(network_event_group, AUTH_FAIL);
            else        
                esp_wifi_connect();
            break;
        case WIFI_EVENT_SCAN_DONE:
            esp_wifi_scan_start(NULL, false);
            break;
        }
    }
    if (event_base == IP_EVENT) {
        if ((event_id == IP_EVENT_STA_GOT_IP) ||
            (event_id == IP_EVENT_AP_STAIPASSIGNED) || (event_id == IP_EVENT_ETH_GOT_IP)) {
            xEventGroupSetBits(network_event_group, CONNECTED_BIT);
        }
    }
    if (!strcmp(event_base,"MQTT_EVENTS")) {
        esp_mqtt_event_handle_t event = event_data;
        if(event_id==MQTT_EVENT_CONNECTED) {
            esp_mqtt_client_handle_t client = event->client;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(network_interface,&ip_info);
            char buf[64];
            snprintf(buf,sizeof(buf),"Connected:"IPSTR,IP2STR(&ip_info.ip));
            esp_mqtt_client_publish(client, "/topic/a159236", buf, 0, 1, 0);
            esp_mqtt_client_subscribe(client, "/topic/a159236", 0);
        } else if(event_id==MQTT_EVENT_DATA) {    
            char message[event->data_len+1];
            snprintf(message,event->data_len+1,"%s",event->data);
            snprintf(network_event,64,"%s %d\n%s\n",event_base,event_id,message);
            int r,g,b;
            if(sscanf(message,"%d,%d,%d",&r,&g,&b)==3)
                bg_col=rgbToColour(r,g,b);
        }
    }
}

void init_eth() {
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    if(network_interface==NULL) {    
        esp_netif_init();
        network_event_group = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        network_interface = esp_netif_new(&cfg);// &netif_config);
        // Set default handlers to process TCP/IP stuffs
        ESP_ERROR_CHECK(esp_eth_set_default_handlers(network_interface));
        // Register user defined event handers
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                &event_handler, NULL));
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
        esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
        esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
        
        ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
        glue=esp_eth_new_netif_glue(eth_handle);
        ESP_ERROR_CHECK(esp_netif_attach(network_interface, glue));
        /* start Ethernet driver state machine */
        ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    }
    return;
}
extern char *main_page_html;

esp_err_t get_handler(httpd_req_t *req)
{
    printf("Get %s\n",req->uri);
    if(!strcmp(req->uri,"/red")) bg_col=rgbToColour(255,0,0);
    if(!strcmp(req->uri,"/green")) bg_col=rgbToColour(0,255,0);
    if(!strcmp(req->uri,"/blue")) bg_col=rgbToColour(0,0,255);
    ESP_ERROR_CHECK(httpd_resp_send(req, main_page_html , HTTPD_RESP_USE_STRLEN));
    return ESP_OK;
}
/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/*",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};
void webserver(void) {
    init_wifi(ACCESS_POINT);
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_get);
    }
    do {
        cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(255,255,0);
        draw_rectangle(0,0,display_width,display_height,bg_col);
        gprintf("Web Server\nConnect to the ESP 32\nAccess Point and go to\nhttp://192.168.4.1/");
        setFont(FONT_SMALL);
        setFontColour(255,255,255);
        print_xy(network_event,1,display_height-8);
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
}

QueueHandle_t imageQueue=NULL;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    ESP_LOGI(TAG, "http event %d",(evt->event_id));
    if(evt->event_id==HTTP_EVENT_ON_DATA) {
        char *data=(char *)(evt->data);
        for(int i=0;i<evt->data_len;i++) {
            int s=data[i];
            xQueueSend(imageQueue, &s, portMAX_DELAY);
        }
    }
    if(evt->event_id==HTTP_EVENT_ON_FINISH) {
        int s=-1;
        xQueueSend(imageQueue, &s, portMAX_DELAY);
    }
    return ESP_OK;
}

static uint32_t jpg_read(JDEC *decoder, uint8_t *buf, uint32_t len) {
    for(int i=0;i<len;i++) {
        int data;
        xQueueReceive(imageQueue, &data, portMAX_DELAY);
        if(data<0)
            return i;
        if(buf)
            *buf++=data;
    }
    return len;
}

static uint32_t jpg_write(JDEC *decoder, void *bitmap, JRECT *rect) {
    char *rgb=(char *)bitmap;
    for(int y=rect->top;y<=rect->bottom; y++)
        for(int x=rect->left;x<=rect->right; x++) {
            int r=*rgb++,g=*rgb++,b=*rgb++;
            draw_pixel(x,y,rgbToColour(r,g,b));
        }
    return 1;
}

void web_task(void *pvParameters) {
    esp_http_client_config_t config = {
        .url = "http://www.trafficnz.info/camera/20.jpg",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
           esp_http_client_get_status_code(client),
           esp_http_client_get_content_length(client));
    } else {
        int s=-2;
        xQueueSend(imageQueue, &s, portMAX_DELAY);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void web_client(void) {
    wifi_connect(1);
    if(imageQueue==NULL) imageQueue=xQueueCreate( 512, 4);
    TaskHandle_t wtask;
    cls(0);
    gprintf("Connected");
    flip_frame();
    JDEC decoder;
    char *work=malloc(3100);
    xTaskCreate(web_task,"wt",4096,NULL,1,&wtask);
    int r = jd_prepare(&decoder, jpg_read, work, 3100, NULL);    
    cls(bg_col);
    if (r == JDR_OK)
        r = jd_decomp(&decoder, jpg_write, 1);
    free(work);
    flip_frame();
    while(get_input()!=RIGHT_DOWN);
}


void mqtt() {
    wifi_connect(1);
    esp_mqtt_client_config_t mqtt_cfg = { .uri = "mqtt://mail.marginz.co.nz" };
    esp_mqtt_client_handle_t client = NULL;
    char c;
    do {
        cls(bg_col);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("MQTT\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        setFontColour(0,255,0);
        if(xEventGroupGetBits(network_event_group) & CONNECTED_BIT) {
            gprintf("Connected\n");
            if(client==NULL) {
                client=esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, event_handler, NULL);
                esp_mqtt_client_start(client);
            }
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(network_interface,&ip_info);
            gprintf(IPSTR"\n",IP2STR(&ip_info.ip));
            gprintf(IPSTR"\n",IP2STR(&ip_info.gw));
        }
        flip_frame();
        c=get_input();
        if(c==LEFT_DOWN)
            esp_mqtt_client_publish(client, "/topic/a159236", "left button", 0, 1, 0);
    } while(c!=RIGHT_DOWN);
    if(client!=NULL) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client=NULL;
    }
}

void time_demo() {
    wifi_connect(1);
    int sntp_status=0;
    do {
        cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Time\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(network_event);
        if(xEventGroupGetBits(network_event_group) & CONNECTED_BIT) {
            if(sntp_status==0) {
                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                sntp_setservername(0, "pool.ntp.org");
                sntp_init();
                sntp_status=1;
            }
            time(&time_now);
            tm_info = localtime(&time_now);
            setFont(FreeSansBold24pt7b);
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            if(tm_info->tm_year < (2016 - 1900) )
                setFontColour(255,0,0);
                else
                setFontColour(0,255,0);
            gprintf("\n%2d:%02d:%02d", tm_info->tm_hour,
                 tm_info->tm_min, tm_info->tm_sec);
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    //sntp_stop();
    //esp_wifi_stop();
}

