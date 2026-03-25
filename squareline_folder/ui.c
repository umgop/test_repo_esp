// ui.c - Simplified for Pill Shape only
// ESP32-S3 Touch 3.5B with LVGL 8.3.11

#include "ui.h"
#include "ui_PillShape.h"

///////////////////// LVGL COLOR DEPTH CHECK ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit"
#endif

///////////////////// UI INIT & DESTROY ////////////////////

void ui_init(void)
{
    ui_PillShape_screen_init();
    lv_disp_load_scr(ui_PillShape);
}

void ui_destroy(void)
{
    ui_PillShape_screen_destroy();
}
