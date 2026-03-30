// ESP LCD Touch AXS15231B Driver
// For Waveshare ESP32-S3-Touch-LCD-3.5B
// Minimal I2C touch driver

#ifndef ESP_LCD_TOUCH_AXS15231B_H
#define ESP_LCD_TOUCH_AXS15231B_H

#include <Wire.h>

#define AXS_TOUCH_ADDR 0x3B

typedef struct {
    int16_t x;
    int16_t y;
} touch_coord_t;

typedef struct {
    touch_coord_t coords[1];
    uint8_t point_num;
} touch_data_t;

static TwoWire *_touch_wire = NULL;
static int _touch_width = 320;
static int _touch_height = 480;
static touch_data_t _last_touch = {0};
static bool _touch_pressed = false;

static void bsp_touch_init(TwoWire *wire, int rst_pin, int int_pin, int width, int height) {
    _touch_wire = wire;
    _touch_width = width;
    _touch_height = height;
}

static void bsp_touch_read(void) {
    if (!_touch_wire) return;

    uint8_t buf[8] = {0};
    _touch_pressed = false;

    _touch_wire->beginTransmission(AXS_TOUCH_ADDR);
    _touch_wire->write(0xB5);
    _touch_wire->write(0xAB);
    _touch_wire->write(0xA5);
    _touch_wire->write(0x5A);
    _touch_wire->write(0x00);
    _touch_wire->write(0x00);
    _touch_wire->write(0x00);
    _touch_wire->write(0x08);
    _touch_wire->endTransmission();

    _touch_wire->requestFrom((uint8_t)AXS_TOUCH_ADDR, (uint8_t)8);
    int i = 0;
    while (_touch_wire->available() && i < 8) {
        buf[i++] = _touch_wire->read();
    }

    uint8_t gesture = buf[0];
    uint8_t point_num = buf[1];

    if (point_num > 0 && point_num < 6) {
        _touch_pressed = true;
        int16_t x = ((buf[2] & 0x0F) << 8) | buf[3];
        int16_t y = ((buf[4] & 0x0F) << 8) | buf[5];

        if (x >= 0 && x < _touch_width && y >= 0 && y < _touch_height) {
            _last_touch.coords[0].x = x;
            _last_touch.coords[0].y = y;
            _last_touch.point_num = 1;
        } else {
            _touch_pressed = false;
        }
    }
}

static bool bsp_touch_get_coordinates(touch_data_t *data) {
    if (_touch_pressed && data) {
        data->coords[0].x = _last_touch.coords[0].x;
        data->coords[0].y = _last_touch.coords[0].y;
        data->point_num = 1;
        return true;
    }
    return false;
}

#endif // ESP_LCD_TOUCH_AXS15231B_H
