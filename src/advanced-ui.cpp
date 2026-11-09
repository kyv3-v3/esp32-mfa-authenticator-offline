#include "advanced-ui.h"
#include "ui/ui.h"
#include "mfa.h"
#include "services.h"
#include "settings.hpp"
#include "display.hpp"
#include "clock.hpp"

static lv_obj_t *screen, *status;
static bool reset_armed = false;
static const char *accessibility_profile() {
  if (device_settings.reduced_motion) return "REDUCED";
  if (device_settings.high_contrast) return "CONTRAST";
  if (device_settings.large_text) return "LARGE TEXT";
  return "STANDARD";
}
static void refresh() {
  lv_label_set_text_fmt(status, "UTC %s | Group: %s | %s\nLight %u%%  Screen %us  Lock %us",
    clock_is_set() ? "OK" : "NOT SET", mfa_group_name(get_active_group()),
    accessibility_profile(), device_settings.brightness, device_settings.display_timeout, device_settings.auto_lock);
}
static void action(lv_event_t *e) {
  intptr_t id = (intptr_t)lv_event_get_user_data(e);
  if (id == 1) {
    if (!device_settings.large_text && !device_settings.high_contrast && !device_settings.reduced_motion) device_settings.large_text = true;
    else if (device_settings.large_text) { device_settings.large_text = false; device_settings.high_contrast = true; }
    else if (device_settings.high_contrast) { device_settings.high_contrast = false; device_settings.reduced_motion = true; }
    else device_settings.reduced_motion = false;
    settings_save(); display_apply_settings(); ui_totp_screen_render_totp_components();
  }
  else if (id == 2) { device_settings.brightness = device_settings.brightness >= 100 ? 25 : device_settings.brightness + 25; settings_save(); display_apply_settings(); }
  else if (id == 3) { device_settings.light_theme = !device_settings.light_theme; settings_save(); display_apply_settings(); }
  else if (id == 4) { device_settings.display_timeout = device_settings.display_timeout == 30 ? 60 : device_settings.display_timeout == 60 ? 120 : 30; settings_save(); display_apply_settings(); }
  else if (id == 5) { device_settings.auto_lock = device_settings.auto_lock == 120 ? 300 : device_settings.auto_lock == 300 ? 600 : 120; settings_save(); }
  else if (id == 6) { int group = mfa_create_group("New group"); if (group >= 0) set_active_group(group); }
  else if (id == 7) request_device_lock();
  else if (id == 8) {
    if (!reset_armed) { reset_armed = true; lv_label_set_text(status, "Tap RESET again to erase the vault"); return; }
    mfa_factory_reset(); ESP.restart();
  } else if (id == 9) { reset_armed = false; ui_load_screen(ui_totp_screen, true); return; }
  reset_armed = false; refresh();
}
void open_advanced_menu(void) {
  if (!screen) {
    screen = lv_obj_create(nullptr); lv_obj_set_style_bg_color(screen, lv_color_make(12,18,30), 0); lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_t *title = lv_label_create(screen); lv_label_set_text(title, "Offline settings"); lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 7);
    status = lv_label_create(screen); lv_obj_set_width(status, 300); lv_obj_align(status, LV_ALIGN_TOP_LEFT, 8, 29);
    static const char *names[] = {"ACCESS", "BRIGHTNESS", "THEME", "SCREEN", "AUTO LOCK", "NEW GROUP", "LOCK", "RESET", "BACK"};
    for (int i = 0; i < 9; ++i) { lv_obj_t *b = lv_btn_create(screen); lv_obj_set_size(b, 96, 44); lv_obj_set_pos(b, 7 + (i % 3) * 102, 76 + (i / 3) * 51);
      lv_obj_set_style_radius(b, 9, 0); if (i == 7) lv_obj_set_style_bg_color(b, lv_color_make(190, 35, 50), 0);
      lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, names[i]); lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0); lv_obj_set_width(l, 86); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_center(l); lv_obj_add_event_cb(b, action, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1)); }
  }
  refresh(); ui_load_screen(screen, false); reset_display_off_timer();
}
