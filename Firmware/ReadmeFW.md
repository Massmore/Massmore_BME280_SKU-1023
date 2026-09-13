# Pre-built Firmware — Massmore BME280 Factory Test (SKU-1023)

<p align="center">
  <img src="../Document/images/01_massmore_bme280_cover.png" alt="Massmore BME280" width="420">
</p>

This folder holds the **Factory Test** (`examples/07_Factory_Test`) already compiled, so it can be flashed onto an
ESP32 / ESP32-S3 without installing the Arduino IDE or PlatformIO.

> เฟิร์มแวร์ชุดทดสอบโรงงานที่คอมไพล์ไว้แล้ว แฟลชลง ESP32 / ESP32-S3 ได้ทันทีโดยไม่ต้องลง IDE

Use it to

- test many boards in a row on the production line
- confirm that a newly received board works on every function
- separate **BME280 (0x60)** from **BMP280 (0x58)**
- verify the chip is genuine Bosch silicon (10-point behavioural check)

---

## Files

All binaries are in [`bin/`](bin/) (version **1.1.0** = library 1.1.0; default address 0x77, 0x76 is scanned too).

| File in `bin/` | Board | Offset | Description |
|---|---|---|---|
| `Massmore_BME280_FactoryTest_v1.1.0_esp32dev_merged.bin` | ESP32 classic | `0x0` | **Single merged file** — recommended |
| `Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1_merged.bin` | ESP32-S3 | `0x0` | **Single merged file** — recommended |
| `Massmore_BME280_FactoryTest_v1.1.0_<board>.bin` | both | `0x10000` | Application only |
| `parts/<board>/bootloader.bin` | both | ESP32 `0x1000` · S3 `0x0` | Bootloader |
| `parts/<board>/partitions.bin` | both | `0x8000` | Partition table |
| `parts/<board>/boot_app0.bin` | both | `0xe000` | OTA data |
| `Massmore_BME280_FactoryTest_v1.0.0_esp32dev*.bin` | ESP32 classic | – | Previous release (library 1.0.0), kept for reference |
| `manifest.json` | – | – | Build info for both boards, ESP Web Tools compatible |
| `SHA256SUMS.txt` | – | – | Checksums |
| `flash_mac.command` / `flash_linux.sh` | – | – | Flash scripts (first argument = board name) |

> **v1.1.0 status**: builds cleanly on PlatformIO but **hardware verification is still pending**.
> `// TODO: [MASSMORE_INPUT_REQUIRED: v1.1.0 hardware test result on ESP32-S3 / ESP32 / Arduino Nano]`
> No pre-built binary is provided for Arduino Nano (bootloader/fuses vary per lot); upload `07_Factory_Test` from the IDE instead.

**Build info**

| Item | Value |
|---|---|
| Sketch | `examples/07_Factory_Test`, library Massmore_BME280 1.1.0 |
| Boards | `esp32dev` (ESP32-WROOM-32, 4 MB flash) · `esp32-s3-devkitc-1` (8 MB flash, USB-CDC on boot) |
| Arduino ESP32 core | 3.3.11 (ESP-IDF v5.x) via pioarduino platform-espressif32 55.03.311 |
| Serial Monitor | **115200** baud |
| I²C pins (variant defaults) | ESP32: SDA **GPIO 21** · SCL **GPIO 22** — ESP32-S3: SDA **GPIO 8** · SCL **GPIO 9** |
| Expected last line | `[PASS] SENSOR QA PASSED - READY TO SHIP` |

---

## Method 1 — flash scripts (easiest)

### macOS

1. Connect the ESP32 with a USB cable that **carries data** (not a charge-only cable)
2. Double-click `bin/flash_mac.command` (if macOS refuses, **right-click → Open → Open**)

Or from a terminal:

```bash
cd bin
chmod +x flash_mac.command
./flash_mac.command                       # ESP32 classic (esp32dev)
./flash_mac.command esp32-s3-devkitc-1    # ESP32-S3
```

### Linux

```bash
cd bin
chmod +x flash_linux.sh
./flash_linux.sh                                # esp32dev, auto-detect port
./flash_linux.sh esp32-s3-devkitc-1             # ESP32-S3
./flash_linux.sh esp32dev /dev/ttyUSB0          # explicit port
```

On `Permission denied`, add yourself to `dialout` and log in again:

```bash
sudo usermod -a -G dialout $USER
```

Both scripts need `esptool`:

```bash
pip3 install esptool
```

> เสียบบอร์ด แล้วดับเบิลคลิก `flash_mac.command` (หรือรันสคริปต์ Linux) ต้องมี esptool ก่อน

---

## Method 2 — esptool by hand

### Merged file (recommended)

```bash
# ESP32 classic
esptool.py --chip esp32 --port <your-port> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32dev_merged.bin

# ESP32-S3
esptool.py --chip esp32s3 --port <your-port> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1_merged.bin
```

### Separate parts (to keep an existing partition layout)

```bash
# ESP32 classic (bootloader at 0x1000)
esptool.py --chip esp32 --port <your-port> --baud 512000 \
  write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000  bin/parts/esp32dev/bootloader.bin \
  0x8000  bin/parts/esp32dev/partitions.bin \
  0xe000  bin/parts/esp32dev/boot_app0.bin \
  0x10000 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32dev.bin

# ESP32-S3 (bootloader at 0x0)
esptool.py --chip esp32s3 --port <your-port> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0     bin/parts/esp32-s3-devkitc-1/bootloader.bin \
  0x8000  bin/parts/esp32-s3-devkitc-1/partitions.bin \
  0xe000  bin/parts/esp32-s3-devkitc-1/boot_app0.bin \
  0x10000 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1.bin
```

