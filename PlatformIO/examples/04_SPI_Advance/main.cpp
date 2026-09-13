/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้ว pio run -t upload
*/

#include <Arduino.h>

/*
  04_SPI_Advance - ใช้บอร์ด Massmore BME280 ผ่านบัส SPI 4 สาย

  เมื่อไรควรใช้ SPI แทน I2C
    - ต้องการความเร็วสูง (SPI ได้ถึง 10 MHz, I2C ปกติ 400 kHz)
    - สายยาวหรือมีสัญญาณรบกวน (SPI เป็น push-pull ทนกว่า open-drain)
    - มี BME280 หลายตัวเกินสอง address ของ I2C (แยกกันด้วยขา CS)

  การต่อสาย SPI (บอร์ด SKU-1023)
    VIN -> 3V3 / 5V     SCK -> SCK
    GND -> GND          SDI -> MOSI (ข้อมูลเข้าเซ็นเซอร์)
    CS  -> ขา CS ที่เลือก SDO -> MISO (ข้อมูลออกจากเซ็นเซอร์)

  ไลบรารีไม่เรียก SPI.begin() เอง sketch เป็นคนเปิดบัสและเลือกขา (ดู busBegin())
    bme.beginSPI(csPin, SPI, ความถี่Hz);

  ขาปริยายของแต่ละบอร์ด (แก้ในบล็อกด้านล่างได้)
    ESP32          SCK 18  MISO 19  MOSI 23  CS 5   (VSPI)
    ESP32-S3       SCK 12  MISO 13  MOSI 11  CS 10  (FSPI - ย้ายได้ทุกขา)
    Pico (RP2040)  SCK 18  MISO 16  MOSI 19  CS 17  (SPI0)
    Nano / Uno     SCK 13  MISO 12  MOSI 11  CS 10  (ขาฮาร์ดแวร์ตายตัว)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <SPI.h>

/* -------------------------------------------------------------------------
   ตั้งค่าขา
   ------------------------------------------------------------------------- */
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define PIN_SCK 12
  #define PIN_MISO 13
  #define PIN_MOSI 11
  #define PIN_CS 10
#elif defined(ARDUINO_ARCH_ESP32)
  #define PIN_SCK 18
  #define PIN_MISO 19
  #define PIN_MOSI 23
  #define PIN_CS 5
#elif defined(ARDUINO_ARCH_RP2040)
  #define PIN_SCK 18
  #define PIN_MISO 16
  #define PIN_MOSI 19
  #define PIN_CS 17
#else
  #define PIN_CS 10 /* AVR : SCK 13 / MISO 12 / MOSI 11 ตายตัว */
#endif

#define SPI_HZ 1000000UL /* เริ่มที่ 1 MHz ถ้าสายสั้นและนิ่งเพิ่มได้ถึง 10 MHz */

MassmoreBME280 bme;

static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  /* core 3.x : SPI.begin(sck, miso, mosi, ss) - ss ใส่ -1 ได้ เพราะไลบรารีคุม CS เอง */
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);
#elif defined(ARDUINO_ARCH_RP2040)
  SPI.setSCK(PIN_SCK);
  SPI.setRX(PIN_MISO);
  SPI.setTX(PIN_MOSI);
  SPI.begin();
#else
  SPI.begin();
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - SPI 4-wire"));
  Serial.println(F("=================================================="));

  busBegin();

  if (!bme.beginSPI(PIN_CS, SPI, SPI_HZ)) {
    Serial.print(F("ไม่พบเซ็นเซอร์บน SPI : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    Serial.println(F("ตรวจ SDI->MOSI, SDO->MISO, SCK, CS และต้องไม่เสียบสาย Qwiic พร้อมกัน"));
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("พบ BME280 ผ่าน SPI  CS = "));
  Serial.print(bme.getCSPin());
  Serial.print(F("  chip id 0x"));
  Serial.println(bme.getChipID(), HEX);

  /* งานความเร็วสูง : indoor navigation preset (x16 ความดัน, ฟิลเตอร์ 16, standby 0.5 ms)
     อัตราอ่านสูงสุดของชิปราว 25 Hz ที่ค่านี้ - SPI รับไหวสบาย */
  bme.useIndoorNavigationPreset();

  /* ทดสอบว่าที่ความถี่นี้อ่านค่าชดเชยได้เสถียร : อ่าน 3 ครั้ง dig_T1 ต้องเท่ากันหมด */
  massmore_bme280_calib_t c1, c2;
  bme.getCalibration(c1);
  bme.readCalibration();
  bme.getCalibration(c2);
  Serial.print(F("ตรวจความเสถียรของบัส : dig_T1 = "));
  Serial.print(c1.dig_T1);
  Serial.println(c1.dig_T1 == c2.dig_T1 ? F("  (อ่านซ้ำตรงกัน OK)")
                                          : F("  (อ่านซ้ำไม่ตรง ลดความถี่ SPI ลง)"));
  Serial.println();
}

void loop() {
  /* วัดอัตราอ่านจริง : อ่าน 100 ครั้งแล้วจับเวลา */
  massmore_bme280_reading_t r;
  uint32_t t0 = millis();
  uint16_t ok = 0;
  for (uint16_t i = 0; i < 100; i++) {
    if (bme.read(r)) {
      ok++;
    }
  }
  uint32_t elapsed = millis() - t0;

  Serial.print(r.temperature, 2);
  Serial.print(F(" C  "));
  Serial.print(r.humidity, 2);
  Serial.print(F(" %RH  "));
  Serial.print(r.pressure, 2);
  Serial.print(F(" hPa   | อ่าน 100 ครั้งสำเร็จ "));
  Serial.print(ok);
  Serial.print(F(" ใช้ "));
  Serial.print(elapsed);
  Serial.println(F(" ms"));

  delay(1000);
}
