// ui_PillShape.h
// Rounded Pill Shape Screen Header
// Generated for ESP32-S3 Touch 3.5B

#ifndef _UI_PILLSHAPE_H
#define _UI_PILLSHAPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"

extern lv_obj_t *ui_PillShape;

void ui_PillShape_screen_init(void);
void ui_PillShape_set_color(uint32_t color_hex);
void ui_PillShape_screen_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
