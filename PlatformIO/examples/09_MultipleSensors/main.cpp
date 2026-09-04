/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  09_MultipleSensors - ใช้เซ็นเซอร์สองตัวบนบัสเดียวกัน (0x76 และ 0x77)

  BME280 มี address ให้เลือกสองค่า ขึ้นกับขา SDO
    SDO ลง GND    -> 0x76   (ค่าปริยายของบอร์ด Massmore)
    SDO ขึ้น VDDIO -> 0x77   (บัดกรีจัมเปอร์ ADDR ด้านหลังบอร์ด)

  จึงต่อสองตัวบนสาย Qwiic เส้นเดียวกันได้เลย ไม่ต้องใช้ตัวสลับบัส
  บอร์ด Massmore มีหัว Qwiic สองหัว ต่อพ่วงกันได้ทันที

  งานที่ใช้จริง
    - เทียบในบ้านกับนอกบ้าน
    - เทียบก่อนกับหลังผ่านตัวกรองอากาศ
    - วัดผลต่างความสูงระหว่างสองจุด (ตัดผลของสภาพอากาศออกได้หมด
      เพราะความดันที่ระดับน้ำทะเลกระทบทั้งสองตัวเท่ากัน)
    - ใช้ตัวหนึ่งเป็นตัวอ้างอิงเช็คว่าอีกตัวยังเที่ยงอยู่ไหม

  ถ้ามีตัวเดียวก็รันได้ ระบบจะข้ามตัวที่ไม่เจอไปเอง

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic ต่อพ่วง)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreBME280 sensorA; /* ที่ 0x76 */
MassmoreBME280 sensorB; /* ที่ 0x77 */

static bool g_hasA = false;
static bool g_hasB = false;

void printOne(const char *label, MassmoreBME280 &sensor,
              massmore_bme280_reading_t &reading) {
  Serial.print(F("  "));
  Serial.print(label);
  Serial.print(F(" (0x"));
  Serial.print(sensor.getAddress(), HEX);
  Serial.print(F(")  "));
  Serial.print(reading.temperature, 2);
  Serial.print(F(" C   "));
  Serial.print(reading.humidity, 2);
  Serial.print(F(" %RH   "));
  Serial.print(reading.pressure, 2);
  Serial.print(F(" hPa   น้ำค้าง "));
  Serial.print(MassmoreBME280::dewPoint(reading.temperature, reading.humidity), 2);
  Serial.println(F(" C"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - สองตัวบนบัสเดียว"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);

  g_hasA = sensorA.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire);
  Serial.print(F("เซ็นเซอร์ A ที่ 0x76 : "));
  Serial.println(g_hasA ? F("พบแล้ว") : F("ไม่พบ"));

  g_hasB = sensorB.begin(MASSMORE_BME280_I2C_ADDR_B, &Wire);
  Serial.print(F("เซ็นเซอร์ B ที่ 0x77 : "));
  Serial.println(g_hasB ? F("พบแล้ว") : F("ไม่พบ (บัดกรีจัมเปอร์ ADDR เพื่อย้ายมาที่นี่)"));

  if (!g_hasA && !g_hasB) {
    Serial.println(F("\nไม่พบเซ็นเซอร์เลยสักตัว ตรวจสาย Qwiic แล้วรีเซ็ตใหม่"));
    while (true) {
      delay(1000);
    }
  }

  /* ตั้งค่าให้เหมือนกันทั้งสองตัว ไม่งั้นเทียบกันไม่ได้
     ใช้ oversampling x4 กับฟิลเตอร์ 4 เพื่อให้ค่านิ่งพอจะเห็นผลต่างจริง */
  if (g_hasA) {
    sensorA.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X4,
                        MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_X4,
                        MASSMORE_BME280_FILTER_4, MASSMORE_BME280_STANDBY_250_MS);
  }
  if (g_hasB) {
    sensorB.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X4,
                        MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_X4,
                        MASSMORE_BME280_FILTER_4, MASSMORE_BME280_STANDBY_250_MS);
  }

  /* ลายนิ้วมือของแต่ละตัว ใช้ยืนยันว่าเป็นชิปคนละตัวจริง ไม่ใช่ตัวเดิมที่ตอบสอง address */
  if (g_hasA && g_hasB) {
    massmore_bme280_calib_t ca, cb;
    sensorA.getCalibration(ca);
    sensorB.getCalibration(cb);
    Serial.println();
    Serial.println(F("ลายนิ้วมือค่าชดเชย (ต้องไม่เหมือนกัน)"));
    Serial.print(F("  A : dig_T1="));
    Serial.print(ca.dig_T1);
    Serial.print(F("  dig_P1="));
    Serial.print(ca.dig_P1);
    Serial.print(F("  dig_H1="));
    Serial.println(ca.dig_H1);
    Serial.print(F("  B : dig_T1="));
    Serial.print(cb.dig_T1);
    Serial.print(F("  dig_P1="));
    Serial.print(cb.dig_P1);
    Serial.print(F("  dig_H1="));
    Serial.println(cb.dig_H1);
    if (ca.dig_T1 == cb.dig_T1 && ca.dig_P1 == cb.dig_P1) {
      Serial.println(F("  !! ค่าชดเชยเหมือนกันเป๊ะ ผิดปกติมาก ตรวจสอบบอร์ดด้วย"));
    } else {
      Serial.println(F("  ค่าต่างกัน ยืนยันว่าเป็นชิปคนละตัวจริง"));
    }
  }

  Serial.println();
  delay(500);
}

