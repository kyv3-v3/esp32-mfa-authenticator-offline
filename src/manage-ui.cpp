#include "manage-ui.h"
#include "ui/ui.h"
#include "mfa.h"
#include "services.h"
#include "display.hpp"
#include <stdint.h>
#include <string.h>

static lv_obj_t *add_screen = nullptr;
static lv_obj_t *add_title = nullptr;
static lv_obj_t *add_input = nullptr;
static lv_obj_t *add_message = nullptr;
static lv_obj_t *add_keyboard = nullptr;
static lv_obj_t *show_button = nullptr;
static lv_obj_t *show_label = nullptr;
static char service_name[MAX_SERVICE_NAME_LENGTH] = {};
static int add_stage = 0;
static int keyboard_page = 0;
static bool show_secret = false;

static lv_obj_t *manage_screen = nullptr;
static lv_obj_t *manage_list = nullptr;
static lv_obj_t *manage_message = nullptr;
static lv_obj_t *delete_buttons[MAX_NUMBER_OF_SERVICES] = {};
static int pending_delete = -1;
static lv_obj_t *edit_screen = nullptr;
static lv_obj_t *edit_title = nullptr;
static lv_obj_t *edit_values[4] = {};
static int edit_index = -1;
static uint8_t edit_group, edit_algorithm, edit_digits, edit_period;
static void render_manage_list();

static void refresh_edit_values()
{
  lv_label_set_text_fmt(edit_values[0], "GROUP: %s", mfa_group_name(edit_group));
  lv_label_set_text_fmt(edit_values[1], "ALGORITHM: SHA%s", edit_algorithm == 1 ? "1" : edit_algorithm == 2 ? "256" : "512");
  lv_label_set_text_fmt(edit_values[2], "DIGITS: %u", edit_digits);
  lv_label_set_text_fmt(edit_values[3], "PERIOD: %us", edit_period);
}

static void on_edit_action(lv_event_t *event)
{
  intptr_t action = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  if (action == 0) { ui_load_screen(manage_screen, true); return; }
  if (edit_index < 0 || edit_index >= MAX_NUMBER_OF_SERVICES) return;
  if (action == 1) { do { edit_group = (edit_group + 1) % MAX_NUMBER_OF_GROUPS; } while (!mfa_group_name(edit_group)[0]); }
  else if (action == 2) edit_algorithm = edit_algorithm % 3 + 1;
  else if (action == 3) edit_digits = edit_digits == 6 ? 8 : 6;
  else if (action == 4) edit_period = edit_period == 30 ? 60 : edit_period == 60 ? 90 : 30;
  else if (action == 5 || action == 6) { if (mfa_move_service(edit_index, action == 5 ? -1 : 1)) edit_index += action == 5 ? -1 : 1; }
  else if (action == 7) {
    Service *service = &get_services()[edit_index];
    if (mfa_edit_service(edit_index, service->name, edit_group, edit_algorithm, edit_digits, edit_period, service->type, service->counter, service->favorite)) {
      ui_totp_screen_render_totp_components(); render_manage_list(); ui_load_screen(manage_screen, true); return;
    }
  }
  refresh_edit_values();
}

static void open_edit_service(int index)
{
  Service *service = &get_services()[index]; edit_index = index; edit_group = service->group;
  edit_algorithm = service->algorithm; edit_digits = service->digits; edit_period = service->period;
  if (!edit_screen) {
    edit_screen = lv_obj_create(nullptr); lv_obj_set_style_bg_color(edit_screen, lv_color_make(12,18,30), 0); lv_obj_set_style_text_color(edit_screen, lv_color_white(), 0);
    edit_title = lv_label_create(edit_screen); lv_obj_align(edit_title, LV_ALIGN_TOP_LEFT, 8, 8);
    const char *labels[] = {"GROUP", "ALGORITHM", "DIGITS", "PERIOD", "UP", "DOWN", "SAVE", "BACK"};
    for (int i = 0; i < 8; ++i) { lv_obj_t *button = lv_btn_create(edit_screen); lv_obj_set_size(button, 148, 44); lv_obj_set_pos(button, 7 + (i % 2) * 157, 39 + (i / 2) * 49);
      lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, labels[i]); lv_obj_center(label); if (i < 4) edit_values[i] = label;
      intptr_t action = i < 7 ? i + 1 : 0; lv_obj_add_event_cb(button, on_edit_action, LV_EVENT_CLICKED, reinterpret_cast<void *>(action)); }
  }
  lv_label_set_text_fmt(edit_title, "Edit: %s", service->name); refresh_edit_values(); ui_load_screen(edit_screen, false);
}

static const char *keymaps[][19] = {
    {"A", "B", "C", "D", "E", "\n", "F", "G", "H", "I", "J", "\n", "K", "L", "DEL", "PAGE", "OK", ""},
    {"M", "N", "O", "P", "Q", "\n", "R", "S", "T", "U", "V", "\n", "W", "X", "DEL", "PAGE", "OK", ""},
    {"Y", "Z", "0", "1", "2", "\n", "3", "4", "5", "6", "7", "\n", "8", "9", "DEL", "PAGE", "OK", ""}};

