#pragma once
#include <Arduino.h>

void init_display();
void reset_display_off_timer();
void check_display_timeout();
void display_handle_double_touch();
void display_apply_settings();
void request_device_lock();
void security_note_activity();
