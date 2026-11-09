#include <Arduino.h>
#include <SPIFFS.h>
#include "ui/ui.h"
#include "constants.h"
#include "mfa.h"
#include "clock.hpp"
#include "touch.hpp"
#include "display.hpp"
#include "settings.hpp"
#include "usb-manager.hpp"
#include "totp_core.hpp"

enum ApplicationState
{
  START,
  TOUCH_CALIBRATION_START,
  TOUCH_CALIBRATION_MIN,
  TOUCH_CALIBRATION_MAX,
  TOUCH_CALIBRATION_UPDATE,
  TOUCH_CALIBRATION_COMPLETE,
  SECURITY_WAIT,
  CLOCK_SETUP,
  TOTPS_UPDATE
};

static lv_obj_t *clock_screen = nullptr;
static lv_obj_t *clock_input = nullptr;
static lv_obj_t *clock_message = nullptr;
static bool clock_ready = false;
static bool clock_edit_requested = false;
static bool security_ready = false;
static bool lock_requested = false;
static unsigned long last_security_activity = 0;

static void on_touch() { reset_display_off_timer(); security_note_activity(); }
static void on_double_touch() { display_handle_double_touch(); }

extern "C" void notify_security_ready(void) { security_ready = true; last_security_activity = millis(); }
void security_note_activity() { last_security_activity = millis(); }
void request_device_lock() { lock_requested = true; }

extern "C" void request_clock_edit(void)
{
  clock_edit_requested = true;
}

static void on_clock_button(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED)
    return;

  lv_obj_t *keyboard = lv_event_get_target(event);
  uint32_t button = lv_btnmatrix_get_selected_btn(keyboard);
  if (button == LV_BTNMATRIX_BTN_NONE)
    return;

  const char *text = lv_btnmatrix_get_btn_text(keyboard, button);
  if (!text)
    return;

  if (strcmp(text, "DEL") == 0)
    lv_textarea_del_char(clock_input);
  else if (strcmp(text, "CLEAR") == 0)
    lv_textarea_set_text(clock_input, "");
  else if (strcmp(text, "OK") == 0)
  {
    if (set_clock_utc(lv_textarea_get_text(clock_input)))
      clock_ready = true;
    else
      lv_label_set_text(clock_message, "Invalid date or time");
  }
  else
  {
    lv_textarea_add_text(clock_input, text);
    lv_label_set_text(clock_message, "UTC time, 14 digits");
  }
}

static void show_clock_screen()
{
  if (clock_screen)
  {
    lv_textarea_set_text(clock_input, "");
    lv_label_set_text(clock_message, "UTC time, 14 digits");
    ui_load_screen(clock_screen, false);
    return;
  }

  clock_screen = lv_obj_create(nullptr);
  lv_obj_set_user_data(clock_screen, (void *)"clock");
  lv_obj_set_style_bg_color(clock_screen, lv_color_make(12, 18, 30), 0);
  lv_obj_set_style_text_color(clock_screen, lv_color_white(), 0);

  lv_obj_t *title = lv_label_create(clock_screen);
  lv_label_set_text(title, "UTC: DDMMYYYYHHMMSS");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

  clock_input = lv_textarea_create(clock_screen);
  lv_textarea_set_one_line(clock_input, true);
  lv_textarea_set_max_length(clock_input, 14);
  lv_textarea_set_placeholder_text(clock_input, "E.g. 09112026120000");
  lv_obj_set_size(clock_input, 280, 38);
  lv_obj_align(clock_input, LV_ALIGN_TOP_MID, 0, 29);
  lv_obj_set_style_text_align(clock_input, LV_TEXT_ALIGN_CENTER, 0);

  clock_message = lv_label_create(clock_screen);
  lv_label_set_text(clock_message, "UTC time, 14 digits");
  lv_obj_align(clock_message, LV_ALIGN_TOP_MID, 0, 72);

  static const char *buttons[] = {
      "1", "2", "3", "4", "5", "\n",
      "6", "7", "8", "9", "0", "\n",
      "DEL", "CLEAR", "OK", ""};
  lv_obj_t *keyboard = lv_btnmatrix_create(clock_screen);
  lv_btnmatrix_set_map(keyboard, buttons);
  lv_obj_set_size(keyboard, 300, 145);
  lv_obj_set_style_pad_all(keyboard, 2, 0);
  lv_obj_set_style_pad_row(keyboard, 2, 0);
  lv_obj_set_style_pad_column(keyboard, 4, 0);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_add_event_cb(keyboard, on_clock_button, LV_EVENT_VALUE_CHANGED, nullptr);

  ui_load_screen(clock_screen, false);
}

