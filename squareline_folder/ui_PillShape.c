// Horizontal Pill Shape Screen
// Display: 320×480 (portrait hardware, pill drawn horizontally)
// Pill: 260px wide × 140px tall, fully rounded ends

#include "ui.h"

lv_obj_t *ui_PillShape = NULL;
static lv_obj_t *pill_obj = NULL;

void ui_PillShape_screen_init(void)
{
    ui_PillShape = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_PillShape, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_PillShape, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PillShape, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ---- Horizontal Pill (wide, short, fully rounded) ----
    pill_obj = lv_obj_create(ui_PillShape);
    lv_obj_set_width(pill_obj, 260);
    lv_obj_set_height(pill_obj, 140);
    lv_obj_set_align(pill_obj, LV_ALIGN_CENTER);
    lv_obj_clear_flag(pill_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(pill_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(pill_obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(pill_obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(pill_obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_PillShape_set_color(uint32_t color_hex)
{
    if (pill_obj) {
        lv_obj_set_style_bg_color(pill_obj, lv_color_hex(color_hex), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_PillShape_screen_destroy(void)
{
    if (ui_PillShape) lv_obj_del(ui_PillShape);
    ui_PillShape = NULL;
    pill_obj = NULL;
}
