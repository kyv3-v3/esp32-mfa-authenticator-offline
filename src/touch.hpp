#pragma once
#include <lvgl.h>

typedef void (*TouchCallback)();

bool touch_is_calibrated();
void touch_set_calibration_min();
void touch_set_calibration_max();
void touch_save_calibration();
void touch_register();
void init_touch(TouchCallback, TouchCallback);
