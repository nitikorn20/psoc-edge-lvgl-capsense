#include "wifi_manager_v2_screen.h"

#include "wifi_manager_v2_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIFI_MANAGER_V2_REFRESH_MS (500U)
#define WIFI_MANAGER_V2_STATUS_BUF (128U)
#define WIFI_MANAGER_V2_USE_FIXED_PASS_DEFAULT (0U)
#define WIFI_MANAGER_V2_FIXED_PASSWORD "@Nitikorn20"

typedef struct
{
  lv_timer_t *refresh_timer;
  lv_obj_t *header_status;
  lv_obj_t *header_count;
  lv_obj_t *scan_btn;
  lv_obj_t *scan_btn_label;
  lv_obj_t *cache_btn;
  lv_obj_t *disconnect_btn;
  lv_obj_t *disconnect_btn_label;
  lv_obj_t *clear_profile_btn;
  lv_obj_t *spinner;
  lv_obj_t *fixed_pass_switch;
  lv_obj_t *fixed_pass_label;
  lv_obj_t *list_cont;
  lv_obj_t *debug_ta;
  lv_obj_t *dialog_overlay;
  lv_obj_t *dialog_keyboard;
  lv_obj_t *dialog_ta;
  wifi_info_t selected_ap;
  wifi_manager_v2_snapshot_t snapshot;
  bool use_cache_view;
  bool dialog_open;
  bool use_fixed_password;
} wifi_manager_v2_ctx_t;

static wifi_manager_v2_ctx_t s_ctx;

static void dialog_textarea_event_cb(lv_event_t *e);
static void dialog_close(void);
static void fixed_pass_switch_cb(lv_event_t *e);
static void clear_profile_click_cb(lv_event_t *e);
static void render_fixed_pass_state(void);

static const char *state_to_text(uint8_t state)
{
  if ((uint8_t)IPC_WIFI_LINK_DISCONNECTED == state)
  {
    return "Disconnected";
  }
  if ((uint8_t)IPC_WIFI_LINK_CONNECTING == state)
  {
    return "Connecting";
  }
  if ((uint8_t)IPC_WIFI_LINK_CONNECTED == state)
  {
    return "Connected";
  }
  if ((uint8_t)IPC_WIFI_LINK_SCANNING == state)
  {
    return "Scanning";
  }
  if ((uint8_t)IPC_WIFI_LINK_ERROR == state)
  {
    return "Error";
  }
  return "Unknown";
}

static const char *reason_to_text(uint16_t reason)
{
  if ((uint16_t)IPC_WIFI_REASON_NONE == reason)
  {
    return "None";
  }
  if ((uint16_t)IPC_WIFI_REASON_SCAN_BLOCKED_CONNECTED == reason)
  {
    return "Scan blocked (connected)";
  }
  if ((uint16_t)IPC_WIFI_REASON_SCAN_FAILED == reason)
  {
    return "Scan failed";
  }
  if ((uint16_t)IPC_WIFI_REASON_CONNECT_FAILED == reason)
  {
    return "Connect failed";
  }
  if ((uint16_t)IPC_WIFI_REASON_DISCONNECTED == reason)
  {
    return "Disconnected";
  }
  return "Unknown";
}

static bool security_is_open(const char *security_text)
{
  if (NULL == security_text)
  {
    return false;
  }
  return (0 == strcmp(security_text, "OPEN"));
}

static lv_color_t rssi_to_color(int16_t rssi)
{
  if (rssi >= -55)
  {
    return lv_color_hex(0x1E8E3E);
  }
  if (rssi >= -70)
  {
    return lv_color_hex(0xE59F00);
  }
  return lv_color_hex(0xC92A2A);
}

