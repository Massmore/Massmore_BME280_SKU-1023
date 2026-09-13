/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้ว pio run -t upload
*/

#include <Arduino.h>

/*
  01_BasicRead - อ่านค่าพื้นฐานครบทั้งสี่ค่าจากบอร์ด Massmore BME280 (SKU-1023)

  ตัวอย่างนี้แสดง
    - อุณหภูมิ (องศาเซลเซียส)
    - ความชื้นสัมพัทธ์ (%RH)
    - ความดันบรรยากาศ (hPa)
    - ความสูงโดยประมาณ (เมตร)

  ใช้ได้กับ ESP32 / ESP32-S3 / RP2040 / STM32 / AVR (Arduino Nano, Uno) โดยไม่ต้องแก้โค้ด
  ไลบรารีไม่เรียก Wire.begin() เอง sketch เป็นคนเปิดบัสและเลือกขา (ดู busBegin())

  การต่อสาย I2C (บอร์ด SKU-1023)
    VIN  -> 3V3 หรือ 5V   (บอร์ดมีเรกูเลเตอร์ในตัว รับได้ 3-5V)
    GND  -> GND
    SCK  -> SCL   (ESP32: GPIO22, ESP32-S3: GPIO9, Nano/Uno: A5, Pico: GP5)
    SDI  -> SDA   (ESP32: GPIO21, ESP32-S3: GPIO8, Nano/Uno: A4, Pico: GP4)
  หรือเสียบสาย Qwiic เส้นเดียวจบ

  address ปริยายของบอร์ดคือ 0x77 (ถ้าต่อ SDO ลง GND จะเป็น 0x76 -> ใช้ beginAuto())

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

/* ความดันที่ระดับน้ำทะเลของพื้นที่ ณ วันนั้น หน่วย hPa (ค่ามาตรฐาน 1013.25)
   ดูค่าจริงได้จากเว็บกรมอุตุนิยมวิทยา แล้วแก้ตัวเลขนี้ถ้าอยากได้ความสูงที่แม่น */
#define SEA_LEVEL_HPA 1013.25f

MassmoreBME280 bme;

/* เปิดบัส I2C ตามชนิดบอร์ด - ที่เดียวที่มีหมายเลขขา */
static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  /* Arduino-ESP32 core 3.x : SDA / SCL เป็นขาปริยายของ variant นั้น ๆ
     (esp32dev = 21/22, esp32-s3-devkitc-1 = 8/9) เปลี่ยนเป็นขาอื่นได้ตามใจ */
  Wire.begin(SDA, SCL);
#elif defined(ARDUINO_ARCH_RP2040)
  /* Arduino-Pico : ถ้าไม่ setSDA/setSCL จะใช้ GP4/GP5 */
  Wire.begin();
#else
  /* AVR / STM32 : ขาฮาร์ดแวร์ตายตัว (Nano/Uno = A4/A5) */
  Wire.begin();
#endif
  Wire.setClock(100000UL);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ; /* รอให้ Serial Monitor พร้อม แต่ไม่รอเกิน 3 วินาที */
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Basic Read"));
  Serial.println(F("=================================================="));

  busBegin();

  /* begin() ปริยาย = 0x77 บนบัส Wire และตรวจ chip id ให้ด้วย
     ถ้าไม่แน่ใจว่า SDO ต่อไว้อย่างไร ใช้ bme.beginAuto() แทนได้ */
  if (!bme.begin()) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    Serial.println(F("สิ่งที่ควรตรวจ"));
    Serial.println(F("  1. สาย Qwiic เสียบแน่นทั้งสองหัวหรือยัง"));
    Serial.println(F("  2. SDI -> SDA และ SCK -> SCL ถูกด้านหรือไม่"));
    Serial.println(F("  3. ถ้าต่อ SDO ลง GND address จะเป็น 0x76 ให้ใช้ beginAuto()"));
    Serial.println(F("  4. บอร์ดที่ติ๊ก BMP280 จะวัดความชื้นไม่ได้ ใช้ไลบรารีนี้ไม่ได้"));
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("พบเซ็นเซอร์ที่ 0x"));
  Serial.print(bme.getAddress(), HEX);
  Serial.print(F("  chip id 0x"));
  Serial.print(bme.getChipID(), HEX);
  Serial.print(F("  ("));
  Serial.print(MassmoreBME280::chipToString(bme.getChipType()));
  Serial.println(F(")"));

  /* ตั้งค่าแบบสมดุล : normal mode ทุก 500 ms, oversampling x2, ฟิลเตอร์ 4 */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X2,
                  MASSMORE_BME280_SAMPLING_X2, MASSMORE_BME280_SAMPLING_X2,
                  MASSMORE_BME280_FILTER_4, MASSMORE_BME280_STANDBY_500_MS);
  bme.setSeaLevelPressure(SEA_LEVEL_HPA);

  Serial.println();
  Serial.println(F("Temp(C)   Hum(%RH)  Pres(hPa)  Alt(m)   DewPt(C)"));
  Serial.println(F("---------------------------------------------------"));
}

void loop() {
  massmore_bme280_reading_t reading;

  /* read() อ่านรีจิสเตอร์ 0xF7-0xFE รวดเดียว ได้ทั้งสามค่าจากรอบวัดเดียวกัน */
  if (!bme.read(reading)) {
    Serial.print(F("อ่านค่าไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    delay(2000);
    return;
  }

  Serial.print(reading.temperature, 2);
  Serial.print(F("     "));
  Serial.print(reading.humidity, 2);
  Serial.print(F("     "));
  Serial.print(reading.pressure, 2);
  Serial.print(F("    "));
  Serial.print(reading.altitude, 1);
  Serial.print(F("     "));
  Serial.println(MassmoreBME280::dewPoint(reading.temperature, reading.humidity), 2);

  delay(2000);
}
