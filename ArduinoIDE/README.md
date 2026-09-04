# โฟลเดอร์สำหรับ Arduino IDE

โฟลเดอร์ที่ต้องติดตั้งคือ **`Massmore_BME280/`** (โฟลเดอร์ที่มี `library.properties` อยู่ข้างใน)
ไม่ใช่โฟลเดอร์ `ArduinoIDE` นี้

## ไฟล์ในโฟลเดอร์นี้

| ไฟล์ / โฟลเดอร์ | คำอธิบาย |
|---|---|
| `Massmore_BME280.zip` | **ไฟล์พร้อมติดตั้ง** ใช้กับ Add .ZIP Library ได้ทันที |
| `Massmore_BME280/` | ซอร์สจริงของไลบรารี (มี `library.properties` อยู่ข้างใน) |
| `make_zip.sh` | สคริปต์สร้างไฟล์ ZIP ใหม่หลังแก้โค้ด |

## วิธีติดตั้ง

### วิธีที่ 1 — จากไฟล์ ZIP ที่เตรียมไว้ให้แล้ว (แนะนำ)

1. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…**
2. เลือกไฟล์ **`Massmore_BME280.zip`** ในโฟลเดอร์นี้
3. เสร็จแล้วดูตัวอย่างที่ **File → Examples → Massmore_BME280**

> ไม่ต้องแตกไฟล์ ZIP เอง Arduino IDE จัดการให้ทั้งหมด
> และ **ห้ามเลือกโฟลเดอร์ `ArduinoIDE` นี้ทั้งโฟลเดอร์** เพราะข้างในมีสองโปรเจกต์ปนกัน
> IDE จะไม่รู้ว่าอันไหนคือไลบรารี

### วิธีที่ 2 — คัดลอกโฟลเดอร์

คัดลอก `Massmore_BME280` ทั้งโฟลเดอร์ไปวางที่

| ระบบปฏิบัติการ | ตำแหน่ง |
|---|---|
| macOS | `~/Documents/Arduino/libraries/` |
| Windows | `Documents\Arduino\libraries\` |
| Linux | `~/Arduino/libraries/` |

ปิดเปิด Arduino IDE ใหม่ แล้วดูตัวอย่างที่ **File → Examples → Massmore_BME280**

## สิ่งที่ต้องมีก่อน

- Arduino IDE 2.x
- ESP32 board package **core 3.x** (ติดตั้งผ่าน Boards Manager)
  ใส่ URL นี้ใน Preferences → Additional Boards Manager URLs
  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```
- เลือกบอร์ดให้ตรงกับที่ใช้ เช่น **ESP32 Dev Module**
- ตั้ง Serial Monitor ที่ **115200 baud**

## ตัวอย่างทั้งหมด

ดูรายละเอียดของทั้ง 10 ตัวอย่างได้ที่ [README หลักของรีโป](../README.md#ตัวอย่างทั้งหมด)

## แก้โค้ดแล้วอยากอัปเดตไฟล์ ZIP

หลังแก้ซอร์สหรือตัวอย่างในโฟลเดอร์ `Massmore_BME280/` ให้สร้าง ZIP ใหม่ด้วย

```bash
chmod +x make_zip.sh
./make_zip.sh
```

สคริปต์จะตัดไฟล์ขยะ (`.DS_Store`, `*.o`, `*~`, โฟลเดอร์ที่ขึ้นต้นด้วยจุด) ออกให้เอง
เพราะไฟล์พวกนี้ทำให้ Arduino IDE ขึ้นเตือนตอน import

## ถ้า Arduino IDE ไม่ยอมรับไฟล์ ZIP

| ข้อความที่ขึ้น | สาเหตุ | วิธีแก้ |
|---|---|---|
| `A library named Massmore_BME280 already exists` | เคยติดตั้งไว้แล้ว | ลบโฟลเดอร์เดิมใน `libraries/` ออกก่อน แล้ว import ใหม่ |
| `Specified folder/zip file does not contain a valid library` | เลือกไฟล์ ZIP ผิดตัว หรือ ZIP ที่บีบอัดเองมีโฟลเดอร์ซ้อนกันสองชั้น | ใช้ `Massmore_BME280.zip` ที่ให้มา หรือสร้างใหม่ด้วย `make_zip.sh` |
| import ผ่านแต่ไม่เห็นตัวอย่าง | IDE ยังไม่ได้สแกนใหม่ | ปิดเปิด Arduino IDE หนึ่งครั้ง |

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
