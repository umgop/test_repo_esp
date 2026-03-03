# Waveshare ESP32-S3-Touch-LCD-3.5B — SquareLine Studio UI

Arduino sketch for the **Waveshare ESP32-S3-Touch-LCD-3.5B** board featuring a 320×480 QSPI display (AXS15231B controller), capacitive touch, and TCA9554 I/O expander. The UI is generated with SquareLine Studio and updated to work with LVGL v9.

## Folder Structure

```
squareline_folder/
├── squareline_folder.ino    ← Main Arduino sketch
├── lv_conf.h                ← LVGL configuration (reference copy)
├── ui/
│   ├── ui.c
│   ├── ui.h
│   ├── ui_Screen1.c
│   ├── ui_Screen1.h
│   ├── ui_events.h
│   ├── ui_helpers.c
│   ├── ui_helpers.h
│   └── ui_comp_hook.c
└── README.md
```

## Required Libraries

Install all of the following from the **Arduino Library Manager** (Sketch → Include Library → Manage Libraries…) unless noted otherwise:

| Library | Source |
|---------|--------|
| **lvgl** (v9.x) | Arduino Library Manager |
| **GFX Library for Arduino** (`Arduino_GFX`) | Arduino Library Manager |
| **TCA9554** | May be bundled with Waveshare examples; install manually if not found in Library Manager |
| **esp_lcd_touch_axs15231b** | From the [Waveshare demo package](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B) |

## Arduino IDE Board Settings

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) or equivalent with ≥3 MB app space |
| USB CDC On Boot | Enabled |

## Setup Instructions

1. **Install Arduino IDE** (v2.x recommended) and add ESP32 board support:
   - Go to *File → Preferences → Additional Boards Manager URLs* and add:
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Then go to *Tools → Board → Boards Manager*, search for **esp32** (by Espressif Systems), and install it.

2. **Install the required libraries** listed in the table above.

3. **Copy `lv_conf.h` to the Arduino libraries folder** — LVGL looks for this file next to the `lvgl` library folder:
   ```
   ~/Arduino/libraries/lv_conf.h      ← place it here (same level as the lvgl/ folder)
   ~/Arduino/libraries/lvgl/          ← lvgl library folder
   ```
   The copy inside this sketch folder is provided for reference only.

4. **Open the sketch** in Arduino IDE:
   ```
   File → Open → squareline_folder/squareline_folder.ino
   ```

5. **Select the board and port** under the *Tools* menu using the settings shown above.

6. **Upload** (*Sketch → Upload* or Ctrl+U).

## Re-exporting from SquareLine Studio

If you re-export the UI from SquareLine Studio (v1.5.x, LVGL 8.3.11 target), place the exported files into the `ui/` subfolder and re-apply the following LVGL v9 API changes:

| File | Change |
|------|--------|
| `ui.c` | Remove `lv_disp_t *dispp`, `lv_theme_*`, and `lv_disp_set_theme` lines; change `lv_disp_load_scr` → `lv_screen_load` |
| `ui_helpers.c` | `lv_img_set_src` → `lv_image_set_src`; `lv_img_set_zoom` → `lv_image_set_scale`; `lv_img_set_angle` → `lv_image_set_rotation`; `lv_img_get_zoom` → `lv_image_get_scale`; `lv_img_get_angle` → `lv_image_get_rotation`; `lv_mem_free` → `lv_free` |
| `ui_helpers.h` | `lv_img_dsc_t` → `lv_image_dsc_t` |
| `ui_Screen1.c` (and any other screen files) | `lv_obj_del` → `lv_obj_delete` |
