#pragma once
#include <cstdint>
#include "soc/ledc_struct.h"
extern uint32_t g_div; extern int g_updates;
inline void ledc_ll_set_clock_divider(ledc_dev_t*,int,int,uint32_t d){ g_div=d; }
inline void ledc_ll_ls_timer_update(ledc_dev_t*,int,int){ g_updates++; }
