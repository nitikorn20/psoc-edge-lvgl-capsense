#include "sensorhub_v1_capsense_touch_component.h"

#include <stdio.h>
#include <string.h>

static bool sensor_enabled(uint8_t mask, uint8_t sensor_mask)
{
  return (0U != (mask & sensor_mask));
}

static void set_button_tile_state(lv_obj_t *tile, lv_obj_t *icon, lv_obj_t *state, bool on)
{
  if ((NULL == tile) || (NULL == icon) || (NULL == state))
  {
    return;
  }

  if (on)
  {
    lv_obj_set_style_bg_color(tile, lv_color_hex(0xE7F6EE), 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(0x6CC08B), 0);
    lv_label_set_text(icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x188038), 0);
    lv_label_set_text(state, "ACTIVE");
    lv_obj_set_style_text_color(state, lv_color_hex(0x188038), 0);
  }
  else
  {
    lv_obj_set_style_bg_color(tile, lv_color_hex(0xEEF3F9), 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(0xC9D4E5), 0);
    lv_label_set_text(icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x7A8DA3), 0);
    lv_label_set_text(state, "IDLE");
    lv_obj_set_style_text_color(state, lv_color_hex(0x7A8DA3), 0);
  }
}

static lv_obj_t *create_button_tile(lv_obj_t *parent, const char *title, lv_obj_t **out_icon, lv_obj_t **out_state)
{
  lv_obj_t *tile;
  lv_obj_t *title_label;
  lv_obj_t *icon_label;
  lv_obj_t *state_label;

  tile = lv_obj_create(parent);
  lv_obj_set_size(tile, 154, 102);
  lv_obj_set_style_radius(tile, 14, 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  lv_obj_set_style_pad_all(tile, 8, 0);
  lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(tile, 4, 0);

  title_label = lv_label_create(tile);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x334E68), 0);

  icon_label = lv_label_create(tile);
  lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_28, 0);

  state_label = lv_label_create(tile);
  lv_obj_set_style_text_font(state_label, &lv_font_montserrat_14, 0);

  if (NULL != out_icon)
  {
    *out_icon = icon_label;
  }
  if (NULL != out_state)
  {
    *out_state = state_label;
  }

  return tile;
}

