

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

#if USE_WIFI
static EventGroupHandle_t wifi_event_group;
typedef enum {
    SCAN,
    STATION,
    ACCESS_POINT,
    ETHERNET
} wifi_mode_type;

wifi_mode_type wifi_mode=0;
static esp_netif_t *demo_netif = NULL;
int bg_col=0;
#define TAG "Wifi"

const int CONNECTED_BIT = 0x00000001;
char wifi_event[64];
void client_task(void *pvParameters);
TaskHandle_t ctask=NULL;
static void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    ESP_LOGI(tag, "WiFi event: %s %d\n",event_base,event_id);
    snprintf(wifi_event,64,"%s %d\n",event_base,event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if(wifi_mode==STATION)
            esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && ((event_id == IP_EVENT_STA_GOT_IP) || 
    (event_id == IP_EVENT_AP_STAIPASSIGNED) || (event_id == IP_EVENT_ETH_GOT_IP))) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        if(wifi_mode==STATION && event_id == IP_EVENT_STA_GOT_IP) {
            xTaskCreate(client_task,"ct",2048,NULL,1,&ctask);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        esp_wifi_scan_start(NULL,false);
    } else if (!strcmp(event_base,"MQTT_EVENTS")) {
        esp_mqtt_event_handle_t event = event_data;
        if(event_id==MQTT_EVENT_CONNECTED) {
            esp_mqtt_client_handle_t client = event->client;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(demo_netif,&ip_info);
            char buf[64];
            snprintf(buf,sizeof(buf),"Connected:"IPSTR,IP2STR(&ip_info.ip));
            esp_mqtt_client_publish(client, "/topic/a159236", buf, 0, 1, 0);
            esp_mqtt_client_subscribe(client, "/topic/a159236", 0);
        } else if(event_id==MQTT_EVENT_DATA) {    
            char message[event->data_len+1];
            snprintf(message,event->data_len+1,"%s",event->data);
            snprintf(wifi_event,64,"%s %d\n%s\n",event_base,event_id,message);
            int r,g,b;
            if(sscanf(message,"%d,%d,%d",&r,&g,&b)==3)
                bg_col=rgbToColour(r,g,b);
        }
    }
}
 
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

esp_eth_handle_t eth_handle = NULL;
void *glue = NULL;

void init_eth() {
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    if(demo_netif==NULL) {    
        esp_netif_init();
        wifi_event_group = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        demo_netif = esp_netif_new(&cfg);// &netif_config);
        // Set default handlers to process TCP/IP stuffs
        ESP_ERROR_CHECK(esp_eth_set_default_handlers(demo_netif));
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
        ESP_ERROR_CHECK(esp_netif_attach(demo_netif, glue));
        /* start Ethernet driver state machine */
        ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    }
    return;
}

