#include "../ui.h"
#include "services.h"
#include "constants.h"
#include "clock.hpp"
#include "manage-ui.h"
#include "advanced-ui.h"
#include "mfa.h"
#include <stdint.h>
#include <stdio.h>

#define LV_LABEL_SCROLL_SPEED (15)

static void on_service_card(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if (index < 0 || index >= MAX_NUMBER_OF_SERVICES)
        return;
    Service *service = &get_services()[index];
    if (!service->name[0])
        return;
    if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED && service->type == SERVICE_HOTP)
    {
        if (mfa_increment_hotp(index))
        {
            update_totps();
            ui_totp_screen_update_totp_labels();
        }
    }
    else if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED)
    {
        const char *type = service->type == SERVICE_HOTP ? "HOTP" : service->type == SERVICE_STEAM ? "STEAM" : "TOTP";
        const char *algorithm = service->algorithm == 2 ? "SHA256" : service->algorithm == 3 ? "SHA512" : "SHA1";
        char details[190];
        snprintf(details, sizeof(details), "%s%s\nType: %s  Algorithm: %s\nDigits: %u  Period: %us\nGroup: %u%s",
                 service->favorite ? "FAVORITE | " : "", service->name, type, algorithm,
                 service->digits, service->period, service->group,
                 service->type == SERVICE_HOTP ? "\nShort tap: next code" : "");
        lv_obj_t *box = lv_msgbox_create(NULL, "Service details", details, NULL, true);
        lv_obj_set_width(box, 300);
        lv_obj_center(box);
    }
}

static void on_toolbar_button(lv_event_t *event)
{
    intptr_t action = (intptr_t)lv_event_get_user_data(event);
    if (action == 1)
        open_add_service_screen();
    else if (action == 2)
        open_manage_services_screen();
    else if (action == 3)
        open_advanced_menu();
    else
        request_clock_edit();
}

static void create_toolbar(void)
{
    lv_obj_t *toolbar = lv_obj_create(ui_totp_screen);
    lv_obj_set_user_data(toolbar, (void *)(intptr_t)MAX_NUMBER_OF_SERVICES);
    lv_obj_set_size(toolbar, 310, 50);
    lv_obj_set_style_pad_all(toolbar, 1, 0);
    lv_obj_set_style_pad_column(toolbar, 4, 0);
    lv_obj_set_style_bg_opa(toolbar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(toolbar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    const char *names[] = {"NEW", "MANAGE", "MENU", "TIME"};
    for (int i = 0; i < 4; ++i)
    {
        lv_obj_t *button = lv_btn_create(toolbar);
        lv_obj_set_size(button, 72, 44);
        lv_obj_set_style_radius(button, 9, 0);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, names[i]);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, on_toolbar_button, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(i + 1));
    }
}

