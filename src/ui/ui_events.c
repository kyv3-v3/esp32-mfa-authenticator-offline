#include <string.h>
#include "ui.h"
#include "services.h"
#include "constants.h"
#include "mfa.h"
#include <esp_system.h>

void on_totp_screen_gesture(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    bool group_changed = false;
    if (dir == LV_DIR_LEFT)
    {
        group_changed = change_active_group_right();
    }
    else if (dir == LV_DIR_RIGHT)
    {
        group_changed = change_active_group_left();
    }

    if (group_changed)
    {
        update_totps();
        ui_totp_screen_render_totp_components();
        ui_totp_screen_update_totp_labels();
        ui_totp_screen_update_totp_countdowns();
    }
}

void on_pin_screen_keyboard_button_clicked(lv_event_t *e)
{
    static bool reset_armed = false;
    lv_obj_t *keyboard = lv_event_get_target(e);
    uint32_t btn_id = lv_btnmatrix_get_selected_btn(keyboard);
    if (btn_id != LV_BTNMATRIX_BTN_NONE)
    {
        const char *btn_text = lv_btnmatrix_get_btn_text(keyboard, btn_id);

        if (strcmp(btn_text, "DEL") == 0)
            lv_textarea_del_char(ui_pin_screen_textarea);
        else if (strcmp(btn_text, "CLEAR") == 0)
            lv_textarea_set_text(ui_pin_screen_textarea, "");
        else if (strcmp(btn_text, "RESET") == 0) {
            if (!reset_armed) {
                reset_armed = true;
                lv_obj_center(lv_msgbox_create(NULL, "RESET", "Tap RESET again to erase all TOTP entries", NULL, true));
            } else {
                mfa_factory_reset();
                esp_restart();
            }
        }
        else if (strcmp(btn_text, "OK") == 0)
        {
            reset_armed = false;
            lv_event_send(ui_pin_screen_textarea, LV_EVENT_READY, NULL);
        }
        else {
            reset_armed = false;
            lv_textarea_add_text(ui_pin_screen_textarea, btn_text);
        }
    }
}

void on_pin_screen_form_submit(lv_event_t *e)
{
    static char first_pin[PIN_MAX_LENGTH + 1] = {};
    lv_obj_t *textarea = lv_event_get_target(e);
    const char *pin = lv_textarea_get_text(textarea);
    size_t length = strlen(pin);
    if (length < PIN_MIN_LENGTH || length > PIN_MAX_LENGTH) {
        lv_obj_center(lv_msgbox_create(NULL, "PIN", "Use 6 to 20 digits", NULL, true));
        return;
    }
    if (!mfa_vault_exists()) {
        if (!first_pin[0]) {
            strcpy(first_pin, pin); lv_textarea_set_text(textarea, "");
            lv_textarea_set_placeholder_text(textarea, "Confirm the new PIN"); return;
        }
        if (strcmp(first_pin, pin)) {
            memset(first_pin, 0, sizeof(first_pin)); lv_textarea_set_text(textarea, "");
            lv_textarea_set_placeholder_text(textarea, "Create PIN (no match)"); return;
        }
        memset(first_pin, 0, sizeof(first_pin));
        ui_show_loading("Creating encrypted vault...");
        if (!mfa_create_vault(pin)) {
            ui_hide_loading();
            lv_obj_center(lv_msgbox_create(NULL, "ERROR", "Could not create the vault", NULL, true)); return;
        }
        ui_hide_loading();
    } else {
        ui_show_loading("Unlocking vault...");
        bool unlocked = mfa_unlock_vault(pin);
        ui_hide_loading();
        if (!unlocked) {
        lv_textarea_set_text(textarea, "");
        uint32_t wait = mfa_unlock_wait_seconds();
        char message[96];
        if (wait) snprintf(message, sizeof(message), "Wrong PIN. Try again in %lu seconds.", (unsigned long)wait);
        else strcpy(message, "Wrong PIN or damaged vault");
        lv_obj_center(lv_msgbox_create(NULL, "PIN", message, NULL, true)); return;
        }
    }
    lv_textarea_set_text(textarea, "");
    notify_security_ready();
}