void init_wifi(wifi_mode_type mode) {
   
    if(is_emulator) {
        init_eth();
        return;
    }
    wifi_event_group = xEventGroupCreate();
    wifi_mode=mode;
    if(demo_netif!=NULL) {
        esp_event_loop_delete_default();
        esp_wifi_clear_default_wifi_driver_and_handlers(demo_netif);
        esp_netif_destroy(demo_netif);
    } else
        esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if(mode==ACCESS_POINT)
        demo_netif = esp_netif_create_default_wifi_ap();
    else
        demo_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    uint8_t protocol=(WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N);//|WIFI_PROTOCOL_LR);
    if(mode==ACCESS_POINT) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        esp_wifi_set_protocol(ESP_IF_WIFI_AP,protocol);
        wifi_config_t wifi_config = { .ap = {
                .ssid = EXAMPLE_ESP_WIFI_SSID,
                .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
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
        print_xy("Access Point\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(wifi_event);
        setFontColour(0,255,0);
        if(communicating)
            gprintf("Communicating\n");
        setFont(FONT_SMALL);
        esp_wifi_ap_get_sta_list(&wifi_stations);
        for(int i=0;i<wifi_stations.num;i++) {
            uint8_t *mac=wifi_stations.sta[i].mac;
            gprintf("%02x:%02x:%02x:%02x:%02x:%02x %d\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], wifi_stations.sta[i].rssi); 
        }
        if(wifi_stations.num==0) communicating=0;
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    communicating=0;
    vTaskDelete(stask);
    esp_wifi_stop();
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
        print_xy(wifi_event,1,display_height-8);
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    esp_wifi_stop();
}

void wifi_connect(void) {
    cls(0);
    wifi_event[0]=0;
    init_wifi(STATION);

    do {
        if(received==1) 
            cls(rgbToColour(128,0,0));
        else
            cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Wifi Station\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(wifi_event);
        setFontColour(0,255,0);
        if(communicating)
            gprintf("Communicating\n");
        if(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            wifi_ap_record_t ap;
            gprintf("Connected\n");
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(demo_netif,&ip_info);
            gprintf(IPSTR"\n",IP2STR(&ip_info.ip));
            gprintf(IPSTR"\n",IP2STR(&ip_info.gw));
            esp_wifi_sta_get_ap_info(&ap);
            print_ap_info(&ap);
            
        }
        flip_frame();
    } while(get_input()!=RIGHT_DOWN);
    communicating=0;
    esp_wifi_stop();
}

void wifi_settings() {
   // get_string("Password","");
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
        if(c==RIGHT_DOWN) {
            highlight= (highlight+1)%ap_number;
        }
        if(c==LEFT_DOWN) {
            uint8_t *ap_name=ap_list[highlight].ssid;
            storage_write_string("ssid",(char *)ap_name);
            wifi_auth_mode_t auth=ap_list[highlight].authmode;
            printf("Wifi Authmode %d\n",auth);
            if(auth==WIFI_AUTH_WPA2_ENTERPRISE) {
                char username[64];
                char password[64];
                storage_read_string("username","",username,sizeof(username));
                get_string("Username",username,sizeof(username));
                storage_write_string("username",username);
                storage_read_string("password","",password,sizeof(password));
                get_string("Password",password,sizeof(password));
                storage_write_string("password",password);
            }
            if(auth==WIFI_AUTH_WPA2_PSK || auth==WIFI_AUTH_WPA_PSK || auth==WIFI_AUTH_WEP) {
                char password[64];
                storage_read_string("password","",password,sizeof(password));
                get_string("Password",password,sizeof(password));
                storage_write_string("password",password);
            }
            return;
        }
    } while(true);
    esp_wifi_stop();
}

void mqtt() {
    init_wifi(STATION);
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
        gprintf(wifi_event);
        setFontColour(0,255,0);
        if(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            gprintf("Connected\n");
            if(client==NULL) {
                client=esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, event_handler, NULL);
                esp_mqtt_client_start(client);
            }
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(demo_netif,&ip_info);
            gprintf(IPSTR"\n",IP2STR(&ip_info.ip));
            gprintf(IPSTR"\n",IP2STR(&ip_info.gw));
        }
        flip_frame();
        c=get_input();
        if(c==LEFT_DOWN)
            esp_mqtt_client_publish(client, "/topic/a159236", "left button", 0, 1, 0);
    } while(c!=RIGHT_DOWN);
//    esp_mqtt_client_stop(client);
//    esp_mqtt_client_destroy(client);
}

void time_demo() {
    init_wifi(STATION);
    int sntp_status=0;
    do {
        cls(0);
        setFont(FONT_DEJAVU18);
        setFontColour(0,0,0);
        draw_rectangle(3,0,display_width,18,rgbToColour(255,200,0));
        print_xy("Time\n",5,3);
        setFont(FONT_UBUNTU16);
        setFontColour(255,255,255);
        gprintf(wifi_event);
        if(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
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

#endif
