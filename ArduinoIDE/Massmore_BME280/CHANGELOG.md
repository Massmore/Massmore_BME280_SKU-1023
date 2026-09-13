# บันทึกการเปลี่ยนแปลง

รูปแบบเวอร์ชันตาม [Semantic Versioning](https://semver.org/lang/th/)

## [1.1.0] - 2026-09-13

ปรับให้ตรงตาม Massmore Library Standard

### เพิ่ม

- รองรับบัส **SPI 4 สาย** ผ่าน `beginSPI(csPin, SPIClass&, spiHz)` (mode 0, สูงสุด 10 MHz)
- FSM ไม่บล็อกชื่อมาตรฐาน `requestConversion()` / `update()` / `isDataReady()` /
  `getReadings()` / `getState()` (rollover-safe `millis()`, มี timeout)
- `begin(address, TwoWire&)` รับบัสแบบ reference (ส่ง `Wire1` ได้) และ `getBus()` / `isSPI()` / `getCSPin()`
- `MASSMORE_BME280_I2C_ADDR_DEFAULT` = **0x77** ตามฮาร์ดแวร์บอร์ด SKU-1023 (SDO ดึงขึ้น VDDIO)
- ตัวอย่างชุดใหม่ 7 ชุด : `01_BasicRead`, `02_CustomPins_BusRemap`, `03_NonBlocking_Multitask`,
  `07_Factory_Test`, `04_SPI_Advance`, `05_LowPower_ForcedMode`, `06_ChipID_Genuine`
  ทุกชุดคอมไพล์ผ่านบน ESP32 / ESP32-S3 / RP2040 / AVR (Uno, Nano)
- `07_Factory_Test` จบด้วย `[PASS] SENSOR QA PASSED - READY TO SHIP` หรือ
  `[FAIL] QA CHECK FAILED: <REASON>` และรันบน AVR ได้ (สตริงอยู่ใน PROGMEM)
- ชุดทดสอบบนเครื่อง PC เพิ่มกลุ่ม SPI และ FSM รวม 192 ข้อ

### เปลี่ยน

- **ไลบรารีไม่เรียก `Wire.begin()` อีกต่อไป** sketch ต้องเปิดบัสและเลือกขาเองก่อน `begin()`
  (ตามหลัก Zero Pin Hardcoding ทำให้ใช้ได้ทุก MCU)
- `beginAuto()` ลอง 0x77 ก่อนแล้วจึง 0x76
- บน AVR `errorToString()` และตัวช่วยแปลงข้อความอื่นคืน ASCII สั้น ๆ เพื่อประหยัด SRAM
- `read()` แบบบล็อกจะปฏิเสธ (`ERR_NOT_READY`) ขณะ FSM กำลังวัดค้างอยู่

### ลบ

- ตัวอย่างชุดเก่า 10 ชุด (เนื้อหาถูกรวมเข้าไปในชุดใหม่ 7 ชุด)

## [1.0.0] - 2026-09-04

เวอร์ชันแรก

### เพิ่ม

- ไดรเวอร์ BME280 เขียนจากดาต้าชีต BST-BME280-DS002 โดยตรง
  ไม่พึ่งไลบรารีอื่นนอกจาก `Wire`
- สูตรชดเชยแบบจำนวนเต็มครบทั้งสามช่อง (int32 สำหรับอุณหภูมิและความชื้น,
  int64 สำหรับความดัน) ให้ผลตรงกับตัวเลขตัวอย่างในดาต้าชีตทุกหลัก
- oversampling x1 / x2 / x4 / x8 / x16 ตั้งแยกได้ทีละช่อง และปิดช่องที่ไม่ใช้ได้
- โหมด sleep / forced / normal พร้อมโหมดไม่บล็อก
  `startForcedMeasurement()` / `isMeasurementReady()`
- ฟิลเตอร์ IIR ครบทั้งห้าระดับ และเวลาพักระหว่างรอบครบทั้งแปดค่า
- ชุดตั้งค่าสำเร็จรูปสี่ชุดตามที่ Bosch แนะนำ (weather station,
  humidity sensing, indoor navigation, gaming)
- อ่านค่าดิบ ADC ทั้งสามช่องพร้อม `t_fine` และค่าชดเชยจากโรงงานทั้ง 32 ตัว
- `verifyChip()` ตรวจ 10 ข้อว่าเป็นชิป Bosch ของแท้ แยก BMP280 (0x58)
  ออกจาก BME280 (0x60) ได้ชัดเจน
- คำนวณความสูง ปรับเทียบระดับน้ำทะเล จุดน้ำค้าง ความชื้นสัมบูรณ์
  และความดันไออิ่มตัว
- ค่าชดเชยอุณหภูมิ ความชื้น และความดันที่ผู้ใช้ตั้งเองได้
- รหัสข้อผิดพลาดพร้อมคำอธิบายภาษาไทยครบทุกตัว
- ตัวอย่าง 10 ชุด รวมชุดทดสอบโรงงาน `10_FactoryTest`
- ชุดทดสอบที่รันบนเครื่อง PC ได้ 144 ข้อ (`PlatformIO/test`)
