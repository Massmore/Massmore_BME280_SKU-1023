/*
  01_BasicReading - อ่านค่าพื้นฐานครบทั้งสี่ค่าจากบอร์ด Massmore BME280 (SKU-1023)

  ตัวอย่างนี้แสดง
    - อุณหภูมิ (องศาเซลเซียส)
    - ความชื้นสัมพัทธ์ (%RH)
    - ความดันบรรยากาศ (hPa)
    - ความสูงโดยประมาณ (เมตร)

  การต่อสาย (บอร์ด Massmore ESP32 Breakout หรือ ESP32 DevKit ทั่วไป)
    VIN  -> 3V3 หรือ 5V   (บอร์ดมีเรกูเลเตอร์ในตัว รับได้ 3-5V)
    GND  -> GND
    SDA  -> GPIO 21
    SCL  -> GPIO 22
  หรือเสียบสาย Qwiic เส้นเดียวจบ ไม่ต้องต่อสายเปล่า

  address ปริยายของบอร์ดคือ 0x76  (ถ้าบัดกรีจัมเปอร์ ADDR ด้านหลังจะเป็น 0x77)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

/* ความดันที่ระดับน้ำทะเลของพื้นที่ ณ วันนั้น หน่วย hPa
   ค่ามาตรฐานสากลคือ 1013.25 แต่ของจริงเปลี่ยนทุกวัน
   ดูค่าจริงของจังหวัดคุณได้จากเว็บกรมอุตุนิยมวิทยา แล้วแก้ตัวเลขนี้
   ถ้าอยากได้ความสูงที่แม่นยำ (ดูตัวอย่าง 05_Altitude_SeaLevel ประกอบ) */
#define SEA_LEVEL_HPA 1013.25f

MassmoreBME280 bme;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ; /* รอให้ Serial Monitor พร้อม แต่ไม่รอเกิน 3 วินาที */
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Basic Reading"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);

  /* begin() จะตรวจ chip id ให้ด้วย ถ้าเป็น BMP280 หรือของเลียนแบบจะไม่ผ่าน */
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    Serial.println(F("สิ่งที่ควรตรวจ"));
    Serial.println(F("  1. สาย Qwiic เสียบแน่นทั้งสองหัวหรือยัง"));
    Serial.println(F("  2. ถ้าต่อสายเปล่า SDA เข้า GPIO21 และ SCL เข้า GPIO22 ถูกด้านหรือไม่"));
    Serial.println(F("  3. ถ้าบัดกรีจัมเปอร์ ADDR ไว้ ต้องเปลี่ยนเป็น 0x77"));
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

  /* ตั้งค่าแบบสมดุล เหมาะกับงานทั่วไป
     วัดต่อเนื่อง (normal) หนึ่งครั้งทุก 500 ms, oversampling x2 ทุกช่อง,
     เปิดฟิลเตอร์ระดับ 4 เพื่อกดสัญญาณรบกวนจากลมและการสั่นสะเทือน */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X2,
                  MASSMORE_BME280_SAMPLING_X2, MASSMORE_BME280_SAMPLING_X2,
                  MASSMORE_BME280_FILTER_4, MASSMORE_BME280_STANDBY_500_MS);

  bme.setSeaLevelPressure(SEA_LEVEL_HPA);

  Serial.print(F("หนึ่งรอบวัดใช้เวลาราว "));
  Serial.print(bme.measurementTimeMs());
  Serial.println(F(" ms"));
  Serial.println();
  Serial.println(F("อุณหภูมิ    ความชื้น    ความดัน      ความสูง     จุดน้ำค้าง"));
  Serial.println(F("--------------------------------------------------------------"));
}

void loop() {
  massmore_bme280_reading_t reading;

  /* read() อ่านรีจิสเตอร์ 0xF7-0xFE รวดเดียว จึงได้ทั้งสามค่าจากรอบวัดเดียวกัน
     ถ้าเรียก readTemperature() readPressure() readHumidity() แยกกันสามครั้ง
     จะเสียเวลาบัสสามเท่าและได้ค่าจากคนละรอบวัด */
  if (!bme.read(reading)) {
    Serial.print(F("อ่านค่าไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    delay(2000);
    return;
  }

  Serial.print(reading.temperature, 2);
  Serial.print(F(" C     "));

  Serial.print(reading.humidity, 2);
  Serial.print(F(" %     "));

  Serial.print(reading.pressure, 2);
  Serial.print(F(" hPa   "));

  Serial.print(reading.altitude, 1);
  Serial.print(F(" m     "));

  /* จุดน้ำค้างคำนวณจากอุณหภูมิและความชื้น บอกได้ว่าอากาศจะเริ่มควบแน่นที่กี่องศา
     ใช้ประเมินความอึดอัดและความเสี่ยงเชื้อรา */
  Serial.print(MassmoreBME280::dewPoint(reading.temperature, reading.humidity), 2);
  Serial.println(F(" C"));

  delay(2000);
}