static void update_add_message()
{
  if (add_stage == 0)
    lv_label_set_text_fmt(add_message, "Name, keyboard %d/3", keyboard_page + 1);
  else
    lv_label_set_text_fmt(add_message, "Base32 A-Z 2-7, %d/3", keyboard_page + 1);
}

static void reset_add_form()
{
  memset(service_name, 0, sizeof(service_name));
  add_stage = 0;
  keyboard_page = 0;
  show_secret = false;
  lv_label_set_text(show_label, "SHOW");
  lv_label_set_text(add_title, "New service");
  lv_textarea_set_password_mode(add_input, false);
  lv_textarea_set_max_length(add_input, MAX_SERVICE_NAME_LENGTH - 1);
  lv_textarea_set_text(add_input, "");
  lv_textarea_set_placeholder_text(add_input, "Name (e.g. GITHUB)");
  lv_btnmatrix_set_map(add_keyboard, keymaps[0]);
  lv_obj_add_flag(show_button, LV_OBJ_FLAG_HIDDEN);
  update_add_message();
}

static void on_add_key(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED)
    return;
  uint32_t key = lv_btnmatrix_get_selected_btn(add_keyboard);
  if (key == LV_BTNMATRIX_BTN_NONE)
    return;
  const char *text = lv_btnmatrix_get_btn_text(add_keyboard, key);
  if (!text)
    return;

  if (strcmp(text, "DEL") == 0)
    lv_textarea_del_char(add_input);
  else if (strcmp(text, "PAGE") == 0)
  {
    keyboard_page = (keyboard_page + 1) % 3;
    lv_btnmatrix_set_map(add_keyboard, keymaps[keyboard_page]);
    update_add_message();
  }
  else if (strcmp(text, "OK") == 0)
  {
    const char *value = lv_textarea_get_text(add_input);
    if (add_stage == 0)
    {
      if (!value[0])
      {
        lv_label_set_text(add_message, "Enter a name");
        return;
      }
      strncpy(service_name, value, sizeof(service_name) - 1);
      add_stage = 1;
      lv_label_set_text(add_title, "TOTP secret");
      lv_textarea_set_text(add_input, "");
      lv_textarea_set_max_length(add_input, 128);
      lv_textarea_set_placeholder_text(add_input, "Base32 secret");
      lv_textarea_set_password_mode(add_input, true);
      lv_obj_clear_flag(show_button, LV_OBJ_FLAG_HIDDEN);
      update_add_message();
    }
    else
    {
      ui_show_loading("Saving TOTP...");
      const char *error = add_service_from_touch(service_name, value, get_active_group());
      ui_hide_loading();
      if (error)
      {
        lv_label_set_text(add_message, error);
        return;
      }
      lv_textarea_set_text(add_input, "");
      memset(service_name, 0, sizeof(service_name));
      ui_totp_screen_render_totp_components();
      update_totps();
      ui_totp_screen_update_totp_labels();
      ui_totp_screen_update_totp_countdowns();
      ui_load_screen(ui_totp_screen, true);
      reset_display_off_timer();
    }
  }
  else
  {
    lv_textarea_add_text(add_input, text);
    update_add_message();
  }
}

static void on_add_cancel(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    return;
  lv_textarea_set_text(add_input, "");
  memset(service_name, 0, sizeof(service_name));
  ui_load_screen(ui_totp_screen, true);
  reset_display_off_timer();
}

static void on_show_secret(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    return;
  show_secret = !show_secret;
  lv_textarea_set_password_mode(add_input, !show_secret);
  lv_label_set_text(show_label, show_secret ? "HIDE" : "SHOW");
}

void open_add_service_screen(void)
{
  if (!add_screen)
  {
    add_screen = lv_obj_create(nullptr);
    lv_obj_set_user_data(add_screen, (void *)"add");
    lv_obj_set_style_bg_color(add_screen, lv_color_make(12, 18, 30), 0);
    lv_obj_set_style_text_color(add_screen, lv_color_white(), 0);

    add_title = lv_label_create(add_screen);
    lv_obj_align(add_title, LV_ALIGN_TOP_LEFT, 8, 10);

    lv_obj_t *cancel = lv_btn_create(add_screen);
    lv_obj_set_size(cancel, 55, 40);
    lv_obj_align(cancel, LV_ALIGN_TOP_RIGHT, -3, 3);
    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "BACK");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel, on_add_cancel, LV_EVENT_CLICKED, nullptr);

    add_input = lv_textarea_create(add_screen);
    lv_textarea_set_one_line(add_input, true);
    lv_obj_set_size(add_input, 300, 40);
    lv_obj_align(add_input, LV_ALIGN_TOP_MID, 0, 47);

    add_message = lv_label_create(add_screen);
    lv_obj_align(add_message, LV_ALIGN_TOP_LEFT, 8, 86);

    show_button = lv_btn_create(add_screen);
    lv_obj_set_size(show_button, 65, 40);
    lv_obj_align(show_button, LV_ALIGN_TOP_RIGHT, -65, 3);
    show_label = lv_label_create(show_button);
    lv_label_set_text(show_label, "SHOW");
    lv_obj_center(show_label);
    lv_obj_add_event_cb(show_button, on_show_secret, LV_EVENT_CLICKED, nullptr);

    add_keyboard = lv_btnmatrix_create(add_screen);
    lv_obj_set_size(add_keyboard, 310, 135);
    lv_obj_align(add_keyboard, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_set_style_pad_all(add_keyboard, 2, 0);
    lv_obj_set_style_pad_row(add_keyboard, 2, 0);
    lv_obj_set_style_pad_column(add_keyboard, 3, 0);
    lv_obj_add_event_cb(add_keyboard, on_add_key, LV_EVENT_VALUE_CHANGED, nullptr);
  }
  reset_add_form();
  ui_load_screen(add_screen, false);
  reset_display_off_timer();
}

