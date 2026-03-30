# Driving Eyes - MJPEG Animation Player

An MJPEG animation player for the ESP32-S3 Touch 3.5B (Waveshare) board. It plays looping JPEG frame sequences stored on SPIFFS and switches between animations via serial commands. Built with LVGL 8.3.11 and the Arduino framework.

---

## Hardware

- **Board:** ESP32-S3 Touch 3.5B (Waveshare)
- **Display:** 320x480 native portrait, AXS15231B LCD controller, QSPI interface
- **RAM:** 8MB PSRAM (octal SPI)
- **Flash:** 16MB
- **Touch:** AXS15231B capacitive touch (I2C address 0x3B)
- **GPIO Expander:** TCA9554 at I2C address 0x20

---

## Repository Structure

```
driving_eyes/
  driving_eyes.ino          Main sketch
  lv_conf.h                 LVGL configuration
  esp_lcd_touch_axs15231b.h Touch driver
  partitions.csv            Custom partition table
  serial_test.py            Serial test script (Python)
  spiffs.img                Pre-built SPIFFS image with all animations
  data/                     Raw MJPEG animation files
    TURN_LEFT.mjpeg
    TURN_RIGHT.mjpeg
    IDLE.mjpeg
    DOOR_OPEN.mjpeg
    DOOR_CLOSE.mjpeg
    EMERGENCY_STOP.mjpeg
    CHARGING.mjpeg
    CHARGING_FULL.mjpeg
```

---

## How It Works

1. On boot the sketch initializes the display, LVGL, and SPIFFS.
2. Each `.mjpeg` file in SPIFFS is loaded entirely into PSRAM and indexed by frame (SOI/EOI markers).
3. The currently selected animation plays in a loop at 25 FPS (configurable via `TARGET_FPS`).
4. Each frame is a 480x320 JPEG. It gets decoded into a temporary buffer, then software-rotated 90 degrees clockwise into a 320x480 frame buffer. LVGL renders that buffer to the display through an `lv_img` widget in full-refresh mode.
5. Serial commands switch between animations or stop playback.
6. On startup the IDLE animation begins automatically.

---

## Prerequisites

Install the following before building.

### Arduino CLI

```
brew install arduino-cli
```

### ESP32 Board Support

```
arduino-cli core install esp32:esp32
```

### Arduino Libraries

These must be installed under `~/Arduino/libraries/`:

| Library | Purpose |
|---|---|
| lvgl (8.3.11) | Graphics framework |
| Arduino_GFX_Library | Display driver (AXS15231B + QSPI) |
| TCA9554 | I2C GPIO expander |
| JPEGDEC | JPEG decoding |

Install them through the Arduino Library Manager or place them manually in `~/Arduino/libraries/`.

### LVGL Configuration

The `lv_conf.h` file in this directory is picked up automatically during compilation. Key settings:

- `LV_COLOR_DEPTH` = 16
- `LV_COLOR_16_SWAP` = 0
- `LV_MEM_CUSTOM` = 1 (uses stdlib malloc)
- `LV_TICK_CUSTOM` = 1 (uses Arduino millis)

### Python (for serial testing only)

```
pip3 install pyserial
```

---

## Building and Flashing

### 1. Find Your Serial Port

```
ls /dev/cu.usbmodem*
```

Common values: `/dev/cu.usbmodem101` or `/dev/cu.usbmodem1101`. Replace the port in the commands below with whatever shows up.

### 2. Compile

```
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc" \
  --build-property "build.partitions=partitions" \
  --build-property "upload.maximum_size=3145728" \
  /path/to/driving_eyes
```

Expected output: `Sketch uses ~620KB (19%) of program storage space.`

### 3. Upload Sketch

```
arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc" \
  -p /dev/cu.usbmodem101 \
  /path/to/driving_eyes
```

### 4. Flash SPIFFS (animations)

The `spiffs.img` file is pre-built and ready to flash. If you have not already flashed it, or if you changed the animation files in `data/`, you need to flash it:

```
python3 -m esptool \
  --chip esp32s3 \
  --port /dev/cu.usbmodem101 \
  --baud 921600 \
  write_flash --flash_mode dio --flash_size 16MB \
  0x310000 spiffs.img
```

This writes to offset `0x310000` which matches `partitions.csv`.

### 5. Rebuilding spiffs.img (only if you changed data/)

If you added, removed, or replaced MJPEG files in `data/`, rebuild the image:

```
/path/to/mkspiffs \
  -c data \
  -b 4096 \
  -p 256 \
  -s 0xCF0000 \
  spiffs.img
```

On macOS with Arduino, mkspiffs is typically at:
```
~/Library/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs
```

---

## Serial Commands

Open a serial monitor at **115200 baud**. Send a single character followed by Enter.

| Command | Animation |
|---|---|
| `L` | TURN_LEFT |
| `R` | TURN_RIGHT |
| `I` | IDLE |
| `O` | DOOR_OPEN |
| `C` | DOOR_CLOSE |
| `E` | EMERGENCY_STOP |
| `B` | CHARGING |
| `F` | CHARGING_FULL |
| `S` | Stop playback |

You can use `screen`, `minicom`, or the Arduino Serial Monitor:

