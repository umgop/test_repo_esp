#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "TCA9554.h"
#include "esp_lcd_touch_axs15231b.h"
#include <Wire.h>
#include <SPIFFS.h>
#include <JPEGDEC.h>

#define GFX_BL        6
#define LCD_QSPI_CS   12
#define LCD_QSPI_CLK  5
#define LCD_QSPI_D0   1
#define LCD_QSPI_D1   2
#define LCD_QSPI_D2   3
#define LCD_QSPI_D3   4
#define I2C_SDA       8
#define I2C_SCL       7

#define JPEG_W     480
#define JPEG_H     320
#define DISP_W     320
#define DISP_H     480

#define MAX_ANIMS    8
#define MAX_FRAMES 300
#define TARGET_FPS  25
#define FRAME_MS   (1000 / TARGET_FPS)

TCA9554 TCA(0x20);

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK,
    LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);

Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1, 0, false, 320, 480);

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;
static lv_indev_drv_t     indev_drv;

typedef struct {
    const char  *name;
    const char  *path;
    uint8_t     *data;
    uint32_t     data_size;
    uint16_t     num_frames;
    uint32_t    *frame_off;
    uint32_t    *frame_len;
    bool         loaded;
} MjpegAnim;

static MjpegAnim anims[MAX_ANIMS] = {
    {"TURN_LEFT",      "/TURN_LEFT.mjpeg",      NULL, 0, 0, NULL, NULL, false},
    {"TURN_RIGHT",     "/TURN_RIGHT.mjpeg",     NULL, 0, 0, NULL, NULL, false},
    {"IDLE",           "/IDLE.mjpeg",            NULL, 0, 0, NULL, NULL, false},
    {"DOOR_OPEN",      "/DOOR_OPEN.mjpeg",       NULL, 0, 0, NULL, NULL, false},
    {"DOOR_CLOSE",     "/DOOR_CLOSE.mjpeg",      NULL, 0, 0, NULL, NULL, false},
    {"EMERGENCY_STOP", "/EMERGENCY_STOP.mjpeg",  NULL, 0, 0, NULL, NULL, false},
    {"CHARGING",       "/CHARGING.mjpeg",         NULL, 0, 0, NULL, NULL, false},
    {"CHARGING_FULL",  "/CHARGING_FULL.mjpeg",    NULL, 0, 0, NULL, NULL, false},
};

static int cur_anim  = -1;
static int cur_frame =  0;
static JPEGDEC jpeg;

static uint16_t *jpeg_temp = NULL;
static uint16_t *frame_buf = NULL;

static lv_img_dsc_t frame_dsc;
static lv_obj_t *frame_img = NULL;

static unsigned long last_frame_ms = 0;

#define SER_BUF 32
static char serBuf[SER_BUF];
static int  serIdx = 0;

static void my_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}

static void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
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

static int onJpegDraw(JPEGDRAW *pDraw) {
    uint16_t *src = pDraw->pPixels;
    int sw = pDraw->iWidth;
    int sh = pDraw->iHeight;
    int sx0 = pDraw->x;
    int sy0 = pDraw->y;

    for (int sy = 0; sy < sh; sy++) {
        int srcY = sy0 + sy;
        if (srcY >= JPEG_H) break;
        const uint16_t *srcRow = &src[sy * sw];
        for (int sx = 0; sx < sw; sx++) {
            int srcX = sx0 + sx;
            if (srcX >= JPEG_W) break;
            jpeg_temp[srcY * JPEG_W + srcX] = srcRow[sx];
        }
    }
    return 1;
}

static void rotateFrame() {
    for (int sy = 0; sy < JPEG_H; sy++) {
        int dx = (JPEG_H - 1) - sy;
        const uint16_t *srcRow = &jpeg_temp[sy * JPEG_W];
        for (int sx = 0; sx < JPEG_W; sx++) {
            frame_buf[sx * DISP_W + dx] = srcRow[sx];
        }
    }
}

static bool loadMjpeg(MjpegAnim *a) {
    Serial.printf("  Loading %s\n", a->path);
    File f = SPIFFS.open(a->path, "r");
    if (!f) { Serial.printf("  FAIL open: %s\n", a->path); return false; }

    uint32_t fsize = f.size();
    a->data = (uint8_t *)heap_caps_malloc(fsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a->data) { f.close(); return false; }

    uint32_t got = 0;
    while (got < fsize) {
        uint32_t r = f.read(a->data + got, min((uint32_t)4096, fsize - got));
        if (r == 0) break;
        got += r;
    }
    f.close();
    a->data_size = got;

    uint32_t tmpOff[MAX_FRAMES], tmpLen[MAX_FRAMES];
    uint16_t nf = 0;
    uint32_t pos = 0;
    while (pos < got - 1 && nf < MAX_FRAMES) {
        uint32_t soi = got;
        for (uint32_t i = pos; i < got - 1; i++) {
            if (a->data[i] == 0xFF && a->data[i + 1] == 0xD8) { soi = i; break; }
        }
        if (soi >= got) break;
        uint32_t eoi = got;
        for (uint32_t i = soi + 2; i < got - 1; i++) {
            if (a->data[i] == 0xFF && a->data[i + 1] == 0xD9) { eoi = i + 2; break; }
        }
        if (eoi > got) break;
        tmpOff[nf] = soi;
        tmpLen[nf] = eoi - soi;
        nf++;
        pos = eoi;
    }

    a->num_frames = nf;
    a->frame_off = (uint32_t *)malloc(nf * 4);
    a->frame_len = (uint32_t *)malloc(nf * 4);
    memcpy(a->frame_off, tmpOff, nf * 4);
    memcpy(a->frame_len, tmpLen, nf * 4);
    a->loaded = true;

    Serial.printf("    %u frames\n", nf);
    return true;
}