static void render_manage_list()
{
  lv_obj_clean(manage_list);
  memset(delete_buttons, 0, sizeof(delete_buttons));
  Service *services = get_services();
  int count = 0;
  for (int i = 0; i < MAX_NUMBER_OF_SERVICES; ++i)
  {
    if (!services[i].name[0])
      continue;
    lv_obj_t *row = lv_obj_create(manage_list);
    lv_obj_set_size(row, 300, 50);
    lv_obj_set_style_pad_all(row, 3, 0);
    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text_fmt(name, "%s (g%u)", services[i].name, services[i].group);
    lv_obj_set_width(name, 130);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *edit = lv_btn_create(row);
    lv_obj_set_size(edit, 64, 44);
    lv_obj_align(edit, LV_ALIGN_RIGHT_MID, -90, 0);
    lv_obj_t *edit_label = lv_label_create(edit);
    lv_label_set_text(edit_label, "EDIT"); lv_obj_center(edit_label);
    lv_obj_add_event_cb(edit, [](lv_event_t *event) { open_edit_service(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)))); }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));

    lv_obj_t *button = lv_btn_create(row);
    lv_obj_set_size(button, 84, 44);
    lv_obj_align(button, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, i == pending_delete ? "CONFIRM" : "DELETE");
    lv_obj_center(label);
    lv_obj_add_event_cb(button, [](lv_event_t *event) {
      int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
      if (index != pending_delete)
      {
        if (pending_delete >= 0 && delete_buttons[pending_delete])
          lv_label_set_text(lv_obj_get_child(delete_buttons[pending_delete], 0), "DELETE");
        pending_delete = index;
        lv_label_set_text(lv_obj_get_child(delete_buttons[index], 0), "CONFIRM");
        lv_label_set_text(manage_message, "Tap CONFIRM to delete");
        return;
      }
      Service *current = get_services();
      char name_copy[MAX_SERVICE_NAME_LENGTH];
      strcpy(name_copy, current[index].name);
      const char *error = remove_service_from_touch(name_copy, current[index].group);
      pending_delete = -1;
      if (error)
        lv_label_set_text(manage_message, error);
      else
      {
        lv_label_set_text(manage_message, "Service deleted");
        ui_totp_screen_render_totp_components();
        update_totps();
        ui_totp_screen_update_totp_labels();
        ui_totp_screen_update_totp_countdowns();
      }
      lv_async_call([](void *) { render_manage_list(); }, nullptr);
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    delete_buttons[i] = button;
    ++count;
  }
  if (!count)
  {
    lv_obj_t *empty = lv_label_create(manage_list);
    lv_label_set_text(empty, "No saved services");
  }
}

static void on_manage_back(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    return;
  pending_delete = -1;
  ui_load_screen(ui_totp_screen, true);
  reset_display_off_timer();
}

void open_manage_services_screen(void)
{
  if (!manage_screen)
  {
    manage_screen = lv_obj_create(nullptr);
    lv_obj_set_user_data(manage_screen, (void *)"manage");
    lv_obj_set_style_bg_color(manage_screen, lv_color_make(12, 18, 30), 0);
    lv_obj_set_style_text_color(manage_screen, lv_color_white(), 0);

    lv_obj_t *title = lv_label_create(manage_screen);
    lv_label_set_text(title, "Manage TOTP");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 10);

    lv_obj_t *back = lv_btn_create(manage_screen);
    lv_obj_set_size(back, 65, 42);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -3, 2);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "BACK");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, on_manage_back, LV_EVENT_CLICKED, nullptr);

    manage_message = lv_label_create(manage_screen);
    lv_label_set_text(manage_message, "Two taps to delete");
    lv_obj_align(manage_message, LV_ALIGN_TOP_LEFT, 8, 49);

    manage_list = lv_obj_create(manage_screen);
    lv_obj_set_size(manage_list, 312, 168);
    lv_obj_align(manage_list, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_pad_all(manage_list, 3, 0);
    lv_obj_set_flex_flow(manage_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(manage_list, LV_DIR_VER);
  }
  pending_delete = -1;
  lv_label_set_text(manage_message, "Two taps to delete");
  render_manage_list();
  ui_load_screen(manage_screen, false);
  reset_display_off_timer();
}
