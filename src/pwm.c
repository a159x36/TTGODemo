#include <driver/ledc.h>
#include <driver/gpio.h>
#include <graphics.h>
#include <fonts.h>
#include <esp_timer.h>
//#include <rom/ets_sys.h>
#include <driver/mcpwm_prelude.h>
#include "input_output.h"

#ifdef TTGO_S3
#define SERVO_PIN 21
#else
#define SERVO_PIN 27
#endif

void display_duty(int duty, bool servo) {
    cls(rgbToColour(0,0,100));
    if(servo) {
        setFontColour(255,255,0);
        setFont(FONT_DEJAVU18);
        gprintf("Attach servo to pin %d\nor switch to servo window\non emulator\n\n",SERVO_PIN);
    }
    setFontColour(255,255,255);
    setFont(FONT_DEJAVU24);
    gprintf("Duty:%d",duty);
    flip_frame();
}


void ledc_backlight_demo(void) {
    const int timer_res=10;
    const int res_max=1<<timer_res;
    const int mode=LEDC_LOW_SPEED_MODE;
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
        .gpio_num   = PIN_NUM_BCKL,
        .speed_mode = mode,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
    int on=1;
    while(1) {
        int duty=ledc_get_duty(mode,channel);
        display_duty(duty,false);
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
        if(key==LEFT_DOWN) {
            ledc_set_duty_and_update(mode,channel,max,0);
            ledc_fade_func_uninstall();
            return;
        }
    }
}
void ledc_servo_demo(void) {
    const int timer_res=13;
    const int res_max=1<<timer_res;
    const int mode=LEDC_LOW_SPEED_MODE;
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
        .gpio_num   = SERVO_PIN,
        .speed_mode = mode,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_2
    };
    gpio_set_direction(SERVO_PIN, GPIO_MODE_OUTPUT);
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
    int on=1;
    while(1) {
        display_duty(ledc_get_duty(mode,channel),true);
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
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .period_ticks = 20000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    mcpwm_new_timer(&timer_config, &timer);
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    mcpwm_new_operator(&operator_config, &oper);
    mcpwm_operator_connect_timer(oper, timer);
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    mcpwm_new_comparator(oper, &comparator_config, &comparator);
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PIN,
    };
    mcpwm_new_generator(oper, &generator_config, &generator);
    mcpwm_comparator_set_compare_value(comparator, 2000);
    mcpwm_generator_set_action_on_timer_event(generator, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
                        MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(generator,MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
                        comparator, MCPWM_GEN_ACTION_LOW));
    mcpwm_timer_enable(timer);
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP);
    int duty=1500; 
    while(1) {
        display_duty(duty,true);
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
        if(key==RIGHT_DOWN) {
            duty=(duty+100)%2000;
        }
        if(duty<1000) duty=1000;

        mcpwm_comparator_set_compare_value(comparator, duty);
    }
}


void gpio_backlight_demo(void) {
    int64_t duty=5000;
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    while(1) {
        int64_t start=esp_timer_get_time();
        gpio_set_level(PIN_NUM_BCKL,1);
        display_duty((int)duty,false);
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
        if(key==RIGHT_DOWN) {
            duty=duty+1000;
            if(duty>10000) duty=1000;
        }
        while(esp_timer_get_time()<start+duty);
        gpio_set_level(PIN_NUM_BCKL,0);
        delay_us(10000-duty);
    }
}

void gpio_servo_demo(void) {
    int duty=1000;
    gpio_set_direction(SERVO_PIN, GPIO_MODE_OUTPUT);
    while(1) {
        display_duty(duty,true);
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
        if(key==RIGHT_DOWN) {
            duty=duty+100;
            if(duty>2000) duty=1000;
        }
        gpio_set_level(SERVO_PIN,1);
        delay_us(duty);
        gpio_set_level(SERVO_PIN,0);
        delay_us((20000-duty));
    }
}
