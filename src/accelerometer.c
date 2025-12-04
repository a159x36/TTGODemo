#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>


#define tag "mpu6050"

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

#define MPU6050_ADDR 0x68

#define I2C_MASTER_TIMEOUT_MS       1000

#ifdef TTGO_S3
#define VCC 43
#define GND 44
#define SCL 18
#define SDA 17
#else
#define VCC 32
#define GND 33
#define SCL 25
#define SDA 26
#endif

static i2c_master_dev_handle_t dev_handle=0;

static esp_err_t i2c_read(uint8_t reg, uint8_t *data, size_t len) {
    if (len == 0) return ESP_OK;
    return i2c_master_transmit_receive(dev_handle, &reg, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}


static esp_err_t i2c_write_byte(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int i2c_read_byte(int reg) {
        uint8_t val;
        int err=i2c_read(reg, &val, 1);
        if(err!=0) ESP_LOGE(tag, "I2C Read Error %d",err);
        return val;
}

void read_mpu6050(int16_t *buf) {
        i2c_read(MPU6050_REG_DATA_START, (uint8_t *)buf, 14);
        for(int i=0;i<7;i++) {
                buf[i]=(buf[i]<<8) | (buf[i]>>8 & 0xff);
        }
        ESP_LOGI(tag, "read %d %d %d %d %d %d %d",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
}


void mpu6050_init() {
    gpio_config_t cfg={.mode=GPIO_MODE_OUTPUT,.pin_bit_mask=(1ULL<<GND)| (1ULL<<VCC)};
    gpio_config(&cfg);
    gpio_set_level(GND,0);
    gpio_set_level(VCC,1);
    if(dev_handle!=0) return;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = 0,
        .sda_io_num = SDA,
        .scl_io_num = SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 1000000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    int id=i2c_read_byte(MPU6050_REG_CHIP_ID);
    
    ESP_LOGI(tag, "Found MPU6050 id=%x\n", id);
    
    int pwr=i2c_read_byte(MPU6050_REG_PWR_MGMT1);
    ESP_LOGI(tag, "pwr=%x\n", pwr);
    pwr &= ~(MPU6050_SLEEP_EN);
    
    i2c_write_byte(MPU6050_REG_PWR_MGMT1,pwr);
    
    i2c_write_byte(MPU6050_REG_ACCEL_CFG, 0); // 2g fs
    i2c_write_byte(MPU6050_REG_GYRO_CFG, 0); //  250DEG/SEC 
}


