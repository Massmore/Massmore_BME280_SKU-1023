# Massmore BME280 — Environment Sensor (SKU-1023)

<p align="center">
  <img src="Document/images/01_massmore_bme280_cover.png" alt="Massmore BME280 Environment Sensor" width="520">
</p>

<p align="center">
  <b>Arduino / PlatformIO library for the Bosch BME280 (I²C + SPI)</b><br>
  Temperature · Relative Humidity · Barometric Pressure · Altitude<br>
  <sub><b>Designed and Manufactured by Massmore</b> · Massmore Biz Co., Ltd.</sub><br>
  <sub>ไลบรารีสำหรับเซ็นเซอร์ Bosch BME280 วัดอุณหภูมิ ความชื้น ความดัน และคำนวณความสูง</sub>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.1.0-blue" alt="version">
  <img src="https://img.shields.io/badge/license-MIT-green" alt="license">
  <img src="https://img.shields.io/badge/ESP32%20core-3.x-orange" alt="esp32 core">
  <img src="https://img.shields.io/badge/MCU-ESP32%20%7C%20S3%20%7C%20RP2040%20%7C%20STM32%20%7C%20AVR-informational" alt="mcu">
  <img src="https://img.shields.io/badge/tests-192%20passed-brightgreen" alt="tests">
  <img src="https://img.shields.io/badge/heap-free-lightgrey" alt="no heap">
</p>

---

## Table of Contents

