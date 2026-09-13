# Arduino IDE folder

The folder to install is **`Massmore_BME280/`** (the one containing `library.properties`), not this `ArduinoIDE` folder.

> โฟลเดอร์ที่ต้องติดตั้งคือ `Massmore_BME280/` ไม่ใช่โฟลเดอร์ `ArduinoIDE` นี้

## Contents

| File / folder | Description |
|---|---|
| `Massmore_BME280.zip` | **Ready to install** via Add .ZIP Library |
| `Massmore_BME280/` | Library sources (contains `library.properties`) |
| `make_zip.sh` | Rebuilds the ZIP after editing the sources |

## Install

### Option 1 — the prepared ZIP (recommended)

1. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…**
2. Select **`Massmore_BME280.zip`** in this folder
3. Open an example from **File → Examples → Massmore_BME280**

> Do not unzip it yourself, and **do not select this `ArduinoIDE` folder** — it holds two projects and the IDE cannot tell which one is the library.
> ไม่ต้องแตก ZIP เอง และห้ามเลือกโฟลเดอร์ ArduinoIDE ทั้งโฟลเดอร์

### Option 2 — copy the folder

Copy the whole `Massmore_BME280` folder to

| OS | Location |
|---|---|
| macOS | `~/Documents/Arduino/libraries/` |
| Windows | `Documents\Arduino\libraries\` |
| Linux | `~/Arduino/libraries/` |

Restart the Arduino IDE, then open **File → Examples → Massmore_BME280**.

## Prerequisites

- Arduino IDE 2.x
- The board package for your MCU (the library has no other dependency; it only uses `Wire` / `SPI` from the core)

| Board | Board package | Boards Manager URL |
|---|---|---|
| ESP32 / ESP32-S3 | **esp32 by Espressif, core 3.x** | `https://espressif.github.io/arduino-esp32/package_esp32_index.json` |
| Raspberry Pi Pico (RP2040) | **Raspberry Pi Pico/RP2040 by Earle Philhower** | `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json` |
| Arduino Uno / Nano (AVR) | **Arduino AVR Boards** (bundled) | – |
| STM32 | **STM32 MCU based boards (STM32duino)** | `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json` |

- Serial Monitor at **115200 baud**

## Examples

See the 7 examples in the [main README](../README.md#examples-7).

## Rebuilding the ZIP after edits

```bash
chmod +x make_zip.sh
./make_zip.sh
```

The script strips junk files (`.DS_Store`, `*.o`, `*~`, dot-folders) that make the Arduino IDE complain on import.

> แก้โค้ดแล้วรัน `make_zip.sh` เพื่อสร้าง ZIP ใหม่ สคริปต์ตัดไฟล์ขยะให้เอง

## If the IDE rejects the ZIP

| Message | Cause | Fix |
|---|---|---|
| `A library named Massmore_BME280 already exists` | Already installed | Delete the old folder in `libraries/` and import again |
| `Specified folder/zip file does not contain a valid library` | Wrong ZIP, or a self-made ZIP with a double-nested folder | Use the provided `Massmore_BME280.zip` or rebuild with `make_zip.sh` |
| Imported but no examples shown | IDE has not rescanned yet | Restart the Arduino IDE |

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
