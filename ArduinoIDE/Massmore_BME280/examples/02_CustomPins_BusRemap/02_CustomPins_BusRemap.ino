/*
  02_CustomPins_BusRemap - ย้ายขา I2C / ใช้บัสที่สอง / เลือกบัสเองทุก MCU

  หัวใจของไลบรารี Massmore : ไลบรารี "ไม่รู้จัก" หมายเลขขาเลย และไม่เรียก Wire.begin()
  sketch เป็นเจ้าของบัส จึงย้ายขาไปไหนก็ได้ตามที่ MCU อนุญาต แล้วส่งบัสนั้นให้ begin()

    bme.begin(address, Wire1);   // ส่งบัสแบบ reference

  ตัวอย่างนี้แสดงวิธีย้ายขาบนแต่ละ MCU (เลือกอัตโนมัติจากบอร์ดที่คอมไพล์)

    ESP32 / ESP32-S3 (core 3.x)  GPIO matrix ย้ายได้แทบทุกขา และมี Wire1 ให้ใช้เพิ่ม
                                 Wire1.begin(SDA, SCL) หรือ Wire1.setPins(SDA, SCL)
    RP2040 (Arduino-Pico)        เลือกได้เฉพาะขาที่เป็นของ I2C0 / I2C1 ตาม pinmux
                                 Wire.setSDA(p); Wire.setSCL(p); ก่อน begin()
    STM32 (STM32duino)           Wire.setSDA(PBx); Wire.setSCL(PBx); ก่อน begin()
    AVR (Nano / Uno)             ขาฮาร์ดแวร์ตายตัว A4 = SDA, A5 = SCL ย้ายไม่ได้

  แก้ตัวเลขในบล็อก "ตั้งค่าขา" ด้านล่างให้ตรงกับการต่อสายจริงของคุณ

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

/* -------------------------------------------------------------------------
   ตั้งค่าขา - แก้ที่นี่ที่เดียว
   ------------------------------------------------------------------------- */
#if defined(ARDUINO_ARCH_ESP32)
  #define MY_SDA 16 /* ตัวอย่าง : ย้ายไป GPIO16 / GPIO17 (ใช้ได้ทั้ง ESP32 และ S3) */
  #define MY_SCL 17
  #define USE_SECOND_BUS 1 /* 1 = ใช้ Wire1, 0 = ใช้ Wire แต่ย้ายขา */
#elif defined(ARDUINO_ARCH_RP2040)
  #define MY_SDA 6 /* GP6 / GP7 เป็นคู่ของ I2C1 บน Pico */
  #define MY_SCL 7
  #define USE_SECOND_BUS 1 /* Wire1 = I2C1 */
#elif defined(ARDUINO_ARCH_STM32)
  #define MY_SDA PB9 /* I2C1 remap บน Blue Pill */
  #define MY_SCL PB8
  #define USE_SECOND_BUS 0
#else
  /* AVR : ย้ายไม่ได้ ใช้ A4/A5 */
  #define USE_SECOND_BUS 0
#endif

MassmoreBME280 bme;

/* คืน reference ของบัสที่เปิดแล้ว - ไลบรารีจะใช้บัสนี้ต่อไป */
static TwoWire &busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  #if USE_SECOND_BUS
  /* core 3.x : Wire1 เป็น TwoWire(1) มีให้แล้ว ไม่ต้องสร้างเอง */
  Wire1.begin(MY_SDA, MY_SCL, 400000UL);
  return Wire1;
  #else
  Wire.begin(MY_SDA, MY_SCL, 400000UL);
  return Wire;
  #endif
#elif defined(ARDUINO_ARCH_RP2040)
  #if USE_SECOND_BUS
  Wire1.setSDA(MY_SDA);
  Wire1.setSCL(MY_SCL);
  Wire1.begin();
  Wire1.setClock(400000UL);
  return Wire1;
  #else
  Wire.setSDA(MY_SDA);
  Wire.setSCL(MY_SCL);
  Wire.begin();
  Wire.setClock(400000UL);
  return Wire;
  #endif
#elif defined(ARDUINO_ARCH_STM32)
  Wire.setSDA(MY_SDA);
  Wire.setSCL(MY_SCL);
  Wire.begin();
  Wire.setClock(400000UL);
  return Wire;
#else
  Wire.begin(); /* AVR : A4 / A5 */
  Wire.setClock(100000UL);
  return Wire;
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Custom Pins / Bus Remap"));
  Serial.println(F("=================================================="));

#if defined(MY_SDA)
  Serial.print(F("SDA = "));
  Serial.print(MY_SDA);
  Serial.print(F("  SCL = "));
  Serial.print(MY_SCL);
  Serial.println(USE_SECOND_BUS ? F("  (บัส Wire1)") : F("  (บัส Wire)"));
#else
  Serial.println(F("AVR : SDA = A4, SCL = A5 (ขาฮาร์ดแวร์ ย้ายไม่ได้)"));
#endif

  TwoWire &bus = busBegin();

  /* ส่งบัสให้ไลบรารีแบบ reference จะเป็น Wire หรือ Wire1 ก็ได้ */
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_DEFAULT, bus)) {
    Serial.print(F("ไม่พบเซ็นเซอร์บนบัสนี้ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    Serial.println(F("ตรวจว่าย้ายสายไปขาที่ตั้งไว้ในโค้ดแล้ว และ SDO ไม่ได้ต่อลง GND"));
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("พบ BME280 ที่ 0x"));
  Serial.println(bme.getAddress(), HEX);
  Serial.println();
}

void loop() {
  massmore_bme280_reading_t r;
  if (bme.read(r)) {
    Serial.print(r.temperature, 2);
    Serial.print(F(" C  "));
    Serial.print(r.humidity, 2);
    Serial.print(F(" %RH  "));
    Serial.print(r.pressure, 2);
    Serial.println(F(" hPa"));
  } else {
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
  }
  delay(2000);
}