- [Product Overview](#product-overview)
- [Pre-flight Datasheet Verification](#pre-flight-datasheet-verification)
- [MCU Compatibility & Limitation Matrix](#mcu-compatibility--limitation-matrix)
- [Pinout & Wiring](#pinout--wiring)
- [Where to Buy](#where-to-buy)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Pin Mapping Examples](#pin-mapping-examples)
- [API Reference](#api-reference)
- [Examples (7)](#examples-7)
- [Genuine Chip Verification](#genuine-chip-verification)
- [Factory Test (QA/QC)](#factory-test-qaqc)
- [Host Test Suite](#host-test-suite)
- [Troubleshooting](#troubleshooting)
- [Repository Layout](#repository-layout)
- [Massmore Pre-Release Audit & Missing Information Report](#massmore-pre-release-audit--missing-information-report)

---

## Product Overview

<p align="center">
  <img src="Document/images/02_massmore_bme280_product.png" alt="Massmore BME280 board" width="420">
</p>

The **Massmore BME280 (SKU-1023)** is a breakout module for the Bosch BME280 environmental sensor with an on-board
3.3 V regulator and level shifting. It accepts 3–5 V and supports both **I²C (Qwiic / STEMMA QT)** and **4-wire SPI**.

> 🇹🇭 บอร์ด BME280 ของ Massmore มีเรกูเลเตอร์และ level shifter ในตัว รับไฟ 3–5 V ต่อได้ทั้ง I²C (Qwiic) และ SPI 4 สาย

| | Details |
|---|---|
| **Straight from the datasheet** | Every compensation formula is taken from Bosch **BST-BME280-DS002** §4.2.3; results match the datasheet example values exactly |
| **Zero pin hardcoding** | The library never calls `Wire.begin()` / `SPI.begin()` and knows no pin numbers. The sketch owns the bus, so pins can be remapped freely on any MCU |
| **I²C + SPI** | `begin(addr, Wire)` or `beginSPI(cs, SPI)` — everything else is identical |
| **Dual API** | Simple blocking `read()` and a non-blocking FSM: `requestConversion()` / `update()` / `isDataReady()` / `getReadings()` |
| **Heap-free** | No `new` / `malloc` / `String` in the core; runs on ATmega328P (2 KB SRAM) |
| **10-point genuine check** | `verifyChip()` separates **BMP280 (0x58)** from **BME280 (0x60)** by probing real silicon behaviour |
| **Factory Test** | `07_Factory_Test` runs 22 checks and ends with `[PASS] SENSOR QA PASSED - READY TO SHIP` |
| **192 host tests** | Test suite runs on a PC against a simulated chip (I²C and SPI) — no hardware needed |

### Measurement specifications

| Quantity | Range | Resolution | Accuracy |
|---|---|---|---|
| Temperature | −40 to +85 °C | 0.01 °C | ±0.5 °C (25 °C) · ±1.0 °C (0–65 °C) |
| Relative humidity | 0 to 100 %RH | 0.008 %RH | ±3 %RH (20–80 %RH) |
| Barometric pressure | 300 to 1100 hPa | 0.18 Pa | ±1 hPa (absolute) · ±0.12 hPa (relative) |
| Altitude (derived) | 0 to ~9000 m | ~0.17 m | Depends on sea-level calibration |

### Electrical & interface

| Item | Value |
|---|---|
| Board supply | 3–5 V (on-board regulator) · `3Vo` pin provides 3.3 V out |
| Current: sleep / 1 Hz / normal x16 | 0.1 µA / ~3.6 µA / ~340 µA |
| I²C | 100 kHz · 400 kHz (chip supports up to 3.4 MHz) · address **`0x77` (default)** / `0x76` (SDO → GND) |
| SPI | 4-wire, mode 0/3, up to **10 MHz** (library uses mode 0, clamps at 10 MHz) |
| Connectors | 2× Qwiic / STEMMA QT (daisy-chainable) + 7 solder pads |
| Board size | 25.40 × 20.32 mm · M2 mounting holes |

---

## Pre-flight Datasheet Verification

Summary of the [BST-BME280-DS002](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf) facts the driver is built on.

> 🇹🇭 สรุปค่าจากดาต้าชีตที่ไลบรารีใช้อ้างอิง (ขั้น Step 0 ตามมาตรฐาน Massmore)

| Item | Datasheet | Used in library |
|---|---|---|
| **Device Identification** | Register `0xD0` = **`0x60`** (BMP280 = `0x58`, BME680 = `0x61`). No silicon-revision register | `begin()` rejects anything but 0x60 |
| **Bus – I²C** | Up to 3.4 MHz · `0x76` (SDO = GND) / `0x77` (SDO = VDDIO) · SDO must not float | `MASSMORE_BME280_I2C_ADDR_DEFAULT = 0x77` |
| **Bus – SPI** | 4-wire, mode 0/3, ≤ 10 MHz · control byte bit 7 = R/W · auto-increment | `beginSPI()` mode 0, clamp 10 MHz, `spi3w_en = 0` |
| **Boot & Reset Timing** | 2 ms start-up after power-on · soft reset = write `0xB6` to `0xE0`, then wait for `status[0] im_update = 0` | NVM-copy wait with 100 ms timeout |
| **Data Protocol** | Direct register access, no CRC/packets · calib `0x88–0xA1`, `0xE1–0xE7` · data `0xF7–0xFE` burst read (shadowing) | `read()` reads 8 bytes in one burst |
| **Authenticity / Lot Clues** | Bosch publishes no signature / trim register | 10-point heuristic in `verifyChip()` — `// TODO: [MASSMORE_INPUT_REQUIRED: Factory trim signature register]` |

---

## MCU Compatibility & Limitation Matrix

All 7 examples compile on the 4 environments in `PlatformIO/platformio.ini` with 0 errors and 0 warnings from library code.

> 🇹🇭 ตัวอย่างทั้ง 7 ชุดคอมไพล์ผ่านทุก env; STM32 ผ่านตามหลักการแต่ยังไม่ได้ทดสอบบนบอร์ดจริง

| MCU Platform | Tested Core / Toolchain | Bus Remapping Support | Limitations / Notes |
|---|---|---|---|
| **ESP32-S3** | Arduino-ESP32 **v3.3.11** (pioarduino 55.03.311) | Full GPIO matrix · `Wire`/`Wire1` · `SPI.begin(sck, miso, mosi, -1)` | None. Recommended for high-rate data / 10 MHz SPI. USB-CDC enabled in env |
| **ESP32 (Classic)** | Arduino-ESP32 **v3.3.11** (pioarduino 55.03.311) | Full GPIO matrix · `Wire.begin(sda, scl)` · `Wire1` | None. Default SDA 21 / SCL 22 · VSPI 18/19/23 |
| **RP2040 (Pico)** | Arduino-Pico (Earle Philhower) | I2C0/I2C1 and SPI0/SPI1 pins per pinmux · `Wire.setSDA()/setSCL()` · `SPI.setSCK()/setRX()/setTX()` | Call `setSDA/setSCL` before `begin()` (see `02_CustomPins_BusRemap`) |
| **AVR (ATmega328P)** Uno / Nano | Arduino AVR Core (atmelavr) | Fixed hardware pins: I²C A4/A5 · SPI 11/12/13 | 2 KB SRAM / 32 KB flash — use the Simple API, `*ToString()` returns ASCII, `07_Factory_Test` skips 4 checks (18 remain); old-bootloader Nano has 1.2 % flash headroom |
| **STM32** | STM32duino | I2C1/2/3, SPI1/2/3 · `Wire.setSDA()/setSCL()` before `begin()` | Compiles by design (no platform-specific code) but **not yet verified on hardware** |

---

## Pinout & Wiring

<p align="center">
  <img src="Document/images/04_massmore_bme280_pinout_dimension.png" alt="pinout and dimensions" width="520">
</p>

| Pin | I²C function | SPI function | Notes (🇹🇭 หมายเหตุ) |
|---|---|---|---|
| `VIN` | Power in **3–5 V DC** | Power in 3–5 V DC | Connect to `3V3` or `5V` of the MCU |
| `3Vo` | 3.3 V regulator output | same | Optional; can power other modules |
| `GND` | Ground | Ground | |
| `SCK` | **SCL** | **SCK** | |
| `SDO` | Address select (**float/VDDIO = 0x77**, GND = 0x76) | **MISO** | Board default = 0x77 (ค่าปริยาย 0x77) |
| `SDI` | **SDA** | **MOSI** | |
| `CS` | Leave open (pulled up = I²C mode) | **Chip Select** (active low) | Must not float in SPI mode |
| `Qwiic` ×2 | SDA / SCL / 3V3 / GND | – | Daisy-chain multiple modules |

<p align="center">
  <img src="Document/images/03_massmore_bme280_wiring_esp32.png" alt="wiring with ESP32" width="560">
</p>

> **Two sensors on one I²C bus?** Tie `SDO` of the second board to `GND` to move it to `0x76`, then chain it via the
> second Qwiic connector and call `bme2.begin(MASSMORE_BME280_I2C_ADDR_A)`.
> 🇹🇭 ต่อ SDO ของบอร์ดที่สองลง GND เพื่อย้ายไป 0x76

---

## Where to Buy

| Channel | Link |
|---|---|
| **Massmore Official Store** | <https://www.massmore.shop/products/bf997665-da75-4b6f-9a98-100a5dc4030f> |
| Shopee | `// TODO: [MASSMORE_INPUT_REQUIRED: Shopee product link]` |
| Lazada | `// TODO: [MASSMORE_INPUT_REQUIRED: Lazada product link]` |

Warranty included · Tax invoice available · Ships daily
🇹🇭 สินค้ามีรับประกัน ออกใบกำกับภาษีได้ จัดส่งทุกวัน

---

## Installation

### Arduino IDE

1. Download this repository as ZIP or `git clone`
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…**
3. Select **`ArduinoIDE/Massmore_BME280.zip`**
4. Open an example from **File → Examples → Massmore_BME280**

No external dependencies (only `Wire` / `SPI` from the core). Board-package details per MCU: [`ArduinoIDE/README.md`](ArduinoIDE/README.md)

> 🇹🇭 ติดตั้งผ่าน Add .ZIP Library แล้วเปิดตัวอย่างได้ทันที ไม่ต้องลงไลบรารีอื่นเพิ่ม

### VS Code + PlatformIO (plug-and-play)

```bash
git clone https://github.com/Massmore/Massmore_BME280_SKU-1023.git
code Massmore_BME280_SKU-1023/PlatformIO
cp examples/01_BasicRead/main.cpp src/main.cpp
pio run -e esp32-s3-devkitc-1 -t upload -t monitor      # or esp32dev / pico / uno / nanoatmega328
```

`platformio.ini` ships ready-made environments (`esp32-s3-devkitc-1`, `esp32dev`, `pico`, `uno`, `nanoatmega328`).
ESP32 is pinned to **pioarduino 55.03.311 = Arduino-ESP32 core 3.3.11** for reproducible builds.

---

## Quick Start

```cpp
#include <Massmore_BME280.h>
#include <Wire.h>

MassmoreBME280 bme;

void setup() {
  Serial.begin(115200);
  Wire.begin();          // the sketch owns the bus and picks the pins (ESP32: Wire.begin(21, 22))
  bme.begin();           // default 0x77 on Wire, verifies chip id
}

void loop() {
  massmore_bme280_reading_t r;
  if (bme.read(r)) {
    Serial.print(r.temperature); Serial.print(" C  ");
    Serial.print(r.humidity);    Serial.print(" %RH  ");
    Serial.print(r.pressure);    Serial.println(" hPa");
  }
  delay(2000);
}
```

**SPI** — only two lines change (🇹🇭 ใช้ SPI เปลี่ยนแค่สองบรรทัด)

```cpp
SPI.begin();               // ESP32: SPI.begin(18, 19, 23, -1)
bme.beginSPI(5, SPI);      // CS = GPIO5, 1 MHz (3rd argument raises it up to 10 MHz)
```

**Non-blocking FSM** — no `delay()` in `loop()` (🇹🇭 ไม่บล็อก loop เลย)

```cpp
void loop() {
  bme.update();                                   // advance the FSM one step
  if (bme.isDataReady()) {
    massmore_bme280_reading_t r;
    bme.getReadings(r);                           // fetch result, FSM returns to IDLE
  } else if (bme.getState() == MASSMORE_BME280_STATE_IDLE) {
    bme.requestConversion();                      // start the next conversion
  }
  /* ...other tasks run here without stalling... */
}
```

---

## Pin Mapping Examples

The library hardcodes no pins; every pin lives in the sketch (see `02_CustomPins_BusRemap` and `04_SPI_Advance`).

> 🇹🇭 หมายเลขขาทั้งหมดอยู่ใน sketch เท่านั้น ไลบรารีไม่รู้จักขาใด ๆ

### ESP32 / ESP32-S3 (Arduino-ESP32 core v3.x) — any GPIO via the matrix

```cpp
Wire.begin(16, 17, 400000UL);          // I2C on GPIO16/17 at 400 kHz
bme.begin(0x77, Wire);

Wire1.begin(8, 9);                     // core 3.x provides Wire1 (second bus)
bme2.begin(0x77, Wire1);

SPI.begin(12, 13, 11, -1);             // S3: SCK 12, MISO 13, MOSI 11 (ss = -1, the library drives CS)
bme3.beginSPI(10, SPI, 10000000UL);    // CS = GPIO10 at 10 MHz
```

### RP2040 (Arduino-Pico) — only pins that belong to that bus per pinmux

```cpp
Wire1.setSDA(6);  Wire1.setSCL(7);  Wire1.begin();      // I2C1 on GP6/GP7
bme.begin(0x77, Wire1);

SPI.setSCK(18); SPI.setRX(16); SPI.setTX(19); SPI.begin(); // SPI0
bme2.beginSPI(17, SPI);
```

### AVR (Arduino Uno / Nano) — fixed hardware pins

```cpp
Wire.begin();              // SDA = A4, SCL = A5
bme.begin();               // 0x77

SPI.begin();               // SCK 13, MISO 12, MOSI 11
bme2.beginSPI(10, SPI);    // CS = D10
```

### STM32 (STM32duino)

```cpp
Wire.setSDA(PB9); Wire.setSCL(PB8); Wire.begin();   // remap before begin()
bme.begin(0x77, Wire);
```

---

## API Reference

### Initialisation / bus

| Function | Returns | Description |
|---|---|---|
| `begin(address = 0x77, TwoWire &wirePort = Wire)` | `bool` | I²C init: chip-id check, soft reset, read calibration, apply defaults |
| `begin(address, TwoWire *wire)` | `bool` | Pointer overload (v1.0 compatible) |
| `beginSPI(csPin, SPIClass &spiPort = SPI, spiHz = 1 MHz)` | `bool` | 4-wire SPI init (clamped to 10 MHz) |
| `beginAuto(wire)` | `bool` | Try 0x77, then 0x76 |
| `getBus()` / `isSPI()` / `getAddress()` / `getCSPin()` | | Active bus info |
| `isConnected()` | `bool` | I²C = ACK, SPI = chip id not 0x00/0xFF |

### Simple API (blocking)

| Function | Returns | Description |
|---|---|---|
| `read(reading)` | `bool` | All values from the same conversion in one burst **(recommended)** |
| `readTemperature()` / `readPressure()` / `readPressurePa()` / `readHumidity()` / `readAltitude(seaLevelhPa)` | `float` | `NAN` on failure |
| `takeForcedMeasurement()` | `bool` | One forced conversion, waits for completion |

### Advanced non-blocking FSM

| Function | Description |
|---|---|
| `requestConversion()` | Start a conversion → `MEASURING` (refused while busy) |
| `update()` | Call every `loop()`; when the datasheet time has elapsed and the measuring bit is clear → `READY` (timeout → `ERROR`) |
| `isDataReady()` | `true` when state is `READY` |
| `getReadings(reading)` | Copy the result and return to `IDLE` |
| `getState()` | `IDLE` / `MEASURING` / `READY` / `ERROR` |
| `startForcedMeasurement()` / `isMeasurementReady()` | Lower-level primitives (v1.0) still available |

### Configuration

| Function | Description |
|---|---|
| `setSampling(mode, osrsT, osrsP, osrsH, filter, standby)` | Set everything at once; register write order follows the datasheet (sleep → config → ctrl_hum → ctrl_meas) |
| `setMode()` · `set{Temperature,Pressure,Humidity}Oversampling()` · `setFilter()` · `setStandbyTime()` | Individual settings (read-modify-write from cached state) |
| `useWeatherStationPreset()` · `useHumiditySensingPreset()` · `useIndoorNavigationPreset()` · `useGamingPreset()` | Bosch-recommended presets (datasheet §3.5) |
| `reset()` | Soft reset, then re-apply the user settings |
| `measurementTimeMs()` / `measurementTimeMaxMs()` | Conversion time per datasheet §9.1 |

### Raw data / identity / derived values

| Function | Description |
|---|---|
| `readRawADC(raw)` · `getCalibration(calib)` · `readCalibration()` | Raw ADC + `t_fine` and the 32 calibration words |
| `readRegister(reg, v)` · `writeRegister(reg, v)` · `readStatus(s)` | Direct register access |
| `getChipID()` · `getChipType()` · `verifyChip(&id)` · `isGenuine()` | 10-point identity check |
| `dewPoint(t, rh)` · `absoluteHumidity(t, rh)` · `saturationVaporPressure(t)` · `seaLevelForAltitude(alt, p)` | static helpers |
| `set{Temperature,Pressure,Humidity}Offset()` · `setSeaLevelPressure()` | User offsets |
| `lastError()` · `errorToString()` | `massmore_bme280_error_t`, 12 codes (Thai text; ASCII on AVR) |

---

## Examples (7)

Every example shares one source for Arduino IDE (`.ino`) and PlatformIO (`main.cpp`) and compiles on ESP32-S3 / ESP32 / RP2040 / AVR.

| # | Example | What you learn (🇹🇭 สิ่งที่ได้เรียนรู้) |
|---|---|---|
| 01 | **BasicRead** | All four values + dew point; `busBegin()` picks pins per MCU automatically (อ่านค่าพื้นฐาน เริ่มตรงนี้) |
| 02 | **CustomPins_BusRemap** | Remap pins / use `Wire1` / RP2040 `setSDA` / STM32 remap; pass the bus by reference (ย้ายขาและใช้บัสที่สอง) |
| 03 | **NonBlocking_Multitask** | FSM `requestConversion / update / isDataReady / getReadings` + LED blink + proof that `loop()` never stalls (FSM ไม่บล็อก) |
| 04 | **SPI_Advance** | 4-wire SPI, per-MCU pin setup, bus-stability check, real read-rate measurement (ใช้บัส SPI) |
| 05 | **LowPower_ForcedMode** | Forced mode + ESP32 deep sleep, RTC memory, pressure-trend warning (ประหยัดไฟ) |
| 06 | **ChipID_Genuine** | Bus scan, chip id, 10-point genuine check, chip fingerprint (ตรวจของแท้) |
| 07 | **Factory_Test** | 22-point factory QA ending with `[PASS] SENSOR QA PASSED - READY TO SHIP` (ชุดทดสอบโรงงาน) |

---

## Genuine Chip Verification

```cpp
massmore_bme280_identity_t id;
massmore_bme280_genuine_t verdict = bme.verifyChip(&id);   // YES / SUSPECT / NO
```

> 🇹🇭 ดูแค่ chip id ไม่พอ เพราะของปลอมคัดลอกตัวเลขได้ จึงทดสอบพฤติกรรมจริงของซิลิคอนอีก 9 ข้อ

| # | Check | Why clones fail it |
|---|---|---|
| 1 | `chipIdOk` | Register 0xD0 must read 0x60 |
| 2–3 | `calibTempOk` · `calibPressOk` | Bosch factory `dig_T*` / `dig_P*` ranges are narrower than outsiders guess |
| 4 | `calibHumOk` | **A BMP280 relabelled as BME280 has no humidity calibration at all** |
| 5 | `calibUniqueOk` | Real calibration words never repeat like a hand-typed table |
| 6 | `resetOk` | Soft reset must really clear `ctrl_meas` / `ctrl_hum` / `config` to 0x00 |
| 7 | `ctrlHumLatchOk` | With `osrs_h` off the raw value must be `0x8000`, on it must not — BME280-specific latch behaviour |
| 8 | `registerEchoOk` | Writing `config` must read back bit-exact |
| 9 | `measuringBitOk` | The measuring bit must rise during conversion and clear afterwards |
| 10 | `humidityLiveOk` | The computed humidity must be physically plausible |

**Verdict** — all 10 = `GENUINE` · 8–9 = `SUSPECT` · < 8 or fails #1/#7 = `NO`

---

## Factory Test (QA/QC)

`07_Factory_Test` runs automatically after boot; type `r` to repeat. Serial Monitor at **115200 baud**.

> 🇹🇭 รันเองหลังบูต พิมพ์ r เพื่อทดสอบซ้ำ บรรทัดสุดท้ายบอกผล PASS/FAIL

```
GATE 1    Scan the I2C bus and confirm the address (0x77 / 0x76)
GATE 2    Read CHIP_ID (0x60) — 0x58 is reported explicitly as BMP280 — then begin()
RUN TEST  SOFT_RESET · CALIB_READ/RANGE/HUM (trim registers) · REG_ECHO · STATUS_REG ·
          SLEEP/FORCED/NORMAL_MODE · HUM_CHANNEL · MEAS_TIMING · NOISE · IIR_FILTER ·
          READ_VALUES · TEMP/HUM/PRES_RANGE (physical ranges) · DEWPOINT · ALTITUDE ·
          STABILITY · BUS_400K · PRESETS · GENUINE (10-point heuristic)
```

Last line of the report:

```
[PASS] SENSOR QA PASSED - READY TO SHIP
[FAIL] QA CHECK FAILED: <REASON>          (REASON = first failing check name)
```

Lines starting with `#` are machine-readable for a web tool:

```
#RESULT,<index>,<check>,<PASS|FAIL|WARN>,<detail>
#DEVICE,<addr>,<chip>,<chip_id>,<GENUINE|SUSPECT|FAKE|UNKNOWN>,<passed_of_10>
#VERDICT,<PASS|FAIL>,<pass>,<fail>,<warn>
```

> On AVR (32 KB flash) `MEAS_TIMING` `NOISE` `IIR_FILTER` `BUS_400K` are skipped (18 checks remain).
> Pre-built ESP32 binaries live in [`Firmware/`](Firmware/) (see [`Firmware/README.md`](Firmware/README.md)).

---

## Host Test Suite

```bash
cd PlatformIO/test && make
#  192 passed   0 failed   192 total
```

Covers: compensation formulas against datasheet example values (`adc_T = 519888 → 25.08 °C`, `adc_P = 415148 → 100653.27 Pa`) ·
`dig_H4/H5` decoding · register write order · timing formulas · error paths · BMP280 rejection ·
**SPI bus** (control byte, mode 0, 10 MHz clamp, no-chip case) · **FSM** (IDLE → MEASURING → READY → IDLE, timeout → ERROR) ·
proof that the library **never** calls `Wire.begin()` / `SPI.begin()`

> 🇹🇭 ชุดทดสอบ 192 ข้อรันบน PC ด้วย g++ ไม่ต้องมีบอร์ด

---

## Troubleshooting

| Symptom | Most likely cause | Fix (🇹🇭 วิธีแก้) |
|---|---|---|
| `begin()` fails · "no device" | `Wire.begin()` not called in the sketch (the library no longer does it) or SDI/SCK swapped | Call `Wire.begin(...)` first, then run `06_ChipID_Genuine` to scan the bus |
| Scan finds `0x76` instead of `0x77` | `SDO` is tied to GND | `bme.begin(MASSMORE_BME280_I2C_ADDR_A)` or `beginAuto()` |
| `beginSPI()` fails (chip id 0x00/0xFF) | SDI/SDO swapped, Qwiic still plugged in (CS pulled high), or clock too fast for long wires | Check SDI→MOSI, SDO→MISO, unplug Qwiic, drop `spiHz` to 1 MHz |
| "chip id is not 0x60" | The board is a **BMP280** | BMP280 is not supported by this library |
| `read()` returns `ERR_NOT_READY` | FSM conversion in flight (`requestConversion()` without `getReadings()`) | Use one API style per cycle |
| Humidity is `NAN` | `osrs_h = NONE` | `setHumidityOversampling(X1)` |
| Temperature 1–3 °C too high | Heat from the MCU | Move the board away or `setTemperatureOffset(-1.5f)` |
| AVR "program size" error | Old-bootloader Nano has 30 KB flash | Use `board = nanoatmega328new` or `uno` (Factory Test uses 98.8 % of 30 KB) |
| Upload fails on macOS | `upload_speed` too high | Lower to `460800` / `115200` in `platformio.ini` |

---

## Repository Layout

```
Massmore_BME280_SKU-1023/
├── README.md                          <- this file (showcase, pinout, MCU matrix, where to buy)
├── ArduinoIDE/
│   ├── README.md
│   ├── Massmore_BME280.zip            <- ready for Add .ZIP Library
│   ├── make_zip.sh
│   └── Massmore_BME280/
│       ├── library.properties · keywords.txt · CHANGELOG.md · LICENSE
│       ├── src/  Massmore_BME280.h · Massmore_BME280.cpp · Massmore_BME280_Registers.h
│       └── examples/
│           ├── 01_BasicRead/  02_CustomPins_BusRemap/  03_NonBlocking_Multitask/
│           ├── 04_SPI_Advance/  05_LowPower_ForcedMode/  06_ChipID_Genuine/
│           └── 07_Factory_Test/                       (mandatory QA/QC)
├── PlatformIO/
│   ├── platformio.ini                 esp32-s3-devkitc-1 · esp32dev · pico · uno · nanoatmega328
│   ├── src/main.cpp                   the example currently in use
│   ├── include/
│   ├── lib/Massmore_BME280/           same sources as the Arduino IDE tree (byte-identical)
│   ├── examples/                      7 examples (main.cpp)
│   └── test/                          192 host tests (simulated I2C + SPI chip)
├── Document/
│   ├── README.md                      reserved for schematics / user-supplied images
│   └── images/
├── Firmware/
│   ├── README.md                      flashing manual, wiring notes, expected report
│   └── bin/                           pre-built Factory Test binaries + flash scripts
├── .github/workflows/build.yml        CI: host tests + PlatformIO matrix + arduino-cli (4 FQBN)
└── LICENSE
```

---

## References

- Bosch Sensortec **BST-BME280-DS002** — <https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf>
- Espressif **Arduino ESP32 core 3.x** — <https://github.com/espressif/arduino-esp32>
- **pioarduino** platform-espressif32 — <https://github.com/pioarduino/platform-espressif32>
- **Arduino-Pico** (Earle Philhower) — <https://github.com/earlephilhower/arduino-pico>

## License

MIT License — see [LICENSE](LICENSE)

---

## Massmore Pre-Release Audit & Missing Information Report

Items that were **not guessed** and are marked in code/docs with `// TODO: [MASSMORE_INPUT_REQUIRED: ...]`.

> 🇹🇭 รายการข้อมูลที่ยังขาดและไม่ได้เดา มาร์ก TODO ไว้ในโค้ดและเอกสาร

### Missing Datasheet Registers / Silicon details

| Item | Status | Location |
|---|---|---|
| Factory trim / signature register to separate genuine from clone | Not published by Bosch — 10-point heuristic used instead | `Massmore_BME280.h` header, README Pre-flight section |
| Silicon-revision register | BME280 has none — Factory Test shows CHIP_ID + trim registers (`CALIB_READ`) | – |

### Missing Hardware Pinouts / Board revisions

| Item | Status | Location |
|---|---|---|
| Schematic PDF of SKU-1023 | Not in repository | `Document/README.md` |
| PCB revision number and production date | Not provided | `Document/README.md` |
| Confirmation that SDO is pulled to VDDIO on the board (default = 0x77) | Taken from user input — should be confirmed against the schematic | README Pinout section |
| Factory Test binaries v1.1.0 (esp32dev / esp32-s3) | Built, awaiting hardware verification | `Firmware/README.md` |
| STM32 hardware test result | Compiles by design, not tested on hardware | MCU matrix |

### Missing Commercial Links (Shopee, Lazada, Docs)

| Item | Status |
|---|---|
| Shopee product link | `// TODO: [MASSMORE_INPUT_REQUIRED: Shopee product link]` |
| Lazada product link | `// TODO: [MASSMORE_INPUT_REQUIRED: Lazada product link]` |
| Massmore Official Store | ✅ <https://www.massmore.shop/products/bf997665-da75-4b6f-9a98-100a5dc4030f> |
| Datasheet | ✅ Bosch link above (copy not yet placed in `Document/datasheet/`) |

---

<p align="center">
  <b>Designed and Manufactured by Massmore</b> · <a href="https://www.massmore.shop">massmore.shop</a><br>
  <sub>Massmore Biz Co., Ltd. — Warranty included · Tax invoice available · Ships daily</sub>
</p>
