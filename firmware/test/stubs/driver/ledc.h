#pragma once
#include <cstdint>
typedef int esp_err_t; typedef int ledc_mode_t; typedef int ledc_timer_t; typedef int ledc_channel_t; typedef int ledc_timer_bit_t;
enum { LEDC_LOW_SPEED_MODE=0, LEDC_TIMER_0=0, LEDC_CHANNEL_0=0, LEDC_USE_XTAL_CLK=3 };
struct ledc_timer_config_t { int speed_mode; ledc_timer_bit_t duty_resolution; int timer_num; uint32_t freq_hz; int clk_cfg; };
struct ledc_channel_config_t { int gpio_num, speed_mode, channel, timer_sel; uint32_t duty; int hpoint; };
extern uint32_t g_duty; extern bool g_stopped;
inline esp_err_t ledc_timer_config(const ledc_timer_config_t*){return 0;}
inline esp_err_t ledc_channel_config(const ledc_channel_config_t*){return 0;}
inline esp_err_t ledc_stop(int,int,uint32_t){ g_stopped=true; return 0;}
inline esp_err_t ledc_set_duty(int,int,uint32_t d){ g_duty=d; return 0;}
inline esp_err_t ledc_update_duty(int,int){ g_stopped=false; return 0;}
