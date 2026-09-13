# โฟลเดอร์สำหรับ VS Code + PlatformIO

เปิดโฟลเดอร์ **นี้** ด้วย VS Code ได้เลย (ไม่ใช่โฟลเดอร์แม่)

```bash
code PlatformIO
```

ไลบรารีอยู่ใน `lib/Massmore_BME280/` อยู่แล้ว PlatformIO จะหาเจอเองโดยไม่ต้องตั้งค่าเพิ่ม
ไม่มี dependency ภายนอก (`lib_deps` ว่าง) ใช้แค่ `Wire` / `SPI` ที่มากับ core

## ใช้ตัวอย่าง

```bash
cp examples/01_BasicRead/main.cpp src/main.cpp
pio run -e esp32-s3-devkitc-1 -t upload -t monitor
```

## บอร์ดที่เตรียม env ไว้ให้ (ทั้งหมด build ผ่านกับทุกตัวอย่าง)

| env | บอร์ด | platform / core |
|---|---|---|
| `esp32-s3-devkitc-1` | ESP32-S3 (USB-CDC เปิดไว้แล้ว) | pioarduino 55.03.311 = Arduino-ESP32 core **3.3.11** |
| `esp32dev` | ESP32-WROOM-32 | pioarduino 55.03.311 = Arduino-ESP32 core **3.3.11** |
| `pico` | Raspberry Pi Pico (RP2040) | Arduino-Pico (Earle Philhower) |
| `uno` | Arduino Uno (ATmega328P) | atmelavr |
| `nanoatmega328` | Arduino Nano (ATmega328P) | atmelavr |

```bash
pio run                       # build ทั้ง 4 env ปริยาย
pio run -e uno -t upload      # เฉพาะ Uno
```

## รันชุดทดสอบบนเครื่อง PC

ไม่ต้องมีบอร์ด ไม่ต้องมี PlatformIO ใช้แค่ `g++`

```bash
cd test && make
```

ชุดทดสอบมี 192 ข้อ ครอบคลุมสูตรชดเชย การถอดค่าชดเชย ลำดับการเขียนรีจิสเตอร์
สูตรเวลาที่ใช้วัด บัส SPI (ชิปจำลอง) FSM ไม่บล็อก และการแยก BME280 ออกจาก BMP280

## Arduino core เวอร์ชันไหน

`platformio.ini` pin ไปที่ pioarduino `55.03.311` ซึ่งให้ **Arduino ESP32 core 3.3.11**
(ฐาน ESP-IDF v5.x) ตรงกับที่ Arduino IDE รุ่นล่าสุดใช้ และทำให้ผลการ build ซ้ำได้เหมือนเดิมทุกครั้ง

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
