#include <driver/ledc.h>
#include <driver/gpio.h>
#include <graphics.h>
#include <fonts.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>
#define CONFIG_MCPWM_SUPPRESS_DEPRECATE_WARN 1
#include <driver/mcpwm.h>
#include "input_output.h"

void ledc_backlight_demo(void) {
    const int timer_res=10;
    const int res_max=1<<timer_res;
    const int mode=LEDC_HIGH_SPEED_MODE;
    const int channel=LEDC_CHANNEL_1;
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = timer_res, // resolution of PWM duty
        .freq_hz = 200,                      // frequency of PWM signal
        .speed_mode = mode,    // timer mode
        .timer_num = LEDC_TIMER_1,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    int min=1;
    int max= res_max-1;
    ledc_channel_config_t ledc_channel = {
        .channel    = channel,
        .duty       = max,
        .gpio_num   = 4,
        .speed_mode = mode,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
    int on=1;
    while(1) {
        cls(rgbToColour(0,0,100));
        setFont(FONT_DEJAVU24);
        int duty=ledc_get_duty(mode,channel);
        gprintf("%d",duty);
        flip_frame();
        key_type key=get_input();
        
        if(key==RIGHT_DOWN) {
            if(on && duty==max) {
                ledc_set_fade_with_time(mode, channel, min,1000);
                ledc_fade_start(mode, channel, LEDC_FADE_NO_WAIT);
                on=0;
            } else {
                if(!on && duty==min) {
                    ledc_set_fade_with_time(mode, channel, max,1000);
                    ledc_fade_start(mode, channel, LEDC_FADE_NO_WAIT);
                    on=1;
                }
            }
            
        }
        if(key==LEFT_DOWN) return;
    }
}
void ledc_servo_demo(void) {
    const int timer_res=13;
    const int res_max=1<<timer_res;
    const int mode=LEDC_HIGH_SPEED_MODE;
    const int channel=LEDC_CHANNEL_7;
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = timer_res, // resolution of PWM duty
        .freq_hz = 50,                      // frequency of PWM signal
        .speed_mode = mode,    // timer mode
        .timer_num = LEDC_TIMER_2,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .channel    = channel,
        .duty       = (15*res_max)/200,
        .gpio_num   = 27,
        .speed_mode = mode,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_2
    };
    gpio_set_direction(27, GPIO_MODE_OUTPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
    int on=1;
    while(1) {
        cls(rgbToColour(0,0,100));
        setFont(FONT_DEJAVU24);
        gprintf("%d",ledc_get_duty(mode,channel));
        flip_frame();
        key_type key=get_input();
        if(key==RIGHT_DOWN) {
            on=!on;
            if(on ) ledc_set_fade_with_time(mode, channel, (10*res_max)/200,500);
            else ledc_set_fade_with_time(mode, channel, (20*res_max)/200,500);
            ledc_fade_start(mode, channel, LEDC_FADE_NO_WAIT);
        }
        if(key==LEFT_DOWN) return;
    }
}

void mcpwm_demo(void) {

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 27);
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //50Hz = 20ms period
    pwm_config.cmpr_a = 1500;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxB = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    int duty=1500; 
    while(1) {
        cls(rgbToColour(0,0,100));
        setFont(FONT_DEJAVU24);
        gprintf("%d",duty);
        flip_frame();
        if(!gpio_get_level(0))
            duty-=20;
        if(!gpio_get_level(35))
            duty+=20;
        if(duty<1000) duty=1000;
        if(duty>2000) duty=2000;
        mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0,
                             MCPWM_OPR_A, duty);
    }
}
void gpio_backlight_demo(void) {
    int64_t duty=5000;
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_direction(4, GPIO_MODE_OUTPUT);
    while(1) {
        int64_t start=esp_timer_get_time();
        gpio_set_level(4,1);
        cls(rgbToColour(0,0,100));
        setFont(FONT_DEJAVU24);
        gprintf("%d",(int)duty);
        flip_frame();
        if(!gpio_get_level(0))
            duty-=20;
        if(!gpio_get_level(35))
            duty+=20;
        if(duty<100) duty=100;
        if(duty>10000) duty=10000;
        while(esp_timer_get_time()<start+duty);
        gpio_set_level(4,0);
        ets_delay_us(10000-duty);
    }
}

void gpio_servo_demo(void) {
    int duty=1000;
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_direction(27, GPIO_MODE_OUTPUT);
    while(1) {
        cls(rgbToColour(0,0,100));
        setFont(FONT_DEJAVU24);
        gprintf("%d",duty);
        flip_frame();
        if(!gpio_get_level(0))
            duty-=20;
        if(!gpio_get_level(35))
            duty+=20;
        if(duty<1000) duty=1000;
        if(duty>2000) duty=2000;
        gpio_set_level(27,1);
        ets_delay_us(duty);
        gpio_set_level(27,0);
        ets_delay_us((20000-duty));;
    }
}
