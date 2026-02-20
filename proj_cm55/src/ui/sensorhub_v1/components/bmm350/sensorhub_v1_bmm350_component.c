#include "sensorhub_v1_bmm350_component.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SENSORHUB_V1_BMM_COMPASS_SIZE (160)
#define SENSORHUB_V1_BMM_NEEDLE_LEN (60)

static bool sensor_enabled(uint8_t mask, uint8_t sensor_mask)
{
  return (0U != (mask & sensor_mask));
}

static uint16_t normalize_heading_deg(int32_t heading)
{
  while (heading < 0)
  {
    heading += 360;
  }
  while (heading >= 360)
  {
    heading -= 360;
  }
  return (uint16_t)heading;
}

static uint16_t heading_compass_deg(const ipc_sensorhub_bmm350_t *bmm)
{
  float heading_math;
  int32_t heading_compass;

  if (NULL == bmm)
  {
    return 0U;
  }

  heading_math = atan2f(bmm->my, bmm->mx) * (180.0f / 3.1415926f);
  if (heading_math < 0.0f)
  {
    heading_math += 360.0f;
  }

  heading_compass = (int32_t)(90.0f - heading_math);
  return normalize_heading_deg(heading_compass);
}

static const char *heading_to_cardinal(uint16_t heading_deg)
{
  static const char *dirs[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  uint8_t idx = (uint8_t)(((heading_deg + 22U) % 360U) / 45U);
  return dirs[idx];
}

static void set_needle(sensorhub_v1_bmm350_component_t *comp, uint16_t heading_deg)
{
  float theta_deg;
  float theta_rad;
  float dx;
  float dy;
  int32_t cx = (int32_t)(SENSORHUB_V1_BMM_COMPASS_SIZE / 2);
  int32_t cy = (int32_t)(SENSORHUB_V1_BMM_COMPASS_SIZE / 2);
  int32_t tip_x;
  int32_t tip_y;

  if ((NULL == comp) || (NULL == comp->needle))
  {
    return;
  }

  theta_deg = 90.0f - (float)heading_deg;
  theta_rad = theta_deg * (3.1415926f / 180.0f);
  dx = cosf(theta_rad) * (float)SENSORHUB_V1_BMM_NEEDLE_LEN;
  dy = -sinf(theta_rad) * (float)SENSORHUB_V1_BMM_NEEDLE_LEN;

  tip_x = cx + (int32_t)dx;
  tip_y = cy + (int32_t)dy;

  comp->needle_points[0].x = cx;
  comp->needle_points[0].y = cy;
  comp->needle_points[1].x = tip_x;
  comp->needle_points[1].y = tip_y;
  lv_obj_invalidate(comp->needle);
}

void sensorhub_v1_bmm350_component_create(sensorhub_v1_bmm350_component_t *comp, lv_obj_t *parent)
{
  lv_obj_t *north;
  lv_obj_t *east;
  lv_obj_t *south;
  lv_obj_t *west;

  if ((NULL == comp) || (NULL == parent))
  {
    return;
  }

  (void)memset(comp, 0, sizeof(*comp));

  lv_obj_set_style_pad_all(parent, 12, 0);
  lv_obj_set_style_pad_row(parent, 10, 0);
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  comp->compass = lv_obj_create(parent);
  lv_obj_set_size(comp->compass, SENSORHUB_V1_BMM_COMPASS_SIZE, SENSORHUB_V1_BMM_COMPASS_SIZE);
  lv_obj_set_style_radius(comp->compass, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(comp->compass, 2, 0);
  lv_obj_set_style_border_color(comp->compass, lv_color_hex(0xD84315), 0);
  lv_obj_set_style_bg_color(comp->compass, lv_color_hex(0xFFF5F0), 0);
  lv_obj_set_style_pad_all(comp->compass, 0, 0);
  lv_obj_set_style_clip_corner(comp->compass, true, 0);

  north = lv_label_create(comp->compass);
  lv_label_set_text(north, "N");
  lv_obj_set_style_text_color(north, lv_color_hex(0xD84315), 0);
  lv_obj_set_style_text_font(north, &lv_font_montserrat_16, 0);
  lv_obj_align(north, LV_ALIGN_TOP_MID, 0, 4);

  east = lv_label_create(comp->compass);
  lv_label_set_text(east, "E");
  lv_obj_set_style_text_color(east, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_font(east, &lv_font_montserrat_16, 0);
  lv_obj_align(east, LV_ALIGN_RIGHT_MID, -6, 0);

  south = lv_label_create(comp->compass);
  lv_label_set_text(south, "S");
  lv_obj_set_style_text_color(south, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_font(south, &lv_font_montserrat_16, 0);
  lv_obj_align(south, LV_ALIGN_BOTTOM_MID, 0, -4);

  west = lv_label_create(comp->compass);
  lv_label_set_text(west, "W");
  lv_obj_set_style_text_color(west, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_font(west, &lv_font_montserrat_16, 0);
  lv_obj_align(west, LV_ALIGN_LEFT_MID, 6, 0);

  comp->needle = lv_line_create(comp->compass);
  lv_obj_set_size(comp->needle, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(comp->needle, 0, 0);
  lv_obj_set_style_line_width(comp->needle, 4, 0);
  lv_obj_set_style_line_color(comp->needle, lv_color_hex(0xD84315), 0);
  lv_obj_set_style_line_rounded(comp->needle, true, 0);
  comp->needle_points[0].x = (SENSORHUB_V1_BMM_COMPASS_SIZE / 2);
  comp->needle_points[0].y = (SENSORHUB_V1_BMM_COMPASS_SIZE / 2);
  comp->needle_points[1].x = (SENSORHUB_V1_BMM_COMPASS_SIZE / 2);
  comp->needle_points[1].y = (SENSORHUB_V1_BMM_COMPASS_SIZE / 2 - SENSORHUB_V1_BMM_NEEDLE_LEN);
  lv_line_set_points_mutable(comp->needle, comp->needle_points, 2);

  comp->heading_label = lv_label_create(parent);
  lv_obj_set_width(comp->heading_label, LV_PCT(100));
  lv_obj_set_style_text_font(comp->heading_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(comp->heading_label, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_align(comp->heading_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(comp->heading_label, "Heading: --");

  comp->info_label = lv_label_create(parent);
  lv_obj_set_width(comp->info_label, LV_PCT(100));
  lv_label_set_long_mode(comp->info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(comp->info_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(comp->info_label, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_align(comp->info_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(comp->info_label, "Waiting for BMM350 sample...");
}

void sensorhub_v1_bmm350_component_render(sensorhub_v1_bmm350_component_t *comp, const sensorhub_v1_snapshot_t *snapshot)
{
  const ipc_sensorhub_bmm350_t *bmm;
  const char *dir;
  char heading_text[48];
  char info[220];
  uint16_t heading_deg;

  if ((NULL == comp) || (NULL == snapshot) || (NULL == comp->needle) || (NULL == comp->heading_label) ||
      (NULL == comp->info_label))
  {
    return;
  }

  if (!sensor_enabled(snapshot->status.sensors_mask, IPC_SENSORHUB_SENSOR_MASK_BMM350))
  {
    lv_label_set_text(comp->heading_label, "Heading: --");
    lv_label_set_text(comp->info_label, "BMM350 disabled in mask.");
    set_needle(comp, 0U);
    return;
  }

  if (!snapshot->has_bmm350)
  {
    lv_label_set_text(comp->heading_label, "Heading: --");
    lv_label_set_text(comp->info_label, "Waiting for BMM350 sample...");
    set_needle(comp, 0U);
    return;
  }

  bmm = &snapshot->bmm350_sample.data.bmm350;
  heading_deg = heading_compass_deg(bmm);
  dir = heading_to_cardinal(heading_deg);
  set_needle(comp, heading_deg);

  (void)snprintf(heading_text, sizeof(heading_text), "Heading: %u deg (%s)", (unsigned int)heading_deg, dir);
  lv_label_set_text(comp->heading_label, heading_text);

  (void)snprintf(info, sizeof(info),
                 "M: %.1f %.1f %.1f uT\n"
                 "|B|: %.1f uT\n"
                 "T: %.1f C | Seq %u",
                 (double)bmm->mx,
                 (double)bmm->my,
                 (double)bmm->mz,
                 (double)bmm->mag,
                 (double)bmm->temperature,
                 (unsigned int)snapshot->bmm350_sample.sequence);
  lv_label_set_text(comp->info_label, info);
}
