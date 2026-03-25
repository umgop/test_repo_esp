/*
 * Horizontal Pill Shape — ESP32-S3 Touch 3.5B
 *
 * Serial commands (115200 baud):
 *   1 = White
 *   2 = Red
 *   3 = Green
 *   4 = Blue
 *   5 = Yellow
 *
 * LVGL 8.3.11
 */

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "TCA9554.h"
#include "esp_lcd_touch_axs15231b.h"
#include <Wire.h>
#include "ui.h"
#include "ui_PillShape.h"

/* -------- hardware pin definitions -------- */
#define GFX_BL        6
#define LCD_QSPI_CS   12
#define LCD_QSPI_CLK  5
#define LCD_QSPI_D0   1
#define LCD_QSPI_D1   2
#define LCD_QSPI_D2   3
#define LCD_QSPI_D3   4
#define I2C_SDA       8
#define I2C_SCL       7

/* -------- display / IO objects -------- */
TCA9554 TCA(0x20);
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK,
    LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1, 0, false, 320, 480);

/* -------- LVGL 8 display driver objects -------- */
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;
static lv_indev_drv_t     indev_drv;

static uint32_t screenWidth;
static uint32_t screenHeight;

/* -------- serial command buffer -------- */
#define SERIAL_BUF_SIZE 32
static char serialBuf[SERIAL_BUF_SIZE];
static int  serialBufIdx = 0;

/* ============================================================
 *  LVGL 8 flush callback
 * ============================================================ */
static void my_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}

/* ============================================================
 *  LVGL 8 touch-pad read callback
 * ============================================================ */
static void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    touch_data_t td;
    bsp_touch_read();
    if (bsp_touch_get_coordinates(&td)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = td.coords[0].x;
        data->point.y = td.coords[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ============================================================
 *  Serial → pill color
 * ============================================================ */
static void handleCommand(const char *cmd)
{
    String c = String(cmd);
    c.trim();

    if      (c == "1") { ui_PillShape_set_color(0xFFFFFF); Serial.println(">> White");  }
    else if (c == "2") { ui_PillShape_set_color(0xFF0000); Serial.println(">> Red");    }
    else if (c == "3") { ui_PillShape_set_color(0x00FF00); Serial.println(">> Green");  }
    else if (c == "4") { ui_PillShape_set_color(0x0000FF); Serial.println(">> Blue");   }
    else if (c == "5") { ui_PillShape_set_color(0xFFFF00); Serial.println(">> Yellow"); }
    else {
        Serial.print(">> Unknown: ");
        Serial.println(cmd);
        Serial.println("   1=White  2=Red  3=Green  4=Blue  5=Yellow");
    }
}

/* ============================================================
 *  SETUP
 * ============================================================ */
void setup()
{
#ifdef DEV_DEVICE_INIT
    DEV_DEVICE_INIT();
#endif

    /* ---- I2C + GPIO expander ---- */
    Wire.begin(I2C_SDA, I2C_SCL);
    TCA.begin();
    TCA.pinMode1(1, OUTPUT);
    TCA.write1(1, 1);
    delay(10);
    TCA.write1(1, 0);
    delay(10);
    TCA.write1(1, 1);
    delay(200);

    /* ---- Touch init ---- */
    bsp_touch_init(&Wire, -1, 0, 320, 480);

    /* ---- Serial ---- */
    Serial.begin(115200);
    Serial.println("\n========================================");
    Serial.println("  Pill Shape Display");
    Serial.println("  ESP32-S3 Touch 3.5B  (LVGL 8.3.11)");
    Serial.println("========================================");
    Serial.println("  1=White  2=Red  3=Green  4=Blue  5=Yellow");
    Serial.println("========================================\n");

    /* ---- Display hardware ---- */
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    /* ---- LVGL init ---- */
    lv_init();
    /* LV_TICK_CUSTOM is enabled in lv_conf.h → uses millis() automatically */

    screenWidth  = gfx->width();
    screenHeight = gfx->height();

    /* ---- Allocate frame buffers in PSRAM (full-frame for smooth transitions) ---- */
    uint32_t bufPixels = screenWidth * screenHeight;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!buf1 || !buf2) {
        Serial.println("LVGL buffer alloc failed!");
        /* Fall back to smaller internal-RAM buffer */
        bufPixels = screenWidth * 40;
        buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!buf1 || !buf2) {
        Serial.println("FATAL: cannot allocate any LVGL buffer!");
        while (1) delay(1000);
    }

    /* ---- LVGL 8 display driver ---- */
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufPixels);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = screenWidth;
    disp_drv.ver_res   = screenHeight;
    disp_drv.flush_cb  = my_disp_flush;
    disp_drv.draw_buf  = &draw_buf;
    if (bufPixels == screenWidth * screenHeight) {
        disp_drv.full_refresh = 1;   /* full-frame mode for PSRAM buffer */
    }
    lv_disp_drv_register(&disp_drv);

    /* ---- LVGL 8 input device ---- */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    /* ---- SquareLine UI init (loads NEUTRAL screen) ---- */
    ui_init();

    Serial.println(">> Ready!  Showing white pill.  Send 1-5 to change color.");
}

/* ============================================================
 *  LOOP
 * ============================================================ */
void loop()
{
    lv_timer_handler();   /* LVGL 8: was lv_task_handler, both work */

    /* Read serial commands (newline-terminated) */
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBufIdx > 0) {
                serialBuf[serialBufIdx] = '\0';
                handleCommand(serialBuf);
                serialBufIdx = 0;
            }
        } else if (serialBufIdx < SERIAL_BUF_SIZE - 1) {
            serialBuf[serialBufIdx++] = c;
        }
    }

    delay(5);
}
