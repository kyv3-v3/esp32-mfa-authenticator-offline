#include "display.hpp"
#include "constants.h"
#include "settings.hpp"
#include "ui/ui.h"
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <string.h>

static TFT_eSPI tft(DISPLAY_WIDTH, DISPLAY_HEIGHT);
static lv_disp_drv_t driver;
static unsigned long sleep_ms, last_activity;
static bool active;

static void set_backlight(bool on) {
  ledcWrite(PWM_CHANNEL_BCKL, on ? device_settings.brightness * PWM_MAX_BCKL / 100 : 0);
  active = on;
}

void reset_display_off_timer() {
  last_activity = millis();
  if (!active) set_backlight(true);
}

void check_display_timeout() {
  if (sleep_ms && active && millis() - last_activity >= sleep_ms) {
    set_backlight(false);
    last_activity = millis();
  }
}

void display_handle_double_touch() {
  const char *screen = (const char *)lv_obj_get_user_data(lv_scr_act());
  if (screen && !strcmp(screen, TOTP_SCREEN_NAME)) {
    set_backlight(!active);
    if (active) reset_display_off_timer();
  }
}

static void flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *colors) {
  uint32_t width = area->x2 - area->x1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, area->y2 - area->y1 + 1);
  for (int y = area->y1; y <= area->y2; ++y)
    for (int x = area->x1; x <= area->x2; ++x)
      tft.writeColor(colors[(y - area->y1) * width + x - area->x1].full, 1);
  tft.endWrite();
  lv_disp_flush_ready(display);
}

void init_display() {
  sleep_ms = device_settings.display_timeout * 1000UL;
  pinMode(TFT_BCKL, OUTPUT);
  digitalWrite(TFT_BCKL, HIGH);
  tft.begin();
  tft.setSwapBytes(true);
  tft.fillScreen(ILI9341_PINK);
  tft.setRotation(2);
  active = true;
  lv_init();

  lv_disp_drv_init(&driver);
  driver.hor_res = DISPLAY_WIDTH;
  driver.ver_res = DISPLAY_HEIGHT;
  driver.flush_cb = flush;
  driver.antialiasing = 1;
  driver.draw_buf = (lv_disp_draw_buf_t *)malloc(sizeof(lv_disp_draw_buf_t));
  void *buffer = heap_caps_malloc(sizeof(lv_color_t) * LVGL_BUFFER_PIXELS, LVGL_BUFFER_MALLOC_FLAGS);
  lv_disp_draw_buf_init(driver.draw_buf, buffer, nullptr, LVGL_BUFFER_PIXELS);
  if (!lv_disp_drv_register(&driver)) return;
  lv_obj_clean(lv_scr_act());
  ledcSetup(PWM_CHANNEL_BCKL, PWM_FREQ_BCKL, PWM_BITS_BCKL);
  ledcAttachPin(TFT_BCKL, PWM_CHANNEL_BCKL);
  reset_display_off_timer();
}

void display_apply_settings() {
  sleep_ms = device_settings.display_timeout * 1000UL;
  if (active) set_backlight(true);
  lv_disp_t *display = lv_disp_get_default();
  if (display) lv_disp_set_theme(display, lv_theme_default_init(display,
    lv_palette_main(device_settings.high_contrast ? LV_PALETTE_GREEN : LV_PALETTE_BLUE),
    lv_palette_main(LV_PALETTE_ORANGE), !device_settings.light_theme,
    device_settings.large_text ? &lv_font_montserrat_18 : LV_FONT_DEFAULT));
}
