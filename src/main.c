/* TTGO Demo example for 159236

*/
#include <driver/gpio.h>
#include <driver/i2c.h>
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

#include "fonts.h"
#include "graphics.h"
#include "image_wave.h"
#include "demos.h"
#include "graphics3d.h"
#include "input_output.h"

#define PAD_START 3
#define PAD_END 5

#define SHOW_PADS

void wifi_settings_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Choose AP","SSID","Username",
                         "Password", "Back"};
        sel=demo_menu("Wifi Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan(1);
                break;
            case 1:
                edit_stored_string("ssid","SSID");
                break;
            case 2:
                edit_stored_string("username","Username");
                break;
            case 3:
                edit_stored_string("password","Password");
                break;
            case 4:
                return;
        }
    }
}

void led_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"MQTT", "Circles", "Numbers", "Cube", "Back"};
        sel=demo_menu("Leds Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                mqtt_leds();
                break;
            case 1:
                led_circles();
                break;
            case 2:
                led_numbers();
                break;
            case 3:
                led_cube();
                break;
            case 4:
                return;
        }
    }
}

void wifi_menu() {
    int sel=0;
    while(1) {
        int connected=wifi_connected();
        char *entries[]={"Scan",connected?"Disconnect":"Connect","Access Point",
                         "Settings", "Back"};
        sel=demo_menu("Wifi Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_scan(0);
                break;
            case 1:
                if(connected)
                    wifi_disconnect();
                else 
                    wifi_connect(0);
                break;
            case 2:
                wifi_ap();
                break;
            case 3:
                wifi_settings_menu();
                break;
            case 4:
                return;
        }
    }
}
void graphics_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Boids", "Life","Image Wave", "Spaceship", "Teapots","Back"};
        sel=demo_menu("Graphics Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                boids_demo();            
                break;
            case 1:
                life_demo();
                break;
            case 2:
                image_wave_demo();
                break;
            case 3:
                spaceship_demo();
                break;
            case 4:
                teapots_demo();
                break;
            case 5:
                return;
        }
    }
}

void pwm_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"LEDC Backlight","LEDC Servo","MCPWM Servo","GPIO Backlight","GPIO Servo","Back"};
        sel=demo_menu("PWM Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                ledc_backlight_demo();
                break;
            case 1:
                ledc_servo_demo();
                break;
            case 2:
                mcpwm_demo();
                break;
            case 3:
                gpio_backlight_demo();
                break;
            case 4:
                gpio_servo_demo();
                break;
            case 5:
                return;
        }
    }
}
void network_menu() {
    int sel=0;
    while(1) {
        char *entries[]={"Wifi","MQTT","Time","Web Server","Web Client","Back"};
        sel=demo_menu("Network Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch(sel) {
            case 0:
                wifi_menu();
                break;
            case 1:
                mqtt();
                break;
            case 2:
                time_demo();
                break;
            case 3:
                webserver();
                break;
            case 4:
                web_client();
                break;
            case 5:
                return;
        }
    }
}


#define MPU6050_REG_CHIP_ID             0x75
#define MPU6050_CHIP_ID                 0x68

#define MPU6050_REG_GYRO_CFG            0x1B
#define MPU6050_GYRO_FS_SHIFT           3

#define MPU6050_REG_ACCEL_CFG           0x1C
#define MPU6050_ACCEL_FS_SHIFT          3

#define MPU6050_REG_INT_EN              0x38
#define MPU6050_DRDY_EN                 BIT(0)

#define MPU6050_REG_DATA_START          0x3B

#define MPU6050_REG_PWR_MGMT1           0x6B
#define MPU6050_SLEEP_EN                BIT(6)

#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define MPU6050_ADDR 0x68
#define VCC 32
#define GND 33
#define SCL 25
#define SDA 26

static esp_err_t i2c_read(uint8_t reg, uint8_t *data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


static esp_err_t i2c_write_byte(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int i2c_read_byte(int reg) {
        uint8_t val;
        int err=i2c_read(reg, &val, 1);
        if(err!=0) printf("Error %d\n",err);
        return val;
}
int16_t buf[7];

void read_mpu6050() {
        i2c_read(MPU6050_REG_DATA_START, (uint8_t *)&buf, 14);
        for(int i=0;i<7;i++) {
                buf[i]=(buf[i]<<8) | (buf[i]>>8 & 0xff);
        }
        ESP_LOGI(tag, "read %d %d %d %d %d %d %d\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
}

void mpu6050_init() {
    gpio_set_direction(GND,GPIO_MODE_OUTPUT);
    gpio_set_level(GND,0);
    gpio_set_direction(VCC,GPIO_MODE_OUTPUT);
    gpio_set_level(VCC,1);
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    ESP_LOGI(tag, "sda_io_num %d", SDA);
    conf.sda_io_num = SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    ESP_LOGI(tag, "scl_io_num %d", SCL);
    conf.scl_io_num = SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 200000;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
    ESP_LOGI(tag, "i2c_param_config %d", conf.mode);
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_LOGI(tag, "i2c_driver_install %d", I2C_NUM_0);
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

    int id=i2c_read_byte(MPU6050_REG_CHIP_ID);
    
    ESP_LOGI(tag, "Found MPU6050 id=%x\n", id);
    
    int pwr=i2c_read_byte(MPU6050_REG_PWR_MGMT1);
    ESP_LOGI(tag, "pwr=%x\n", pwr);
    pwr &= ~(MPU6050_SLEEP_EN);
    
    i2c_write_byte(MPU6050_REG_PWR_MGMT1,pwr);
    
    i2c_write_byte(MPU6050_REG_ACCEL_CFG, 0); // 2g fs
    i2c_write_byte(MPU6050_REG_GYRO_CFG, 0); //  250DEG/SEC 
}


#define VCC 32
#define GND 33
#define SCL 25
#define SDA 26

void app_main() {
    // initialise button handling
    input_output_init();
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    // ===== Set time zone to NZ time using custom daylight savings rule======
    // if you are anywhere else in the world, this will need to be changed
    setenv("TZ", "NZST-12:00:00NZDT-13:00:00,M9.5.0,M4.1.0", 0);
    tzset();
    // initialise graphics and lcd display
    graphics_init();
    cls(0);
    // Initialize the image wave
    if (DISPLAY_IMAGE_WAVE) image_wave_init();
    mpu6050_init();
    while(get_input()!=LEFT_DOWN) {
            read_mpu6050();
            vTaskDelay(5);
    }
    // main menu
    int sel=0;
    while(1) {
        char *entries[]={"Graphics","Networking","Leds",
                        "PWM","Bubble Game",
                        get_orientation()?"Landscape":"Portrait"};
        sel=demo_menu("Demo",sizeof(entries)/sizeof(char *),entries,sel);
        
        switch(sel) {
            case 0:
                graphics_menu();
                break;
            case 1:
                network_menu();
                break;
            case 2:
                led_menu();
                break;
            case 3:
                pwm_menu();
                break;
            case 4:
                bubble_demo();
                break;
            case 5:
                set_orientation(1-get_orientation());
                break;
        }
    }
}
