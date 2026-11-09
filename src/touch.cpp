#include "touch.hpp"
#include "constants.h"
#include <Arduino.h>
#include <SPIFFS.h>

namespace {
constexpr uint8_t READ_Y = 0x90, READ_X = 0xD0;
struct Point { int x, y; };
struct Calibration { int x_min, x_max, y_min, y_max; } calibration{};
lv_indev_drv_t driver;
TouchCallback on_single, on_double;
bool pressed;
int releases;
uint32_t first_release;

int read_spi(uint8_t command) {
  int result = 0;
  for (int i = 7; i >= 0; --i) {
    digitalWrite(TOUCH_MOSI, command & (1 << i));
    digitalWrite(TOUCH_CLK, HIGH); delayMicroseconds(10);
    digitalWrite(TOUCH_CLK, LOW); delayMicroseconds(10);
  }
  for (int i = 11; i >= 0; --i) {
    digitalWrite(TOUCH_CLK, HIGH); delayMicroseconds(10);
    result |= digitalRead(TOUCH_MISO) << i;
    digitalWrite(TOUCH_CLK, LOW); delayMicroseconds(10);
  }
  return result;
}

Point read_touch() {
  digitalWrite(TOUCH_CS, LOW);
  int x = read_spi(READ_X), y = read_spi(READ_Y);
  digitalWrite(TOUCH_CS, HIGH);
  return {constrain(map(x, calibration.x_min, calibration.x_max, 0, DISPLAY_HEIGHT), 0, DISPLAY_HEIGHT),
          constrain(map(y, calibration.y_min, calibration.y_max, 0, DISPLAY_WIDTH), 0, DISPLAY_WIDTH)};
}

void read(lv_indev_drv_t *, lv_indev_data_t *data) {
  Point point = read_touch();
  int x = point.y, y = point.x;
  if (x < DISPLAY_WIDTH && y < DISPLAY_HEIGHT) {
    pressed = true;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    if (pressed) {
      pressed = false;
      if (!first_release) first_release = millis();
      ++releases;
    }
  }
  if (first_release && millis() - first_release > TOUCH_DOUBLE_TOUCH_INTERVAL) {
    if (releases == 2 && on_double) on_double();
    else if (releases == 1 && on_single) on_single();
    releases = 0;
    first_release = 0;
  }
}
}

void init_touch(TouchCallback single, TouchCallback twice) {
  on_single = single;
  on_double = twice;
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_CLK, LOW);
  if (touch_is_calibrated()) touch_register();
}

static void capture(int &x, int &y) {
  digitalWrite(TOUCH_CS, LOW);
  x = read_spi(READ_X);
  y = read_spi(READ_Y);
  digitalWrite(TOUCH_CS, HIGH);
}

void touch_set_calibration_min() { capture(calibration.x_min, calibration.y_min); }
void touch_set_calibration_max() { capture(calibration.x_max, calibration.y_max); }

bool touch_is_calibrated() {
  File file = SPIFFS.open(TOUCH_CALIBRATION_SPIFFS_FILE_PATH, FILE_READ);
  if (!file || !file.size()) return false;
  calibration = {file.parseInt(), 0, file.parseInt(), 0};
  calibration.x_max = file.parseInt();
  calibration.y_max = file.parseInt();
  return calibration.x_min != calibration.x_max && calibration.y_min != calibration.y_max;
}

void touch_save_calibration() {
  File file = SPIFFS.open(TOUCH_CALIBRATION_SPIFFS_FILE_PATH, FILE_WRITE);
  if (!file) return;
  file.printf("%d\n%d\n%d\n%d\n", calibration.x_min, calibration.y_min,
              calibration.x_max, calibration.y_max);
  file.flush();
}

void touch_register() {
  lv_disp_t *display = lv_disp_get_default();
  if (!display) return;
  lv_indev_drv_init(&driver);
  driver.disp = display;
  driver.type = LV_INDEV_TYPE_POINTER;
  driver.read_cb = read;
  lv_indev_drv_register(&driver);
}
