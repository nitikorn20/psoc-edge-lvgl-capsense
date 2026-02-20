#include "sensorhub_v1_screen.h"

#include "sensorhub_v1_state.h"
#include "components/bmi270/sensorhub_v1_bmi270_component.h"
#include "components/bmm350/sensorhub_v1_bmm350_component.h"
#include "components/capsense_touch/sensorhub_v1_capsense_touch_component.h"
#include "components/dashboard/sensorhub_v1_dashboard_component.h"
#include "components/pot/sensorhub_v1_pot_component.h"
#include "cm55_ipc_app.h"
#include "user_buttons_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SENSORHUB_V1_REFRESH_MS (33U)
#define SENSORHUB_V1_DEFAULT_PERIOD_MS (120U)
#define SENSORHUB_V1_DEFAULT_MASK                                                                                         \
  (IPC_SENSORHUB_SENSOR_MASK_POT | IPC_SENSORHUB_SENSOR_MASK_BMI270 | IPC_SENSORHUB_SENSOR_MASK_CAPSENSE |            \
   IPC_SENSORHUB_SENSOR_MASK_BMM350)
#define SENSORHUB_V1_AUTO_START_ON_CREATE (1U)
#define SENSORHUB_V1_DISPLAY_ONLY_MODE (1U)
#define SENSORHUB_V1_TAB_BAR_SIZE (46)
#define SENSORHUB_V1_BUTTON_TAB_NAV_ENABLE (1U)
#define SENSORHUB_V1_TAB_DASHBOARD (0U)
#define SENSORHUB_V1_TAB_POT (1U)
#define SENSORHUB_V1_TAB_BMI270 (2U)
#define SENSORHUB_V1_TAB_CAPSENSE (3U)
#define SENSORHUB_V1_TAB_BMM350 (4U)
#define SENSORHUB_V1_TAB_COUNT (5U)
#define SENSORHUB_V1_BUTTON_NAV_MIN_INTERVAL_MS (120U)

typedef struct
{
  lv_timer_t *refresh_timer;
  lv_obj_t *status_label;
  lv_obj_t *start_btn;
  lv_obj_t *stop_btn;
  lv_obj_t *tabview;
  uint32_t active_tab;
  uint32_t button_last_press_count[BUTTON_ID_MAX];
  uint32_t button_last_nav_ms[BUTTON_ID_MAX];
  bool button_press_latched[BUTTON_ID_MAX];

  sensorhub_v1_dashboard_component_t dashboard;
  sensorhub_v1_pot_component_t pot;
  sensorhub_v1_bmi270_component_t bmi;
  sensorhub_v1_capsense_touch_component_t capsense_touch;
  sensorhub_v1_bmm350_component_t bmm;

  sensorhub_v1_snapshot_t snapshot;
} sensorhub_v1_ctx_t;

static sensorhub_v1_ctx_t s_ctx;

static const char *sensorhub_state_text(uint8_t state)
{
  if ((uint8_t)IPC_SENSORHUB_STATE_STOPPED == state)
  {
    return "Stopped";
  }
  if ((uint8_t)IPC_SENSORHUB_STATE_RUNNING == state)
  {
    return "Running";
  }
  if ((uint8_t)IPC_SENSORHUB_STATE_ERROR == state)
  {
    return "Error";
  }
  return "Unknown";
}

static const char *sensorhub_reason_text(uint16_t reason)
{
  if ((uint16_t)IPC_SENSORHUB_REASON_NONE == reason)
  {
    return "None";
  }
  if ((uint16_t)IPC_SENSORHUB_REASON_STARTED == reason)
  {
    return "Started";
  }
  if ((uint16_t)IPC_SENSORHUB_REASON_STOPPED == reason)
  {
    return "Stopped";
  }
  if ((uint16_t)IPC_SENSORHUB_REASON_ERROR == reason)
  {
    return "Error";
  }
  return "Unknown";
}

static void sensorhub_mask_text(uint8_t mask, char *buf, uint32_t buf_size)
{
  uint32_t pos = 0U;

  if ((NULL == buf) || (0U == buf_size))
  {
    return;
  }

  buf[0] = '\0';
  if (0U == mask)
  {
    (void)snprintf(buf, buf_size, "None");
    return;
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_POT))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "POT");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMI270))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sBMI270", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_CAPSENSE))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sCAP", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_TOUCH))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sTOUCH", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMM350))
  {
    (void)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sBMM350", (pos > 0U) ? "+" : "");
  }
}