lv_obj_t *create_totp_component(
    lv_obj_t *parent,
    const char *service,
    const char *totp,
    const int index)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_user_data(container, (void *)(intptr_t)index);
    lv_obj_set_width(container, 150);
    lv_obj_set_height(container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(container, ui_high_contrast_enabled() ? lv_color_black() : lv_color_make(25, 33, 52), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(container, ui_high_contrast_enabled() ? lv_color_white() : lv_color_make(45, 58, 80), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(container, ui_high_contrast_enabled() ? 2 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(container, 9, 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_style_pad_row(container, 6, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(container, on_service_card, LV_EVENT_ALL, (void *)(intptr_t)index);

    lv_obj_t *totp_service = lv_label_create(container);
    Service *item = &get_services()[index];
    lv_label_set_text_fmt(totp_service, "%s%s%s", item->favorite ? "FAV | " : "", service,
                          item->type == SERVICE_HOTP ? " | HOTP" : item->type == SERVICE_STEAM ? " | STEAM" : "");
    lv_obj_set_width(totp_service, LV_PCT(100));
    lv_obj_set_style_text_opa(totp_service, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(totp_service, lv_color_make(254, 254, 254), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(totp_service, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(totp_service, ui_large_text_enabled() ? &lv_font_montserrat_18 : &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(totp_service, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_t *totp_label = lv_label_create(container);
    lv_label_set_text(totp_label, totp);
    lv_obj_set_style_text_opa(totp_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(totp_label, lv_color_make(254, 254, 254), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(totp_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(totp_label, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *totp_countdown = lv_bar_create(container);
    lv_bar_set_range(totp_countdown, 0, 120);
    lv_bar_set_value(totp_countdown, 120, LV_ANIM_OFF);
    lv_bar_set_start_value(totp_countdown, 0, LV_ANIM_OFF);
    lv_obj_set_width(totp_countdown, LV_PCT(100));
    lv_obj_set_height(totp_countdown, 5);
    lv_obj_set_style_pad_top(totp_countdown, 2, 0);
    lv_obj_set_style_bg_color(totp_countdown, lv_color_make(37, 50, 76), LV_PART_MAIN);
    lv_obj_set_style_bg_color(totp_countdown, lv_color_make(16, 185, 129), LV_PART_INDICATOR);

    return container;
}

void ui_totp_screen_init(void)
{
    ui_totp_screen = lv_obj_create(NULL);
    const char *name = TOTP_SCREEN_NAME;
    lv_obj_set_user_data(ui_totp_screen, (void *)name);
    lv_obj_add_flag(ui_totp_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_totp_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_totp_screen, lv_color_make(12, 18, 30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_totp_screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_totp_screen, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_flex_flow(ui_totp_screen, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_totp_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_layout(ui_totp_screen, LV_LAYOUT_FLEX);

    ui_totp_screen_render_totp_components();

    lv_obj_add_event_cb(ui_totp_screen, ui_event_totp_screen, LV_EVENT_ALL, NULL);
}

void ui_totp_screen_render_totp_components(void)
{
    lv_obj_clean(ui_totp_screen);
    create_toolbar();
    Service *services = get_services();
    uint8_t active_group = get_active_group();
    int shown = 0;
    for (int i = 0; i < MAX_NUMBER_OF_SERVICES; i++)
    {
        Service *service = &services[i];
        if (service->name[0] != '\0' && service->group == active_group)
        {
            lv_obj_t *card = create_totp_component(ui_totp_screen, service->name, service->totp, i);
            ui_animate_in(card, 35U * shown);
            ++shown;
        }
    }
    if (!shown)
    {
        lv_obj_t *empty = lv_label_create(ui_totp_screen);
        lv_obj_set_user_data(empty, (void *)(intptr_t)MAX_NUMBER_OF_SERVICES);
        lv_label_set_text(empty, "No saved TOTP entries.\nTap NEW to begin.");
        lv_obj_set_width(empty, 300);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(empty, lv_color_make(148, 163, 184), 0);
        ui_animate_in(empty, 40);
    }
}

void ui_totp_screen_update_totp_labels()
{
    int index = 0;
    lv_obj_t *totp_component = lv_obj_get_child(ui_totp_screen, index);
    Service *services = get_services();
    while (totp_component)
    {
        intptr_t service_index = (intptr_t)lv_obj_get_user_data(totp_component);
        if (service_index >= 0 && service_index < MAX_NUMBER_OF_SERVICES)
        {
            Service *service = &services[service_index];
            lv_obj_t *totp_label = lv_obj_get_child(totp_component, 1);
            lv_label_set_text(totp_label, service->totp);
        }
        index++;
        totp_component = lv_obj_get_child(ui_totp_screen, index);
    }
}

void ui_totp_screen_update_totp_countdowns()
{
    int index = 0;
    lv_obj_t *totp_component = lv_obj_get_child(ui_totp_screen, index);
    while (totp_component)
    {
        intptr_t service_index = (intptr_t)lv_obj_get_user_data(totp_component);
        if (service_index >= 0 && service_index < MAX_NUMBER_OF_SERVICES)
        {
            lv_obj_t *totp_countdown = lv_obj_get_child(totp_component, 2);
            Service *service = &get_services()[service_index];
            if (service->type == SERVICE_HOTP)
            {
                lv_bar_set_range(totp_countdown, 0, 1);
                lv_bar_set_value(totp_countdown, 1, LV_ANIM_OFF);
            }
            else
            {
                uint8_t period = service->type == SERVICE_STEAM ? 30 : service->period;
                lv_bar_set_range(totp_countdown, 0, period);
                lv_bar_set_value(totp_countdown, period - get_epoch() % period, LV_ANIM_ON);
            }
        }
        index++;
        totp_component = lv_obj_get_child(ui_totp_screen, index);
    }
}