static void showFrame(MjpegAnim *a, int idx) {
    if (!a->loaded || idx < 0 || idx >= a->num_frames) return;

    uint8_t *ptr = a->data + a->frame_off[idx];
    uint32_t len = a->frame_len[idx];

    if (jpeg.openRAM(ptr, len, onJpegDraw)) {
        jpeg.decode(0, 0, 0);
        jpeg.close();
    }

    rotateFrame();

    if (frame_img) {
        lv_obj_invalidate(frame_img);
    }
}

static void handleCmd(const char *cmd) {
    String c(cmd); c.trim(); c.toUpperCase();
    int sel = -1;

    if      (c == "L") sel = 0;
    else if (c == "R") sel = 1;
    else if (c == "I") sel = 2;
    else if (c == "O") sel = 3;
    else if (c == "C") sel = 4;
    else if (c == "E") sel = 5;
    else if (c == "B") sel = 6;
    else if (c == "F") sel = 7;
    else if (c == "S") {
        cur_anim = -1;
        Serial.println(">> Stopped");
        return;
    } else {
        Serial.printf(">> Unknown: '%s'\n", cmd);
        return;
    }

    if (sel >= 0 && sel < MAX_ANIMS && anims[sel].loaded) {
        cur_anim = sel; cur_frame = 0;
        Serial.printf(">> %s (%d frames)\n", anims[sel].name, anims[sel].num_frames);
    }
}

void setup() {
#ifdef DEV_DEVICE_INIT
    DEV_DEVICE_INIT();
#endif

    Serial.begin(115200);
    delay(300);
    Serial.println("\n========================================");
    Serial.println("  Driving Eyes MJPEG Player");
    Serial.println("  ESP32-S3 Touch 3.5B  (LVGL 8.3.11)");
    Serial.println("========================================");

    Wire.begin(I2C_SDA, I2C_SCL);
    TCA.begin();
    TCA.pinMode1(1, OUTPUT);
    TCA.write1(1, 1);  delay(10);
    TCA.write1(1, 0);  delay(10);
    TCA.write1(1, 1);  delay(200);

    bsp_touch_init(&Wire, -1, 0, 320, 480);

    if (!gfx->begin()) {
        Serial.println("FATAL: gfx->begin() failed");
        while (1) delay(1000);
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    uint32_t screenWidth  = gfx->width();
    uint32_t screenHeight = gfx->height();
    Serial.printf("  Display: %dx%d\n", screenWidth, screenHeight);

    lv_init();

    uint32_t bufPixels = screenWidth * screenHeight;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!buf1 || !buf2) {
        Serial.println("FATAL: LVGL buffer alloc failed");
        while (1) delay(1000);
    }
    Serial.printf("  LVGL buf: %u pixels\n", bufPixels);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufPixels);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = screenWidth;
    disp_drv.ver_res      = screenHeight;
    disp_drv.flush_cb     = my_disp_flush;
    disp_drv.draw_buf     = &draw_buf;
    disp_drv.full_refresh  = 1;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    jpeg_temp = (uint16_t *)heap_caps_malloc(JPEG_W * JPEG_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    frame_buf = (uint16_t *)heap_caps_malloc(DISP_W * DISP_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jpeg_temp || !frame_buf) {
        Serial.println("FATAL: frame buffer alloc failed");
        while (1) delay(1000);
    }
    memset(jpeg_temp, 0, JPEG_W * JPEG_H * 2);
    memset(frame_buf, 0, DISP_W * DISP_H * 2);

    frame_dsc.header.always_zero = 0;
    frame_dsc.header.w           = DISP_W;
    frame_dsc.header.h           = DISP_H;
    frame_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    frame_dsc.data_size          = DISP_W * DISP_H * 2;
    frame_dsc.data               = (const uint8_t *)frame_buf;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    frame_img = lv_img_create(scr);
    lv_img_set_src(frame_img, &frame_dsc);
    lv_obj_set_pos(frame_img, 0, 0);

    if (!SPIFFS.begin(true)) {
        Serial.println("FATAL: SPIFFS mount failed");
        while (1) delay(1000);
    }
    Serial.printf("  SPIFFS total=%u  used=%u\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());

    int loaded = 0;
    for (int i = 0; i < MAX_ANIMS; i++) {
        if (loadMjpeg(&anims[i])) loaded++;
    }
    Serial.printf("  %d/%d animations loaded\n", loaded, MAX_ANIMS);
    Serial.printf("  Free PSRAM: %.2f MB\n",
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1048576.0f);

    Serial.println("\n  Commands: L R I O C E B F S");
    Serial.println("========================================\n");

    if (anims[2].loaded) {
        cur_anim = 2; cur_frame = 0;
        Serial.println(">> Auto-starting IDLE");
    }

    last_frame_ms = millis();
}

void loop() {
    lv_timer_handler();

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serIdx > 0) {
                serBuf[serIdx] = '\0';
                handleCmd(serBuf);
                serIdx = 0;
            }
        } else if (serIdx < SER_BUF - 1) {
            serBuf[serIdx++] = c;
        }
    }

    if (cur_anim >= 0) {
        unsigned long now = millis();
        if (now - last_frame_ms >= FRAME_MS) {
            last_frame_ms = now;
            MjpegAnim *a = &anims[cur_anim];
            if (a->loaded) {
                showFrame(a, cur_frame);
                if (++cur_frame >= a->num_frames) cur_frame = 0;
            }
        }
    }

    delay(1);
}
