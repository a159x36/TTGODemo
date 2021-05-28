#include <freertos/event_groups.h>
#include <esp_wifi.h>


typedef enum {
    SCAN,
    STATION,
    ACCESS_POINT,
} wifi_mode_type;

EventGroupHandle_t network_event_group;
void init_eth();
wifi_mode_type wifi_mode;
#define CONNECTED_BIT 1
esp_netif_t *network_interface;
extern char network_event[64];
extern int bg_col;

void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
void init_wifi(wifi_mode_type mode);