void loop() {
  massmore_bme280_reading_t readingA, readingB;
  bool okA = g_hasA && sensorA.read(readingA);
  bool okB = g_hasB && sensorB.read(readingB);

  Serial.println(F("--------------------------------------------------"));

  if (okA) {
    printOne("A", sensorA, readingA);
  } else if (g_hasA) {
    Serial.println(F("  A อ่านไม่สำเร็จ"));
  }

  if (okB) {
    printOne("B", sensorB, readingB);
  } else if (g_hasB) {
    Serial.println(F("  B อ่านไม่สำเร็จ"));
  }

  if (okA && okB) {
    float deltaT = readingB.temperature - readingA.temperature;
    float deltaH = readingB.humidity - readingA.humidity;
    float deltaP = readingB.pressure - readingA.pressure;

    Serial.print(F("  ผลต่าง B-A   "));
    Serial.print(deltaT >= 0 ? F("+") : F(""));
    Serial.print(deltaT, 2);
    Serial.print(F(" C   "));
    Serial.print(deltaH >= 0 ? F("+") : F(""));
    Serial.print(deltaH, 2);
    Serial.print(F(" %RH   "));
    Serial.print(deltaP >= 0 ? F("+") : F(""));
    Serial.print(deltaP, 3);
    Serial.println(F(" hPa"));

    /* ความดันต่างกัน 1 hPa เท่ากับสูงต่างกันประมาณ 8.4 เมตรที่ระดับน้ำทะเล
       วิธีนี้แม่นกว่าการเทียบความสูงสัมบูรณ์ของแต่ละตัว
       เพราะสภาพอากาศกระทบทั้งสองตัวเท่ากันแล้วหักล้างกันไป */
    float heightDifference = -deltaP * 8.4f;
    Serial.print(F("  ต่างระดับกันราว "));
    Serial.print(heightDifference, 2);
    Serial.println(F(" เมตร (B สูงกว่า A ถ้าเป็นบวก)"));

    /* ถ้าวางสองตัวไว้ที่เดียวกัน ผลต่างควรอยู่ในสเปกความคลาดเคลื่อนของชิป
       อุณหภูมิ +/-1 C, ความชื้น +/-3 %RH, ความดัน +/-1 hPa (ดาต้าชีตตาราง 1-3) */
    if (fabsf(deltaT) > 1.0f) {
      Serial.println(F("  * อุณหภูมิต่างเกิน 1 C ถ้าวางที่เดียวกันควรตรวจสอบ"));
    }
    if (fabsf(deltaH) > 3.0f) {
      Serial.println(F("  * ความชื้นต่างเกิน 3 %RH ถ้าวางที่เดียวกันควรตรวจสอบ"));
    }
  }

  delay(3000);
}