static void apply_busy_state(void)
{
  bool op_busy = false;
  bool connected = false;
  bool connecting = false;
  bool scanning = false;
  uint32_t i;
  uint32_t child_count;

  op_busy = wifi_manager_v2_is_scanning() || wifi_manager_v2_is_connecting();
  connected = wifi_manager_v2_is_connected();
  connecting = wifi_manager_v2_is_connecting();
  scanning = wifi_manager_v2_is_scanning();

  if (NULL != s_ctx.scan_btn)
  {
    if (op_busy || connected || s_ctx.dialog_open)
    {
      lv_obj_add_state(s_ctx.scan_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.scan_btn, LV_STATE_DISABLED);
    }
  }

  if (NULL != s_ctx.cache_btn)
  {
    if (op_busy || connected || s_ctx.dialog_open)
    {
      lv_obj_add_state(s_ctx.cache_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.cache_btn, LV_STATE_DISABLED);
    }
  }

  if (NULL != s_ctx.disconnect_btn)
  {
    if (s_ctx.dialog_open || scanning || ((!connected) && (!connecting)))
    {
      lv_obj_add_state(s_ctx.disconnect_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.disconnect_btn, LV_STATE_DISABLED);
    }
  }

  if (NULL != s_ctx.disconnect_btn_label)
  {
    if (connecting)
    {
      lv_label_set_text(s_ctx.disconnect_btn_label, "Stop Retry");
    }
    else
    {
      lv_label_set_text(s_ctx.disconnect_btn_label, "Disconnect");
    }
  }

  if (NULL != s_ctx.clear_profile_btn)
  {
    if (op_busy || s_ctx.dialog_open)
    {
      lv_obj_add_state(s_ctx.clear_profile_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.clear_profile_btn, LV_STATE_DISABLED);
    }
  }

  if (NULL != s_ctx.scan_btn_label)
  {
    if (wifi_manager_v2_is_scanning())
    {
      lv_label_set_text(s_ctx.scan_btn_label, "Scanning...");
    }
    else if (wifi_manager_v2_is_connecting())
    {
      lv_label_set_text(s_ctx.scan_btn_label, "Connecting...");
    }
    else
    {
      lv_label_set_text(s_ctx.scan_btn_label, "Scan");
    }
  }

  if (NULL != s_ctx.spinner)
  {
    if (op_busy)
    {
      lv_obj_clear_flag(s_ctx.spinner, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(s_ctx.spinner, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (NULL != s_ctx.fixed_pass_switch)
  {
    if (wifi_manager_v2_is_connecting())
    {
      lv_obj_add_state(s_ctx.fixed_pass_switch, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.fixed_pass_switch, LV_STATE_DISABLED);
    }
  }

  if (NULL == s_ctx.list_cont)
  {
    return;
  }

  child_count = (uint32_t)lv_obj_get_child_count(s_ctx.list_cont);
  for (i = 0U; i < child_count; i++)
  {
    lv_obj_t *row = lv_obj_get_child(s_ctx.list_cont, i);
    if (NULL == row)
    {
      continue;
    }
    if (op_busy || connected || s_ctx.dialog_open)
    {
      lv_obj_add_state(row, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(row, LV_STATE_DISABLED);
    }
  }
}

static void render_header(void)
{
  char status_buf[WIFI_MANAGER_V2_STATUS_BUF];
  char count_buf[48];

  if ((NULL == s_ctx.header_status) || (NULL == s_ctx.header_count))
  {
    return;
  }

  (void)snprintf(status_buf, sizeof(status_buf), "State: %s | RSSI: %d dBm | Reason: %s",
                 state_to_text(s_ctx.snapshot.status.state),
                 (int)s_ctx.snapshot.status.rssi,
                 reason_to_text(s_ctx.snapshot.status.reason));
  lv_label_set_text(s_ctx.header_status, status_buf);
  lv_obj_set_style_text_color(s_ctx.header_status, rssi_to_color(s_ctx.snapshot.status.rssi), 0);

  if (s_ctx.use_cache_view)
  {
    (void)snprintf(count_buf, sizeof(count_buf), "Show Cached: %lu AP",
                   (unsigned long)s_ctx.snapshot.cache_count);
  }
  else
  {
    (void)snprintf(count_buf, sizeof(count_buf), "Latest Scan: %lu AP",
                   (unsigned long)s_ctx.snapshot.live_count);
  }
  lv_label_set_text(s_ctx.header_count, count_buf);
}

static void render_fixed_pass_state(void)
{
  if (NULL == s_ctx.fixed_pass_label)
  {
    return;
  }

  lv_label_set_text(s_ctx.fixed_pass_label, s_ctx.use_fixed_password ? "Fixed Pass: ON" : "Fixed Pass: OFF");
  lv_obj_set_style_text_color(s_ctx.fixed_pass_label,
                              s_ctx.use_fixed_password ? lv_color_hex(0x1E8E3E) : lv_color_hex(0x7C8B9A),
                              0);
}

static void row_clicked_cb(lv_event_t *e)
{
  uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  const wifi_info_t *list = NULL;
  uint32_t count = 0U;

  if (LV_EVENT_CLICKED != lv_event_get_code(e))
  {
    return;
  }

  if (s_ctx.use_cache_view)
  {
    list = s_ctx.snapshot.cache_list;
    count = s_ctx.snapshot.cache_count;
  }
  else
  {
    list = s_ctx.snapshot.live_list;
    count = s_ctx.snapshot.live_count;
  }

  if ((NULL == list) || (index >= count))
  {
    return;
  }

  s_ctx.selected_ap = list[index];

  if (security_is_open(s_ctx.selected_ap.security))
  {
    wifi_manager_v2_request_connect(s_ctx.selected_ap.ssid, "", s_ctx.selected_ap.security);
    wifi_manager_v2_request_status();
    return;
  }

  if (s_ctx.use_fixed_password)
  {
    wifi_manager_v2_request_connect(s_ctx.selected_ap.ssid, WIFI_MANAGER_V2_FIXED_PASSWORD, s_ctx.selected_ap.security);
    wifi_manager_v2_request_status();
    return;
  }

  if (s_ctx.dialog_open)
  {
    return;
  }

  s_ctx.dialog_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(s_ctx.dialog_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_add_flag(s_ctx.dialog_overlay, LV_OBJ_FLAG_FLOATING);
  lv_obj_move_foreground(s_ctx.dialog_overlay);
  lv_obj_set_layout(s_ctx.dialog_overlay, LV_LAYOUT_NONE);
  lv_obj_clear_flag(s_ctx.dialog_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(s_ctx.dialog_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_ctx.dialog_overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_ctx.dialog_overlay, LV_OPA_40, 0);
  lv_obj_set_style_border_width(s_ctx.dialog_overlay, 0, 0);

  {
    lv_obj_t *panel = lv_obj_create(s_ctx.dialog_overlay);
    lv_obj_t *title = lv_label_create(panel);
    lv_obj_t *hint = lv_label_create(panel);
    s_ctx.dialog_ta = lv_textarea_create(panel);

    lv_obj_set_size(panel, LV_PCT(88), 170);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);

    lv_label_set_text_fmt(title, "Password: %s", s_ctx.selected_ap.ssid);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x102A43), 0);

    lv_label_set_text(hint, "Press OK on keyboard to connect");
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x52606D), 0);

    lv_obj_set_size(s_ctx.dialog_ta, LV_PCT(100), 46);
    lv_obj_align(s_ctx.dialog_ta, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_textarea_set_one_line(s_ctx.dialog_ta, true);
    lv_textarea_set_password_mode(s_ctx.dialog_ta, true);
    lv_textarea_set_max_length(s_ctx.dialog_ta, 63U);
    lv_obj_add_state(s_ctx.dialog_ta, LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(s_ctx.dialog_ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_event_cb(s_ctx.dialog_ta, dialog_textarea_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_ctx.dialog_ta, dialog_textarea_event_cb, LV_EVENT_CANCEL, NULL);
  }

  s_ctx.dialog_keyboard = lv_keyboard_create(s_ctx.dialog_overlay);
  if (NULL == s_ctx.dialog_keyboard)
  {
    if (NULL != s_ctx.dialog_overlay)
    {
      lv_obj_delete(s_ctx.dialog_overlay);
      s_ctx.dialog_overlay = NULL;
    }
    s_ctx.dialog_ta = NULL;
    s_ctx.dialog_open = false;
    return;
  }
  lv_obj_set_size(s_ctx.dialog_keyboard, LV_PCT(100), 170);
  lv_obj_align(s_ctx.dialog_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(s_ctx.dialog_keyboard, LV_OBJ_FLAG_FLOATING);
  lv_obj_move_foreground(s_ctx.dialog_keyboard);
  lv_keyboard_set_mode(s_ctx.dialog_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(s_ctx.dialog_keyboard, s_ctx.dialog_ta);
  s_ctx.dialog_open = true;
  apply_busy_state();
}

static void render_list(void)
{
  const wifi_info_t *list = NULL;
  uint32_t count = 0U;
  uint32_t i;

  if (NULL == s_ctx.list_cont)
  {
    return;
  }

  lv_obj_clean(s_ctx.list_cont);

  if (s_ctx.use_cache_view)
  {
    list = s_ctx.snapshot.cache_list;
    count = s_ctx.snapshot.cache_count;
  }
  else
  {
    list = s_ctx.snapshot.live_list;
    count = s_ctx.snapshot.live_count;
  }

  if ((NULL == list) || (0U == count))
  {
    lv_obj_t *empty = lv_label_create(s_ctx.list_cont);
    if (s_ctx.use_cache_view)
    {
      lv_label_set_text(empty, "No cached access points.\nScan at least once first.");
    }
    else if (wifi_manager_v2_is_scanning())
    {
      lv_label_set_text(empty, "Scanning...");
    }
    else
    {
      lv_label_set_text(empty, "Tap Scan to discover Wi-Fi networks.");
    }
    lv_obj_set_style_text_color(empty, lv_color_hex(0x52606D), 0);
    apply_busy_state();
    return;
  }

  for (i = 0U; i < count; i++)
  {
    lv_obj_t *row = lv_button_create(s_ctx.list_cont);
    lv_obj_t *ssid = lv_label_create(row);
    lv_obj_t *meta = lv_label_create(row);
    const char *ssid_text = (list[i].ssid[0] != '\0') ? list[i].ssid : "(hidden)";

    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 64);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xD9E2EC), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_shadow_width(row, 6, 0);
    lv_obj_set_style_shadow_opa(row, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_label_set_text(ssid, ssid_text);
    lv_obj_align(ssid, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(ssid, lv_color_hex(0x102A43), 0);

    lv_label_set_text_fmt(meta, "%s | RSSI %ld dBm | Ch %u",
                          list[i].security,
                          (long)list[i].rssi,
                          (unsigned int)list[i].channel);
    lv_obj_align(meta, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    if (security_is_open(list[i].security))
    {
      lv_obj_set_style_text_color(meta, lv_color_hex(0x1E8E3E), 0);
    }
    else
    {
      lv_obj_set_style_text_color(meta, lv_color_hex(0x486581), 0);
    }

    lv_obj_add_event_cb(row, row_clicked_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }

  apply_busy_state();
}

static void render_debug(void)
{
  if (NULL == s_ctx.debug_ta)
  {
    return;
  }

  if ('\0' == s_ctx.snapshot.debug_text[0])
  {
    lv_textarea_set_text(s_ctx.debug_ta, "[INIT] Waiting for IPC logs...");
  }
  else
  {
    lv_textarea_set_text(s_ctx.debug_ta, s_ctx.snapshot.debug_text);
  }
  lv_textarea_set_cursor_pos(s_ctx.debug_ta, LV_TEXTAREA_CURSOR_LAST);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
  wifi_manager_v2_snapshot_t latest;

  (void)timer;

  if (s_ctx.dialog_open)
  {
    /* Avoid heavy redraw while user is typing on the software keyboard. */
    return;
  }

  wifi_manager_v2_state_poll(&latest);

  if (latest.status_changed || latest.scan_changed)
  {
    s_ctx.snapshot.status = latest.status;
    s_ctx.snapshot.live_count = latest.live_count;
    s_ctx.snapshot.cache_count = latest.cache_count;
    (void)memset(s_ctx.snapshot.live_list, 0, sizeof(s_ctx.snapshot.live_list));
    (void)memset(s_ctx.snapshot.cache_list, 0, sizeof(s_ctx.snapshot.cache_list));
    if (latest.live_count > 0U)
    {
      (void)memcpy(s_ctx.snapshot.live_list, latest.live_list, sizeof(wifi_info_t) * latest.live_count);
    }
    if (latest.cache_count > 0U)
    {
      (void)memcpy(s_ctx.snapshot.cache_list, latest.cache_list, sizeof(wifi_info_t) * latest.cache_count);
    }
  }

  if (latest.debug_changed)
  {
    (void)memcpy(s_ctx.snapshot.debug_text, latest.debug_text, sizeof(s_ctx.snapshot.debug_text));
    render_debug();
  }

  if (latest.status_changed)
  {
    render_header();
  }

  if (latest.scan_changed)
  {
    render_list();
    render_header();
  }

  apply_busy_state();
}

static void scan_click_cb(lv_event_t *e)
{
  (void)e;
  s_ctx.use_cache_view = false;
  render_header();
  wifi_manager_v2_request_scan();
}

static void cache_click_cb(lv_event_t *e)
{
  (void)e;
  s_ctx.use_cache_view = true;
  render_header();
  render_list();
}

static void disconnect_click_cb(lv_event_t *e)
{
  (void)e;
  wifi_manager_v2_request_disconnect();
}

static void clear_profile_click_cb(lv_event_t *e)
{
  (void)e;
  wifi_manager_v2_request_clear_profile();
}

static void fixed_pass_switch_cb(lv_event_t *e)
{
  if (LV_EVENT_VALUE_CHANGED != lv_event_get_code(e))
  {
    return;
  }

  if (NULL == s_ctx.fixed_pass_switch)
  {
    return;
  }

  s_ctx.use_fixed_password = lv_obj_has_state(s_ctx.fixed_pass_switch, LV_STATE_CHECKED);
  render_fixed_pass_state();
}

static void dialog_close(void)
{
  if (NULL != s_ctx.dialog_keyboard)
  {
    lv_obj_delete(s_ctx.dialog_keyboard);
    s_ctx.dialog_keyboard = NULL;
  }
  if (NULL != s_ctx.dialog_overlay)
  {
    lv_obj_delete(s_ctx.dialog_overlay);
    s_ctx.dialog_overlay = NULL;
  }
  s_ctx.dialog_ta = NULL;
  s_ctx.dialog_open = false;
  apply_busy_state();
}

static void dialog_textarea_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  const char *password = NULL;
  lv_obj_t *ta = lv_event_get_target(e);

  if (!s_ctx.dialog_open)
  {
    return;
  }

  if ((LV_EVENT_CANCEL == code) || (NULL == ta))
  {
    dialog_close();
    return;
  }

  if (LV_EVENT_READY != code)
  {
    return;
  }

  password = lv_textarea_get_text(ta);
  if (s_ctx.use_fixed_password)
  {
    password = WIFI_MANAGER_V2_FIXED_PASSWORD;
  }
  else if ((NULL == password) || ('\0' == password[0]))
  {
    return;
  }

  wifi_manager_v2_request_connect(s_ctx.selected_ap.ssid, password, s_ctx.selected_ap.security);
  dialog_close();
}

static void screen_delete_cb(lv_event_t *e)
{
  (void)e;
  if (NULL != s_ctx.refresh_timer)
  {
    lv_timer_delete(s_ctx.refresh_timer);
    s_ctx.refresh_timer = NULL;
  }
}

void wifi_manager_v2_screen_create(lv_obj_t *screen)
{
  lv_obj_t *header_card;
  lv_obj_t *control_row;
  lv_obj_t *list_card;
  lv_obj_t *debug_card;

  if (NULL == screen)
  {
    return;
  }

  (void)memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.use_fixed_password = (WIFI_MANAGER_V2_USE_FIXED_PASS_DEFAULT != 0U);
  wifi_manager_v2_state_init();
  wifi_manager_v2_state_poll(&s_ctx.snapshot);

  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xF4F7FB), 0);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 12, 0);
  lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(screen, 10, 0);

  header_card = lv_obj_create(screen);
  lv_obj_set_width(header_card, LV_PCT(100));
  lv_obj_set_height(header_card, 132);
  lv_obj_set_style_radius(header_card, 12, 0);
  lv_obj_set_style_border_width(header_card, 0, 0);
  lv_obj_set_style_pad_all(header_card, 10, 0);
  lv_obj_set_style_bg_color(header_card, lv_color_hex(0xFFFFFF), 0);

  {
    lv_obj_t *title = lv_label_create(header_card);
    lv_label_set_text(title, "Wi-Fi Manager (IPC v2 UI)");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x102A43), 0);

    s_ctx.header_status = lv_label_create(header_card);
    lv_label_set_text(s_ctx.header_status, "State: -");
    lv_obj_align(s_ctx.header_status, LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_set_style_text_color(s_ctx.header_status, lv_color_hex(0x334E68), 0);

    s_ctx.header_count = lv_label_create(header_card);
    lv_label_set_text(s_ctx.header_count, "Latest Scan: 0 AP");
    lv_obj_align(s_ctx.header_count, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_color(s_ctx.header_count, lv_color_hex(0x486581), 0);

    s_ctx.fixed_pass_switch = lv_switch_create(header_card);
    lv_obj_align(s_ctx.fixed_pass_switch, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(s_ctx.fixed_pass_switch, fixed_pass_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (s_ctx.use_fixed_password)
    {
      lv_obj_add_state(s_ctx.fixed_pass_switch, LV_STATE_CHECKED);
    }

    s_ctx.fixed_pass_label = lv_label_create(header_card);
    lv_obj_align_to(s_ctx.fixed_pass_label, s_ctx.fixed_pass_switch, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    lv_obj_set_style_text_font(s_ctx.fixed_pass_label, LV_FONT_DEFAULT, 0);

    s_ctx.clear_profile_btn = lv_button_create(header_card);
    lv_obj_set_size(s_ctx.clear_profile_btn, 190, 34);
    lv_obj_align(s_ctx.clear_profile_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_ctx.clear_profile_btn, lv_color_hex(0xEDE9FE), 0);
    lv_obj_set_style_radius(s_ctx.clear_profile_btn, 10, 0);
    lv_obj_add_event_cb(s_ctx.clear_profile_btn, clear_profile_click_cb, LV_EVENT_CLICKED, NULL);
    {
      lv_obj_t *label = lv_label_create(s_ctx.clear_profile_btn);
      lv_label_set_text(label, "Clear Saved Wi-Fi");
      lv_obj_set_style_text_color(label, lv_color_hex(0x4338CA), 0);
      lv_obj_center(label);
    }
  }

  control_row = lv_obj_create(screen);
  lv_obj_set_width(control_row, LV_PCT(100));
  lv_obj_set_height(control_row, 58);
  lv_obj_set_style_bg_opa(control_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(control_row, 0, 0);
  lv_obj_set_style_pad_all(control_row, 0, 0);
  lv_obj_set_layout(control_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(control_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(control_row, 8, 0);

  s_ctx.scan_btn = lv_button_create(control_row);
  lv_obj_set_size(s_ctx.scan_btn, 150, 46);
  lv_obj_set_style_bg_color(s_ctx.scan_btn, lv_color_hex(0x1A73E8), 0);
  lv_obj_set_style_radius(s_ctx.scan_btn, 12, 0);
  lv_obj_add_event_cb(s_ctx.scan_btn, scan_click_cb, LV_EVENT_CLICKED, NULL);
  s_ctx.scan_btn_label = lv_label_create(s_ctx.scan_btn);
  lv_label_set_text(s_ctx.scan_btn_label, "Scan");
  lv_obj_set_style_text_color(s_ctx.scan_btn_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(s_ctx.scan_btn_label);

  s_ctx.cache_btn = lv_button_create(control_row);
  lv_obj_set_size(s_ctx.cache_btn, 150, 46);
  lv_obj_set_style_bg_color(s_ctx.cache_btn, lv_color_hex(0xDDEAFB), 0);
  lv_obj_set_style_radius(s_ctx.cache_btn, 12, 0);
  lv_obj_add_event_cb(s_ctx.cache_btn, cache_click_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *label = lv_label_create(s_ctx.cache_btn);
    lv_label_set_text(label, "Show Cached");
    lv_obj_set_style_text_color(label, lv_color_hex(0x1A73E8), 0);
    lv_obj_center(label);
  }

  s_ctx.disconnect_btn = lv_button_create(control_row);
  lv_obj_set_size(s_ctx.disconnect_btn, 130, 46);
  lv_obj_set_style_bg_color(s_ctx.disconnect_btn, lv_color_hex(0xFDE8E7), 0);
  lv_obj_set_style_radius(s_ctx.disconnect_btn, 12, 0);
  lv_obj_add_event_cb(s_ctx.disconnect_btn, disconnect_click_cb, LV_EVENT_CLICKED, NULL);
  s_ctx.disconnect_btn_label = lv_label_create(s_ctx.disconnect_btn);
  lv_label_set_text(s_ctx.disconnect_btn_label, "Disconnect");
  lv_obj_set_style_text_color(s_ctx.disconnect_btn_label, lv_color_hex(0xB42318), 0);
  lv_obj_center(s_ctx.disconnect_btn_label);

  s_ctx.spinner = lv_spinner_create(control_row);
  lv_obj_set_size(s_ctx.spinner, 36, 36);
  lv_spinner_set_anim_params(s_ctx.spinner, 900U, 120U);
  lv_obj_add_flag(s_ctx.spinner, LV_OBJ_FLAG_HIDDEN);

  list_card = lv_obj_create(screen);
  lv_obj_set_width(list_card, LV_PCT(100));
  lv_obj_set_height(list_card, LV_PCT(42));
  lv_obj_set_style_radius(list_card, 12, 0);
  lv_obj_set_style_border_width(list_card, 0, 0);
  lv_obj_set_style_pad_all(list_card, 10, 0);
  lv_obj_set_style_bg_color(list_card, lv_color_hex(0xFFFFFF), 0);

  s_ctx.list_cont = lv_obj_create(list_card);
  lv_obj_set_size(s_ctx.list_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(s_ctx.list_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_ctx.list_cont, 0, 0);
  lv_obj_set_style_pad_all(s_ctx.list_cont, 0, 0);
  lv_obj_set_style_pad_row(s_ctx.list_cont, 8, 0);
  lv_obj_set_layout(s_ctx.list_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(s_ctx.list_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(s_ctx.list_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_ctx.list_cont, LV_SCROLLBAR_MODE_AUTO);

  debug_card = lv_obj_create(screen);
  lv_obj_set_width(debug_card, LV_PCT(100));
  lv_obj_set_flex_grow(debug_card, 2);
  lv_obj_set_style_min_height(debug_card, 190, 0);
  lv_obj_set_style_radius(debug_card, 12, 0);
  lv_obj_set_style_border_width(debug_card, 0, 0);
  lv_obj_set_style_pad_all(debug_card, 10, 0);
  lv_obj_set_style_bg_color(debug_card, lv_color_hex(0xFFFFFF), 0);

  s_ctx.debug_ta = lv_textarea_create(debug_card);
  lv_obj_set_size(s_ctx.debug_ta, LV_PCT(100), LV_PCT(100));
  lv_textarea_set_one_line(s_ctx.debug_ta, false);
  lv_textarea_set_cursor_click_pos(s_ctx.debug_ta, false);
  lv_obj_set_scrollbar_mode(s_ctx.debug_ta, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_text_color(s_ctx.debug_ta, lv_color_hex(0x334E68), 0);

  lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

  s_ctx.refresh_timer = lv_timer_create(refresh_timer_cb, WIFI_MANAGER_V2_REFRESH_MS, NULL);

  render_header();
  render_fixed_pass_state();
  render_list();
  render_debug();
  apply_busy_state();
  wifi_manager_v2_request_status();
}