void setup()
{
  usb_manager_begin();
  SPIFFS.begin(true);
  settings_load();
  init_display();
  init_touch(on_touch, on_double_touch);
  init_ui();
  display_apply_settings();
  if (totp_self_test()) return;
  ui_show_error_screen("Fatal Error", "Code self-test failed");
  while (true) { ui_task_handler(); delay(100); }
}

void loop()
{
  static ApplicationState application_state = START;
  switch (application_state)
  {
  case START:
    if (touch_is_calibrated())
    {
      application_state = TOUCH_CALIBRATION_COMPLETE;
    }
    else
    {
      application_state = TOUCH_CALIBRATION_START;
    }
    break;

  case TOUCH_CALIBRATION_START:
    static unsigned long state_change_time = 0;
    ui_load_screen(ui_touch_calibration_screen, false);
    application_state = TOUCH_CALIBRATION_MIN;
    state_change_time = millis();
    break;

  case TOUCH_CALIBRATION_MIN:
    ui_touch_calibration_screen_step_1();
    if (millis() - state_change_time > TOUCH_TIME_TO_CALIBRATE_EACH_POINT)
    {
      touch_set_calibration_min();
      application_state = TOUCH_CALIBRATION_MAX;
      state_change_time = millis();
    }
    break;

  case TOUCH_CALIBRATION_MAX:
    ui_touch_calibration_screen_step_2();
    if (millis() - state_change_time > TOUCH_TIME_TO_CALIBRATE_EACH_POINT)
    {
      touch_set_calibration_max();
      application_state = TOUCH_CALIBRATION_UPDATE;
    }
    break;

  case TOUCH_CALIBRATION_UPDATE:
    touch_save_calibration();
    if (!touch_is_calibrated())
    {
      application_state = TOUCH_CALIBRATION_START;
    }
    else
    {
      touch_register();
      application_state = TOUCH_CALIBRATION_COMPLETE;
      state_change_time = millis();
    }
    break;

  case TOUCH_CALIBRATION_COMPLETE:
    ui_touch_calibration_screen_step_3();
    if (millis() - state_change_time > TOUCH_TIME_DISPLAYING_SUCCESS_CALIBRATION_MESSAGE)
    {
      lv_textarea_set_text(ui_pin_screen_textarea, "");
      lv_textarea_set_placeholder_text(ui_pin_screen_textarea, mfa_vault_exists() ? "Vault PIN" : "Create PIN (6-20 digits)");
      ui_load_screen(ui_pin_screen, false);
      security_ready = false;
      application_state = SECURITY_WAIT;
      reset_display_off_timer();
      ui_touch_calibration_screen_destroy();
    }
    break;
  case SECURITY_WAIT:
    if (security_ready) {
      security_ready = false;
      if (clock_is_set()) { ui_totp_screen_render_totp_components(); update_totps(); ui_load_screen(ui_totp_screen, false); application_state = TOTPS_UPDATE; }
      else { clock_ready = false; show_clock_screen(); application_state = CLOCK_SETUP; }
    }
    break;
  case CLOCK_SETUP:
    if (clock_ready)
    {
      ui_totp_screen_render_totp_components();
      update_totps();
      ui_totp_screen_update_totp_labels();
      ui_totp_screen_update_totp_countdowns();
      lock_requested = false;
      last_security_activity = millis();
      ui_load_screen(ui_totp_screen, true);
      reset_display_off_timer();
      application_state = TOTPS_UPDATE;
    }
    break;

  case TOTPS_UPDATE:
    if (lock_requested || (device_settings.auto_lock && millis() - last_security_activity >= device_settings.auto_lock * 1000UL)) {
      lock_requested = false; mfa_lock_vault(); lv_textarea_set_text(ui_pin_screen_textarea, "");
      lv_textarea_set_placeholder_text(ui_pin_screen_textarea, "Vault PIN"); ui_load_screen(ui_pin_screen, false);
      application_state = SECURITY_WAIT; break;
    }
    if (clock_edit_requested)
    {
      clock_edit_requested = false;
      clock_ready = false;
      show_clock_screen();
      application_state = CLOCK_SETUP;
      break;
    }
    check_display_timeout();
    int current_second = get_second();
    static unsigned long previous_second = 0;
    if (current_second != previous_second)
    {
      update_totps();
      ui_totp_screen_update_totp_labels();
      ui_totp_screen_update_totp_countdowns();
      previous_second = current_second;
    }
    break;
  }
  usb_manager_poll();
  ui_task_handler();
}
