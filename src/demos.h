
void life_demo();
void spaceship_demo();
void image_wave_demo();
void sensors_demo();
void teapots_demo();
void bubble_demo();
void wifi_scan();
void wifi_connect(int onlyconnect);
int wifi_connected();
void wifi_disconnect();
void wifi_ap();
void webserver();
void web_client();
void mqtt();
void time_demo();
void boids_demo();
void mqtt_leds();
void led_circles(void);
void led_numbers(void);
void led_cube(void);
void ledc_backlight_demo(void);
void ledc_servo_demo(void);
void mcpwm_demo(void);
void gpio_backlight_demo(void);
void gpio_servo_demo(void);

#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

#define USE_WIFI 1
#define DISPLAY_VOLTAGE 1

#ifdef TTGO_S3
    #define VOLTAGE_GPIO 4
    #define VOLTAGE_ADC ADC_CHANNEL_3
#else
    #define VOLTAGE_GPIO 34
    #define VOLTAGE_ADC ADC_CHANNEL_6
#endif
#define DISPLAY_IMAGE_WAVE 1

extern struct tm *tm_info;
extern int is_emulator;

extern const char *tag;
void initialise_wifi(void);
int obtain_time(void);

// these allow intellisense to work
#ifndef __ASSERT_FUNCTION
#define __ASSERT_FUNCTION 0
#endif
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL 3
#endif