void sensorhub_v1_capsense_touch_component_create(sensorhub_v1_capsense_touch_component_t *comp, lv_obj_t *parent)
{
  lv_obj_t *row;
  lv_obj_t *slider_wrap;

  if ((NULL == comp) || (NULL == parent))
  {
    return;
  }

  (void)memset(comp, 0, sizeof(*comp));

  lv_obj_set_style_pad_top(parent, 16, 0);
  lv_obj_set_style_pad_bottom(parent, 16, 0);
  lv_obj_set_style_pad_left(parent, 12, 0);
  lv_obj_set_style_pad_right(parent, 12, 0);
  lv_obj_set_style_pad_row(parent, 14, 0);
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);

  row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 16, 0);

  comp->btn0_tile = create_button_tile(row, "BTN0", &comp->btn0_icon, &comp->btn0_state);
  comp->btn1_tile = create_button_tile(row, "BTN1", &comp->btn1_icon, &comp->btn1_state);

  slider_wrap = lv_obj_create(parent);
  lv_obj_set_size(slider_wrap, 192, 192);
  lv_obj_set_style_bg_opa(slider_wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(slider_wrap, 0, 0);
  lv_obj_set_style_pad_all(slider_wrap, 0, 0);

  comp->slider_arc = lv_arc_create(slider_wrap);
  lv_obj_set_size(comp->slider_arc, 192, 192);
  lv_arc_set_range(comp->slider_arc, 0, 100);
  lv_arc_set_rotation(comp->slider_arc, 135);
  lv_arc_set_bg_angles(comp->slider_arc, 0, 270);
  lv_arc_set_value(comp->slider_arc, 0);
  lv_obj_remove_style(comp->slider_arc, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(comp->slider_arc, 15, LV_PART_MAIN);
  lv_obj_set_style_arc_width(comp->slider_arc, 15, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(comp->slider_arc, lv_color_hex(0xDDE7F6), LV_PART_MAIN);
  lv_obj_set_style_arc_color(comp->slider_arc, lv_color_hex(0x1A73E8), LV_PART_INDICATOR);
  lv_obj_center(comp->slider_arc);

  comp->slider_value = lv_label_create(slider_wrap);
  lv_label_set_text(comp->slider_value, "--%");
  lv_obj_set_style_text_font(comp->slider_value, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(comp->slider_value, lv_color_hex(0x102A43), 0);
  lv_obj_align(comp->slider_value, LV_ALIGN_CENTER, 0, -8);

  {
    lv_obj_t *slider_caption = lv_label_create(slider_wrap);
    lv_label_set_text(slider_caption, "CAP Slider");
    lv_obj_set_style_text_font(slider_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(slider_caption, lv_color_hex(0x627D98), 0);
    lv_obj_align(slider_caption, LV_ALIGN_CENTER, 0, 24);
  }

  comp->meta_label = lv_label_create(parent);
  lv_obj_set_style_text_font(comp->meta_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(comp->meta_label, lv_color_hex(0x627D98), 0);
  lv_label_set_text(comp->meta_label, "Waiting for CAPSENSE sample...");

  set_button_tile_state(comp->btn0_tile, comp->btn0_icon, comp->btn0_state, false);
  set_button_tile_state(comp->btn1_tile, comp->btn1_icon, comp->btn1_state, false);
}

void sensorhub_v1_capsense_touch_component_render(sensorhub_v1_capsense_touch_component_t *comp,
                                                  const sensorhub_v1_snapshot_t *snapshot)
{
  char text[128];
  bool capsense_enabled;
  bool b0;
  bool b1;
  uint8_t slider;

  if ((NULL == comp) || (NULL == snapshot) || (NULL == comp->btn0_tile) || (NULL == comp->btn1_tile) ||
      (NULL == comp->slider_arc) || (NULL == comp->slider_value) || (NULL == comp->meta_label))
  {
    return;
  }

  capsense_enabled = sensor_enabled(snapshot->status.sensors_mask, IPC_SENSORHUB_SENSOR_MASK_CAPSENSE);

  if (!capsense_enabled)
  {
    set_button_tile_state(comp->btn0_tile, comp->btn0_icon, comp->btn0_state, false);
    set_button_tile_state(comp->btn1_tile, comp->btn1_icon, comp->btn1_state, false);
    lv_arc_set_value(comp->slider_arc, 0);
    lv_label_set_text(comp->slider_value, "--%");
    lv_label_set_text(comp->meta_label, "CAPSENSE disabled in mask");
    return;
  }

  if (!snapshot->has_capsense)
  {
    set_button_tile_state(comp->btn0_tile, comp->btn0_icon, comp->btn0_state, false);
    set_button_tile_state(comp->btn1_tile, comp->btn1_icon, comp->btn1_state, false);
    lv_arc_set_value(comp->slider_arc, 0);
    lv_label_set_text(comp->slider_value, "--%");
    lv_label_set_text(comp->meta_label, "Waiting for CAPSENSE sample...");
    return;
  }

  b0 = (0U != snapshot->capsense_sample.data.capsense.btn0_pressed);
  b1 = (0U != snapshot->capsense_sample.data.capsense.btn1_pressed);
  slider = snapshot->capsense_sample.data.capsense.slider;

  set_button_tile_state(comp->btn0_tile, comp->btn0_icon, comp->btn0_state, b0);
  set_button_tile_state(comp->btn1_tile, comp->btn1_icon, comp->btn1_state, b1);
  lv_arc_set_value(comp->slider_arc, slider);
  (void)snprintf(text, sizeof(text), "%u%%", (unsigned int)slider);
  lv_label_set_text(comp->slider_value, text);

  (void)snprintf(text, sizeof(text), "CAP seq=%u | %lu ms",
                 (unsigned int)snapshot->capsense_sample.sequence,
                 (unsigned long)snapshot->capsense_sample.timestamp_ms);
  lv_label_set_text(comp->meta_label, text);
}