static void render_status(void)
{
  char line[180];
  char mask_text[40];
  bool running;

  if (NULL == s_ctx.status_label)
  {
    return;
  }

  sensorhub_mask_text(s_ctx.snapshot.status.sensors_mask, mask_text, sizeof(mask_text));
  (void)snprintf(line, sizeof(line), "State: %s | Sensors: %s | Reason: %s",
                 sensorhub_state_text(s_ctx.snapshot.status.state),
                 mask_text,
                 sensorhub_reason_text(s_ctx.snapshot.status.reason));
  lv_label_set_text(s_ctx.status_label, line);
  lv_obj_set_style_text_color(
      s_ctx.status_label,
      ((uint8_t)IPC_SENSORHUB_STATE_RUNNING == s_ctx.snapshot.status.state) ? lv_color_hex(0x1E8E3E) : lv_color_hex(0x334E68), 0);

  running = ((uint8_t)IPC_SENSORHUB_STATE_RUNNING == s_ctx.snapshot.status.state);
  if (NULL != s_ctx.start_btn)
  {
    if (running)
    {
      lv_obj_add_state(s_ctx.start_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_clear_state(s_ctx.start_btn, LV_STATE_DISABLED);
    }
  }
  if (NULL != s_ctx.stop_btn)
  {
    if (running)
    {
      lv_obj_clear_state(s_ctx.stop_btn, LV_STATE_DISABLED);
    }
    else
    {
      lv_obj_add_state(s_ctx.stop_btn, LV_STATE_DISABLED);
    }
  }
}

#if (SENSORHUB_V1_DISPLAY_ONLY_MODE == 0U)
static void start_click_cb(lv_event_t *e)
{
  (void)e;
  sensorhub_v1_request_start(SENSORHUB_V1_DEFAULT_MASK, SENSORHUB_V1_DEFAULT_PERIOD_MS);
}

static void stop_click_cb(lv_event_t *e)
{
  (void)e;
  sensorhub_v1_request_stop();
}
#endif

#if (SENSORHUB_V1_BUTTON_TAB_NAV_ENABLE != 0U)
static void set_tab_active(uint32_t tab_id)
{
  if (NULL == s_ctx.tabview)
  {
    return;
  }
  lv_tabview_set_active(s_ctx.tabview, tab_id, LV_ANIM_ON);
  s_ctx.active_tab = tab_id;
}

static uint32_t get_current_tab(void)
{
  uint32_t tab_count;
  uint32_t active;

  if (NULL == s_ctx.tabview)
  {
    return 0U;
  }

  tab_count = lv_tabview_get_tab_count(s_ctx.tabview);
  if (0U == tab_count)
  {
    return 0U;
  }

  active = lv_tabview_get_tab_active(s_ctx.tabview);
  if (active < tab_count)
  {
    return active;
  }

  if (s_ctx.active_tab < tab_count)
  {
    return s_ctx.active_tab;
  }
  return 0U;
}

static void switch_tab_relative(int32_t delta)
{
  uint32_t tab_count;
  int32_t next_tab;
  uint32_t current_tab;

  if ((NULL == s_ctx.tabview) || (0 == delta))
  {
    return;
  }

  tab_count = lv_tabview_get_tab_count(s_ctx.tabview);
  if (0U == tab_count)
  {
    return;
  }

  current_tab = get_current_tab();
  next_tab = (int32_t)current_tab + delta;
  while (next_tab < 0)
  {
    next_tab += (int32_t)tab_count;
  }
  while ((uint32_t)next_tab >= tab_count)
  {
    next_tab -= (int32_t)tab_count;
  }
  set_tab_active((uint32_t)next_tab);
}

static void poll_board_buttons_for_tab_navigation(void)
{
  uint32_t now_ms;
  uint32_t elapsed_ms;
  uint32_t press_count;
  bool pressed;
  bool count_changed;

  now_ms = lv_tick_get();

  if (cm55_get_button_state((uint32_t)BUTTON_ID_0, &press_count, &pressed))
  {
    count_changed = (press_count > s_ctx.button_last_press_count[BUTTON_ID_0]);

    /* Arm on press-down edge; actual tab switch is handled on release. */
    if (pressed && !s_ctx.button_press_latched[BUTTON_ID_0])
    {
      s_ctx.button_press_latched[BUTTON_ID_0] = true;
    }

    if (!pressed && s_ctx.button_press_latched[BUTTON_ID_0])
    {
      s_ctx.button_press_latched[BUTTON_ID_0] = false;
      if (count_changed)
      {
        s_ctx.button_last_press_count[BUTTON_ID_0] = press_count;
        elapsed_ms = now_ms - s_ctx.button_last_nav_ms[BUTTON_ID_0];
        if (elapsed_ms >= SENSORHUB_V1_BUTTON_NAV_MIN_INTERVAL_MS)
        {
          switch_tab_relative(-1);
          s_ctx.button_last_nav_ms[BUTTON_ID_0] = now_ms;
        }
      }
    }
    else if (!pressed && !s_ctx.button_press_latched[BUTTON_ID_0] && count_changed)
    {
      /* Fallback for very short tap between polling intervals. */
      s_ctx.button_last_press_count[BUTTON_ID_0] = press_count;
      elapsed_ms = now_ms - s_ctx.button_last_nav_ms[BUTTON_ID_0];
      if (elapsed_ms >= SENSORHUB_V1_BUTTON_NAV_MIN_INTERVAL_MS)
      {
        switch_tab_relative(-1);
        s_ctx.button_last_nav_ms[BUTTON_ID_0] = now_ms;
      }
    }
  }

  if (cm55_get_button_state((uint32_t)BUTTON_ID_1, &press_count, &pressed))
  {
    count_changed = (press_count > s_ctx.button_last_press_count[BUTTON_ID_1]);

    /* Arm on press-down edge; actual tab switch is handled on release. */
    if (pressed && !s_ctx.button_press_latched[BUTTON_ID_1])
    {
      s_ctx.button_press_latched[BUTTON_ID_1] = true;
    }

    if (!pressed && s_ctx.button_press_latched[BUTTON_ID_1])
    {
      s_ctx.button_press_latched[BUTTON_ID_1] = false;
      if (count_changed)
      {
        s_ctx.button_last_press_count[BUTTON_ID_1] = press_count;
        elapsed_ms = now_ms - s_ctx.button_last_nav_ms[BUTTON_ID_1];
        if (elapsed_ms >= SENSORHUB_V1_BUTTON_NAV_MIN_INTERVAL_MS)
        {
          switch_tab_relative(1);
          s_ctx.button_last_nav_ms[BUTTON_ID_1] = now_ms;
        }
      }
    }
    else if (!pressed && !s_ctx.button_press_latched[BUTTON_ID_1] && count_changed)
    {
      /* Fallback for very short tap between polling intervals. */
      s_ctx.button_last_press_count[BUTTON_ID_1] = press_count;
      elapsed_ms = now_ms - s_ctx.button_last_nav_ms[BUTTON_ID_1];
      if (elapsed_ms >= SENSORHUB_V1_BUTTON_NAV_MIN_INTERVAL_MS)
      {
        switch_tab_relative(1);
        s_ctx.button_last_nav_ms[BUTTON_ID_1] = now_ms;
      }
    }
  }
}
#endif

static void refresh_timer_cb(lv_timer_t *timer)
{
  sensorhub_v1_snapshot_t latest;
  uint32_t active_tab = SENSORHUB_V1_TAB_DASHBOARD;
  bool tab_changed;
  bool dashboard_dirty;
  (void)timer;

  sensorhub_v1_state_poll(&latest);
  s_ctx.snapshot = latest;

#if (SENSORHUB_V1_BUTTON_TAB_NAV_ENABLE != 0U)
  poll_board_buttons_for_tab_navigation();
#endif

  if (NULL != s_ctx.tabview)
  {
    active_tab = lv_tabview_get_tab_active(s_ctx.tabview);
    if (active_tab >= SENSORHUB_V1_TAB_COUNT)
    {
      active_tab = SENSORHUB_V1_TAB_DASHBOARD;
    }
  }

  tab_changed = (active_tab != s_ctx.active_tab);
  s_ctx.active_tab = active_tab;

  if (s_ctx.snapshot.status_changed || tab_changed)
  {
    render_status();
  }

  switch (active_tab)
  {
  case SENSORHUB_V1_TAB_DASHBOARD:
    dashboard_dirty = s_ctx.snapshot.status_changed || s_ctx.snapshot.pot_changed || s_ctx.snapshot.bmi270_changed ||
                      s_ctx.snapshot.capsense_changed || s_ctx.snapshot.touch_changed || s_ctx.snapshot.bmm350_changed ||
                      s_ctx.snapshot.debug_changed || tab_changed;
    if (dashboard_dirty)
    {
      sensorhub_v1_dashboard_component_render(&s_ctx.dashboard, &s_ctx.snapshot);
    }
    break;

  case SENSORHUB_V1_TAB_POT:
    if (s_ctx.snapshot.status_changed || s_ctx.snapshot.pot_changed || tab_changed)
    {
      sensorhub_v1_pot_component_render(&s_ctx.pot, &s_ctx.snapshot);
    }
    break;

  case SENSORHUB_V1_TAB_BMI270:
    if (s_ctx.snapshot.status_changed || s_ctx.snapshot.bmi270_changed || tab_changed)
    {
      sensorhub_v1_bmi270_component_render(&s_ctx.bmi, &s_ctx.snapshot);
    }
    break;

  case SENSORHUB_V1_TAB_CAPSENSE:
    if (s_ctx.snapshot.status_changed || s_ctx.snapshot.capsense_changed || tab_changed)
    {
      sensorhub_v1_capsense_touch_component_render(&s_ctx.capsense_touch, &s_ctx.snapshot);
    }
    break;

  case SENSORHUB_V1_TAB_BMM350:
    if (s_ctx.snapshot.status_changed || s_ctx.snapshot.bmm350_changed || tab_changed)
    {
      sensorhub_v1_bmm350_component_render(&s_ctx.bmm, &s_ctx.snapshot);
    }
    break;

  default:
    break;
  }
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

void sensorhub_v1_screen_create(lv_obj_t *screen)
{
  lv_obj_t *header_card;
  lv_obj_t *control_row;
  lv_obj_t *title;
  lv_obj_t *tv;
  lv_obj_t *tab_dashboard;
  lv_obj_t *tab_pot;
  lv_obj_t *tab_bmi;
  lv_obj_t *tab_cap;
  lv_obj_t *tab_bmm;
  lv_obj_t *tv_content;

  if (NULL == screen)
  {
    return;
  }

  (void)memset(&s_ctx, 0, sizeof(s_ctx));
  sensorhub_v1_state_init();
  sensorhub_v1_state_poll(&s_ctx.snapshot);

  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xF4F7FB), 0);
  lv_obj_set_style_pad_all(screen, 10, 0);
  lv_obj_set_style_pad_row(screen, 10, 0);
  lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

  header_card = lv_obj_create(screen);
  lv_obj_set_width(header_card, LV_PCT(100));
  lv_obj_set_height(header_card, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(header_card, 14, 0);
  lv_obj_set_style_border_width(header_card, 1, 0);
  lv_obj_set_style_border_color(header_card, lv_color_hex(0xD9E4F2), 0);
  lv_obj_set_style_pad_all(header_card, 10, 0);
  lv_obj_set_style_pad_row(header_card, 8, 0);
  lv_obj_set_style_bg_color(header_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_layout(header_card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(header_card, LV_FLEX_FLOW_COLUMN);

  title = lv_label_create(header_card);
  lv_label_set_text(title, "SensorHub v1 - Multi Sensor Dashboard");
  lv_obj_set_style_text_color(title, lv_color_hex(0x102A43), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  s_ctx.status_label = lv_label_create(header_card);
  lv_obj_set_style_text_font(s_ctx.status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_ctx.status_label, lv_color_hex(0x334E68), 0);

  control_row = lv_obj_create(header_card);
  lv_obj_set_width(control_row, LV_PCT(100));
  lv_obj_set_height(control_row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(control_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(control_row, 0, 0);
  lv_obj_set_style_pad_all(control_row, 0, 0);
  lv_obj_set_layout(control_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(control_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(control_row, 10, 0);

#if (SENSORHUB_V1_DISPLAY_ONLY_MODE != 0U)
  {
    lv_obj_t *auto_label = lv_label_create(control_row);
    lv_label_set_text(auto_label, "Auto mode | BTN0 Prev Tab | BTN1 Next Tab");
    lv_obj_set_style_text_color(auto_label, lv_color_hex(0x627D98), 0);
    lv_obj_set_style_text_font(auto_label, &lv_font_montserrat_14, 0);
  }
#else
  s_ctx.start_btn = lv_button_create(control_row);
  lv_obj_set_size(s_ctx.start_btn, 130, 42);
  lv_obj_set_style_bg_color(s_ctx.start_btn, lv_color_hex(0x1A73E8), 0);
  lv_obj_set_style_radius(s_ctx.start_btn, 10, 0);
  lv_obj_add_event_cb(s_ctx.start_btn, start_click_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *label = lv_label_create(s_ctx.start_btn);
    lv_label_set_text(label, "Start");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);
  }

  s_ctx.stop_btn = lv_button_create(control_row);
  lv_obj_set_size(s_ctx.stop_btn, 110, 42);
  lv_obj_set_style_bg_color(s_ctx.stop_btn, lv_color_hex(0xFDE8E7), 0);
  lv_obj_set_style_radius(s_ctx.stop_btn, 10, 0);
  lv_obj_add_event_cb(s_ctx.stop_btn, stop_click_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *label = lv_label_create(s_ctx.stop_btn);
    lv_label_set_text(label, "Stop");
    lv_obj_set_style_text_color(label, lv_color_hex(0xC5221F), 0);
    lv_obj_center(label);
  }
#endif

  tv = lv_tabview_create(screen);
  s_ctx.tabview = tv;
  s_ctx.active_tab = 0U;
  lv_obj_set_width(tv, LV_PCT(100));
  lv_obj_set_flex_grow(tv, 1);
  lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
  lv_tabview_set_tab_bar_size(tv, SENSORHUB_V1_TAB_BAR_SIZE);
  lv_obj_set_style_radius(tv, 14, LV_PART_MAIN);
  lv_obj_set_style_border_width(tv, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(tv, lv_color_hex(0xD9E4F2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(tv, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  tv_content = lv_tabview_get_content(tv);
  if (NULL != tv_content)
  {
    lv_obj_set_style_pad_top(tv_content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(tv_content, 8, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(tv_content, LV_SCROLLBAR_MODE_OFF);
  }

  tab_dashboard = lv_tabview_add_tab(tv, "Dashboard");
  tab_pot = lv_tabview_add_tab(tv, "POT");
  tab_bmi = lv_tabview_add_tab(tv, "BMI270");
  tab_cap = lv_tabview_add_tab(tv, "CAPSENSE");
  tab_bmm = lv_tabview_add_tab(tv, "BMM350");

  sensorhub_v1_dashboard_component_create(&s_ctx.dashboard, tab_dashboard);
  sensorhub_v1_pot_component_create(&s_ctx.pot, tab_pot);
  sensorhub_v1_bmi270_component_create(&s_ctx.bmi, tab_bmi);
  sensorhub_v1_capsense_touch_component_create(&s_ctx.capsense_touch, tab_cap);
  sensorhub_v1_bmm350_component_create(&s_ctx.bmm, tab_bmm);

  lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, NULL);

#if (SENSORHUB_V1_BUTTON_TAB_NAV_ENABLE != 0U)
  {
    uint32_t press_count = 0U;
    bool pressed = false;
    if (cm55_get_button_state((uint32_t)BUTTON_ID_0, &press_count, &pressed))
    {
      s_ctx.button_last_press_count[BUTTON_ID_0] = press_count;
      s_ctx.button_press_latched[BUTTON_ID_0] = pressed;
      s_ctx.button_last_nav_ms[BUTTON_ID_0] = lv_tick_get();
    }
    if (cm55_get_button_state((uint32_t)BUTTON_ID_1, &press_count, &pressed))
    {
      s_ctx.button_last_press_count[BUTTON_ID_1] = press_count;
      s_ctx.button_press_latched[BUTTON_ID_1] = pressed;
      s_ctx.button_last_nav_ms[BUTTON_ID_1] = lv_tick_get();
    }
  }
#endif

  s_ctx.refresh_timer = lv_timer_create(refresh_timer_cb, SENSORHUB_V1_REFRESH_MS, NULL);
  render_status();
  sensorhub_v1_dashboard_component_render(&s_ctx.dashboard, &s_ctx.snapshot);
  sensorhub_v1_pot_component_render(&s_ctx.pot, &s_ctx.snapshot);
  sensorhub_v1_bmi270_component_render(&s_ctx.bmi, &s_ctx.snapshot);
  sensorhub_v1_capsense_touch_component_render(&s_ctx.capsense_touch, &s_ctx.snapshot);
  sensorhub_v1_bmm350_component_render(&s_ctx.bmm, &s_ctx.snapshot);

#if (SENSORHUB_V1_AUTO_START_ON_CREATE != 0U)
  sensorhub_v1_request_start(SENSORHUB_V1_DEFAULT_MASK, SENSORHUB_V1_DEFAULT_PERIOD_MS);
#else
  sensorhub_v1_request_status();
#endif
}
