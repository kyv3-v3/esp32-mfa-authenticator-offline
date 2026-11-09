#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "ui_events.h"
#include <string.h>

	extern lv_obj_t *ui_totp_screen;
	extern lv_obj_t *ui_pin_screen;
	extern lv_obj_t *ui_touch_calibration_screen;
	extern lv_obj_t *ui_pin_screen_textarea;
	extern lv_obj_t *ui_touch_calibration_screen_label;
	extern lv_obj_t *ui_touch_calibration_screen_dot_top_left;
	extern lv_obj_t *ui_touch_calibration_screen_dot_bottom_right;
	void ui_event_totp_screen(lv_event_t *e);
	void ui_event_pin_screen_keyboard_button(lv_event_t *e);
	void ui_event_pin_screen_textarea(lv_event_t *e);
	void init_ui(void);
	void ui_task_handler();
	void ui_totp_screen_update_totp_labels();
	void ui_totp_screen_update_totp_countdowns();
	void ui_totp_screen_render_totp_components();
	void ui_touch_calibration_screen_step_1();
	void ui_touch_calibration_screen_step_2();
	void ui_touch_calibration_screen_step_3();
	void ui_touch_calibration_screen_destroy();
	void ui_show_error_screen(const char *title, const char *message);
	void ui_load_screen(lv_obj_t *screen, bool backward);
	void ui_show_loading(const char *message);
	void ui_hide_loading(void);
	void ui_animate_in(lv_obj_t *object, uint32_t delay);
	bool ui_reduced_motion_enabled(void);
	bool ui_large_text_enabled(void);
	bool ui_high_contrast_enabled(void);
	void notify_security_ready(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#ifdef __cplusplus

#endif

#endif // UI_H
