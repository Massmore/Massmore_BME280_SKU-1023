# VS Code + PlatformIO folder

Open **this** folder in VS Code (not the parent).

```bash
code PlatformIO
```

The library lives in `lib/Massmore_BME280/`; PlatformIO picks it up automatically.
No external dependencies (`lib_deps` is empty) — only `Wire` / `SPI` from the core.

> เปิดโฟลเดอร์นี้ด้วย VS Code ไลบรารีอยู่ใน `lib/` แล้ว ไม่ต้องตั้งค่าเพิ่ม

## Using an example

```bash
cp examples/01_BasicRead/main.cpp src/main.cpp
pio run -e esp32-s3-devkitc-1 -t upload -t monitor
```

## Environments (every example builds on every one)

| env | Board | Platform / core |
|---|---|---|
| `esp32-s3-devkitc-1` | ESP32-S3 (USB-CDC enabled) | pioarduino 55.03.311 = Arduino-ESP32 core **3.3.11** |
| `esp32dev` | ESP32-WROOM-32 | pioarduino 55.03.311 = Arduino-ESP32 core **3.3.11** |
| `pico` | Raspberry Pi Pico (RP2040) | Arduino-Pico (Earle Philhower) |
| `uno` | Arduino Uno (ATmega328P) | atmelavr |
| `nanoatmega328` | Arduino Nano (ATmega328P) | atmelavr |

```bash
pio run                       # build the 4 default envs
pio run -e uno -t upload      # Uno only
```

## Host test suite

No board, no PlatformIO needed — just `g++`.

```bash
cd test && make
```

192 tests cover the compensation formulas, calibration decoding, register write order, timing formulas,
the SPI bus (simulated chip), the non-blocking FSM and BME280/BMP280 discrimination.

> ชุดทดสอบ 192 ข้อ รันบน PC ด้วย `make`

## Which Arduino core?

`platformio.ini` pins pioarduino `55.03.311`, which ships **Arduino ESP32 core 3.3.11** (ESP-IDF v5.x) — the same as the current Arduino IDE package — for reproducible builds.

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
