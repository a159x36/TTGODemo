
void life_demo();
void spaceship_demo();
void image_wave_demo();
void sensors_demo();
void teapots_demo();
void bubble_demo();
void wifi_scan();
void wifi_connect(int onlyconnect);
void wifi_ap();
void webserver();
void web_client();
void mqtt();
void time_demo();
void boids_demo();
void mqtt_leds();
void led_circles(void);


// put your wifi ssid name and password in here
//#define WIFI_SSID "MUGuests"
//#define WIFI_PASSWORD ""

#define USE_WIFI 1
#define DISPLAY_VOLTAGE 1
#define DISPLAY_IMAGE_WAVE 1

extern time_t time_now;
extern struct tm *tm_info;
extern int is_emulator;

extern const char *tag;
void initialise_wifi(void);
int obtain_time(void);