**Finding the port**

| OS | Command | Typical name |
|---|---|---|
| macOS | `ls /dev/cu.*` | `/dev/cu.usbserial-0001` · `/dev/cu.wchusbserial1420` · `/dev/cu.usbmodem*` (S3) |
| Linux | `ls /dev/ttyUSB* /dev/ttyACM*` | `/dev/ttyUSB0` · `/dev/ttyACM0` (S3) |
| Windows | Device Manager → Ports (COM & LPT) | `COM5` |

> `--baud 512000` is the most stable value on macOS with Massmore boards.
> On `Timed out waiting for packet header`, drop to `460800` or `115200`.

---

## Method 3 — web browser

`bin/manifest.json` is prepared for **ESP Web Tools**. Open it in a Web-Serial-capable browser (Chrome or Edge) and flash without installing anything.

---

## Verify file integrity

```bash
cd bin
shasum -a 256 -c SHA256SUMS.txt     # macOS
sha256sum -c SHA256SUMS.txt         # Linux
```

---

## After flashing

1. Connect the **Massmore BME280** to the ESP32 — one Qwiic cable, or wires:
   `VIN → 3V3/5V` · `GND → GND` · `SDI → SDA` · `SCK → SCL`
   (ESP32 classic: SDA GPIO 21 / SCL GPIO 22 — ESP32-S3: SDA GPIO 8 / SCL GPIO 9)

   <p align="center">
     <img src="../Document/images/03_massmore_bme280_wiring_esp32.png" alt="wiring" width="480">
   </p>

2. Open the **Serial Monitor at 115200 baud**
3. Press **EN / RESET** on the ESP32 — the test starts automatically
4. Type `r` + Enter to run again (handy when testing boards one after another)

> ต่อเซ็นเซอร์ เปิด Serial Monitor 115200 กด RESET แล้วดูบรรทัดสุดท้าย พิมพ์ r เพื่อทดสอบซ้ำ

---

## Expected output

```
==========================================================
  Massmore BME280 (SKU-1023) - Factory Test
  Environment Sensor  |  Bosch BME280
  Library v1.1.0
==========================================================

--- GATE 1 : สแกนบัส I2C ---
      พบอุปกรณ์ที่ 0x77
[ OK ] 01 I2C_SCAN       พบเซ็นเซอร์ที่ 0x77 (อุปกรณ์บนบัสทั้งหมด 1 ตัว)
#RESULT,1,I2C_SCAN,PASS,พบเซ็นเซอร์ที่ 0x77 (อุปกรณ์บนบัสทั้งหมด 1 ตัว)

--- GATE 2 : ตรวจรหัสประจำรุ่น ---
[ OK ] 02 CHIP_ID        รหัส 0x60 = Bosch BME280 ถูกต้อง
[ OK ] 03 BEGIN          เริ่มต้นไลบรารีและอ่านค่าชดเชยได้ครบ

--- RUN TEST ---
[ OK ] 04 SOFT_RESET     รีเซ็ตแล้วรีจิสเตอร์ตั้งค่าทั้งสามกลับเป็น 0x00
[ OK ] 05 CALIB_READ     T1=28455 T2=26824 P1=37124 P2=-10629 H1=75 H2=364
...
[ OK ] 22 GENUINE        ผ่าน 10 จาก 10 ข้อ

[ สรุป ]
  ผ่าน 22   เตือน 0   ไม่ผ่าน 0
==========================================================
#DEVICE,0x77,BME280,0x60,GENUINE,10
#VERDICT,PASS,22,0,0

[PASS] SENSOR QA PASSED - READY TO SHIP
```

(Per-check detail text is in Thai; the check names, `#` lines and the final verdict are ASCII.)

---

## Machine-readable lines

```
#RESULT,<index>,<check>,<PASS|FAIL|WARN>,<detail>
#DEVICE,<addr>,<chip>,<chip_id>,<GENUINE|SUSPECT|FAKE|UNKNOWN>,<passed_of_10>
#VERDICT,<PASS|FAIL>,<pass>,<fail>,<warn>
```

A web tool only needs to watch for `#VERDICT,PASS` — or the final `[PASS]` / `[FAIL]` line.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| No serial port appears | Use a data USB cable; install the CH340 driver; on ESP32-S3 hold **BOOT** and tap **RESET** to enter download mode |
| `Timed out waiting for packet header` | Lower the baud rate to 460800 or 115200 |
| Serial Monitor shows garbage | Set 115200 baud; ESP32-S3 output is on the USB port (USB-CDC) |
| `[FAIL] QA CHECK FAILED: I2C_SCAN` | Check VIN/GND/SDI/SCK wiring and the Qwiic cable |
| `[FAIL] QA CHECK FAILED: CHIP_ID` | The board is a BMP280 (0x58) — see the silkscreen tick box |

---

## Rebuild it yourself

```bash
cd ../PlatformIO
cp examples/07_Factory_Test/main.cpp src/main.cpp
pio run -e esp32dev                  # or -e esp32-s3-devkitc-1
```

Output files:

```
PlatformIO/.pio/build/<env>/firmware.bin          <- application
PlatformIO/.pio/build/<env>/firmware.factory.bin  <- merged single file
```

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