```
screen /dev/cu.usbmodem101 115200
```

To exit screen: press `Ctrl-A` then `K`, then `Y`.

Or use the included test script:

```
python3 serial_test.py
```

This resets the ESP32, captures 15 seconds of boot output, sends the `L` command, and prints the response.

---

## Boot Output

When the board starts you should see something like:

```
========================================
  Driving Eyes MJPEG Player
  ESP32-S3 Touch 3.5B  (LVGL 8.3.11)
========================================
  Display: 320x480
  LVGL buf: 153600 pixels
  SPIFFS total=13565952  used=6210992
  Loading /TURN_LEFT.mjpeg
    180 frames
  Loading /TURN_RIGHT.mjpeg
    180 frames
  Loading /IDLE.mjpeg
    180 frames
  ...
  8/8 animations loaded
  Free PSRAM: X.XX MB

  Commands: L R I O C E B F S
========================================

>> Auto-starting IDLE
```

If any animation fails to load the log will show `FAIL open: /filename.mjpeg`. This means SPIFFS is either not flashed or the file is missing from the image.

---

## Adding Your Own Animations

### Frame Requirements

- Resolution: **480x320** (landscape). The code rotates each frame 90 degrees clockwise to fill the 320x480 portrait display.
- Format: JPEG
- Frame count: Up to 300 per animation (set by `MAX_FRAMES` in the sketch)

### Step-by-Step

1. **Prepare your frames.** Export your animation as individual JPEG images at 480x320 resolution. Name them sequentially (frame_001.jpg, frame_002.jpg, etc).

2. **Convert to MJPEG.** Concatenate the raw JPEG bytes into a single `.mjpeg` file. A simple Python script:

   ```python
   import glob
   frames = sorted(glob.glob("frame_*.jpg"))
   with open("MY_ANIM.mjpeg", "wb") as out:
       for f in frames:
           out.write(open(f, "rb").read())
   ```

   The parser finds frames by scanning for JPEG SOI (`0xFFD8`) and EOI (`0xFFD9`) markers, so no special header or delimiter is needed. Just concatenate the raw JPEG data.

3. **Copy into data/.** Place `MY_ANIM.mjpeg` into the `data/` directory.

4. **Update the sketch.** In `driving_eyes.ino`, add your animation to the `anims[]` array:

   ```cpp
   static MjpegAnim anims[MAX_ANIMS] = {
       {"TURN_LEFT",      "/TURN_LEFT.mjpeg",      NULL, 0, 0, NULL, NULL, false},
       {"TURN_RIGHT",     "/TURN_RIGHT.mjpeg",     NULL, 0, 0, NULL, NULL, false},
       // ...existing entries...
       {"MY_ANIM",        "/MY_ANIM.mjpeg",         NULL, 0, 0, NULL, NULL, false},
   };
   ```

   Increase `MAX_ANIMS` if needed.

5. **Add a serial command.** In the `handleCmd()` function, add a new case. The index matches the position in the `anims[]` array:

   ```cpp
   else if (c == "M") sel = 8;  // index 8 for the new entry
   ```

6. **Rebuild spiffs.img** using the mkspiffs command shown above.

7. **Flash** both the updated sketch and the new SPIFFS image.

### Size Constraints

The SPIFFS partition is ~13MB (`0xCF0000` bytes). The total size of all `.mjpeg` files in `data/` must fit within this. Current usage is about 6MB across 8 animations (180 frames each at JPEG quality 50). You can fit roughly 8-12 more animations of similar size, or fewer if you use higher quality or more frames.

Each animation is loaded entirely into PSRAM at boot. The ESP32-S3 has 8MB of PSRAM. After LVGL buffers and frame decode buffers, approximately 1-2MB of free PSRAM remains. Keep total animation data well under 6-7MB to leave headroom.

---

## Partition Table

Defined in `partitions.csv`:

| Name | Type | Offset | Size |
|---|---|---|---|
| nvs | data | 0x9000 | 20KB |
| otadata | data | 0xE000 | 8KB |
| app0 | app | 0x10000 | 3MB |
| spiffs | data | 0x310000 | ~13MB |

The sketch binary goes into `app0` (max 3MB). Animations go into `spiffs`.

---

## Troubleshooting

**Port busy error during upload:**
Another process (screen, python, serial monitor) is holding the port. Find and kill it:
```
lsof /dev/cu.usbmodem101
kill -9 <PID>
```

**SPIFFS mount failed:**
The SPIFFS partition has not been flashed. Run the esptool flash command from the "Flash SPIFFS" section above.

**Animation shows FAIL open:**
The `.mjpeg` file is missing from the SPIFFS image. Make sure it is in `data/`, rebuild `spiffs.img`, and reflash.

**Display stays black:**
Check that the backlight pin (GPIO 6) is being driven high and that the TCA9554 GPIO expander is wired correctly. The boot log should show `Display: 320x480`.

**Low FPS:**
The LVGL pipeline with full-frame JPEG decode and 90-degree rotation runs at roughly 5-6 FPS. This is expected for this architecture. The `TARGET_FPS` define controls the maximum frame rate attempt but actual throughput depends on decode and rotation time.
