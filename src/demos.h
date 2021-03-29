
void life_demo();
void spaceship_demo();
void image_wave_demo();
int get_input();
void sensors_demo();
void teapots_demo();

// put your wifi ssid name and password in here
#define WIFI_SSID "MUGuests"
#define WIFI_PASSWORD ""

#define USE_WIFI 0
#define DISPLAY_VOLTAGE 1
#define DISPLAY_IMAGE_WAVE 1

extern time_t time_now;
extern struct tm *tm_info;

extern const char *tag;
void initialise_wifi(void);
int obtain_time(void);