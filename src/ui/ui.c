#include "ui.h"

lv_obj_t *ui_totp_screen;
lv_obj_t *ui_pin_screen;
lv_obj_t *ui_touch_calibration_screen;
lv_obj_t *ui_pin_screen_textarea;
lv_obj_t *ui_touch_calibration_screen_label;
lv_obj_t *ui_touch_calibration_screen_dot_top_left;
lv_obj_t *ui_touch_calibration_screen_dot_bottom_right;
static lv_obj_t *ui_loading_overlay;
void ui_totp_screen_init(void);
void ui_pin_screen_init(void);
void ui_totp_screen_update_totp_labels();
void ui_totp_screen_update_totp_countdowns();
void ui_totp_screen_render_totp_components();
void ui_touch_calibration_screen_init(void);
void ui_touch_calibration_screen_step_2();
void ui_touch_calibration_screen_step_3();
void ui_touch_calibration_screen_destroy();

#if LV_COLOR_DEPTH != 16
#error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif

void ui_event_totp_screen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_GESTURE)
    {
        on_totp_screen_gesture(e);
    }
}

void ui_event_pin_screen_keyboard_button(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_VALUE_CHANGED)
    {
        on_pin_screen_keyboard_button_clicked(e);
    }
}

void ui_event_pin_screen_textarea(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_READY)
    {
        on_pin_screen_form_submit(e);
    }
}

void init_ui(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        disp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,
        LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);
    ui_touch_calibration_screen_init();
    ui_totp_screen_init();
    ui_pin_screen_init();
}

void ui_load_screen(lv_obj_t *screen, bool backward)
{
    if (!screen || lv_scr_act() == screen)
        return;
    if (ui_reduced_motion_enabled())
    {
        lv_scr_load(screen);
        return;
    }
    lv_scr_load_anim(screen,
                     backward ? LV_SCR_LOAD_ANIM_OVER_RIGHT : LV_SCR_LOAD_ANIM_OVER_LEFT,
                     180, 0, false);
}

void ui_animate_in(lv_obj_t *object, uint32_t delay)
{
    if (!object)
        return;
    if (ui_reduced_motion_enabled())
        return;
    lv_obj_fade_in(object, 180, delay);
}

void ui_show_loading(const char *message)
{
    ui_hide_loading();
    ui_loading_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ui_loading_overlay);
    lv_obj_set_size(ui_loading_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ui_loading_overlay, lv_color_make(4, 10, 18), 0);
    lv_obj_set_style_bg_opa(ui_loading_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(ui_loading_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(ui_loading_overlay);
    lv_obj_set_size(panel, 230, 104);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_bg_color(panel, lv_color_make(25, 33, 52), 0);
    lv_obj_set_style_border_color(panel, lv_color_make(5, 150, 105), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *spinner = lv_spinner_create(panel, 900, 70);
    lv_obj_set_size(spinner, 34, 34);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, -2);
    lv_obj_set_style_arc_color(spinner, lv_color_make(55, 65, 81), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, lv_color_make(16, 185, 129), LV_PART_INDICATOR);

    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text(label, message ? message : "Please wait...");
    lv_obj_set_width(label, 204);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
    ui_animate_in(panel, 0);
    lv_refr_now(NULL);
}

void ui_hide_loading(void)
{
    if (ui_loading_overlay)
    {
        lv_obj_del(ui_loading_overlay);
        ui_loading_overlay = NULL;
        lv_refr_now(NULL);
    }
}

void ui_task_handler()
{
    lv_task_handler();
}

void ui_show_error_screen(const char *title, const char *message)
{
    lv_obj_t *error_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(error_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(error_screen);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *message_label = lv_label_create(error_screen);
    lv_label_set_text(message_label, message);
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, 0);

    ui_load_screen(error_screen, false);
}
