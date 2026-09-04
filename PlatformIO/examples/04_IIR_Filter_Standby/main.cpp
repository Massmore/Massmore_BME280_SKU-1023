/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  04_IIR_Filter_Standby - ฟิลเตอร์ IIR ในตัวชิป และเวลาพักระหว่างรอบวัด

  ฟิลเตอร์ IIR คืออะไร
    เป็นฟิลเตอร์ที่อยู่ในตัวชิปเอง ทำงานกับอุณหภูมิและความดันเท่านั้น
    (ความชื้นไม่ผ่านฟิลเตอร์ เพราะตัวมันเปลี่ยนช้าอยู่แล้ว)

    สูตรตามดาต้าชีตหัวข้อ 3.4.4
      y[n] = (y[n-1] * (K-1) + x[n]) / K       เมื่อ K คือค่าสัมประสิทธิ์

    ข้อดี  กดสัญญาณรบกวนระยะสั้นได้เก่งมากโดยไม่เปลืองเวลาวัดเพิ่มเลย
           เช่น ลมจากประตูที่เปิดปิด คนเดินผ่าน หรือแรงกดบนตัวบอร์ด
    ข้อเสีย ค่าตอบสนองช้าลง ต้องรอหลายรอบกว่าจะตามค่าจริงทัน
           และหลังเปลี่ยนค่าฟิลเตอร์ ค่าแรก ๆ จะยังไม่นิ่ง

  เวลาพัก (standby time)
    ใช้เฉพาะโหมด normal เป็นเวลาที่ชิปพักระหว่างรอบวัด
    พักนาน = กินไฟน้อย แต่ค่าเก่าลง

  ตัวอย่างนี้จะวัดจริงแล้วเทียบ noise ของฟิลเตอร์ทั้งห้าระดับให้ดู

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define SAMPLES 40

MassmoreBME280 bme;

static const massmore_bme280_filter_t FILTERS[5] = {
    MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_FILTER_2, MASSMORE_BME280_FILTER_4,
    MASSMORE_BME280_FILTER_8, MASSMORE_BME280_FILTER_16};

/*! วัดความดัน SAMPLES ครั้งแล้วคืนส่วนเบี่ยงเบนมาตรฐาน (หน่วย hPa) */
static float pressureNoise(float *meanOut) {
  float sum = 0.0f;
  float sumSquare = 0.0f;
  uint16_t good = 0;

  for (uint16_t i = 0; i < SAMPLES; i++) {
    massmore_bme280_reading_t r;
    if (bme.read(r) && !isnan(r.pressure)) {
      sum += r.pressure;
      sumSquare += r.pressure * r.pressure;
      good++;
    }
    delay(20);
  }

  if (good < 2) {
    *meanOut = NAN;
    return NAN;
  }
  float n = (float)good;
  float mean = sum / n;
  *meanOut = mean;
  return sqrtf(fabsf((sumSquare / n) - mean * mean));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - IIR Filter"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("ทดสอบฟิลเตอร์ทั้งห้าระดับ อย่าขยับบอร์ดระหว่างทดสอบ"));
  Serial.println();
}

void loop() {
  Serial.println(F("ฟิลเตอร์   ความดันเฉลี่ย     noise (sd)"));
  Serial.println(F("-------------------------------------------"));

  for (uint8_t i = 0; i < 5; i++) {
    /* ใช้ oversampling x1 ตลอด เพื่อให้เห็นผลของฟิลเตอร์ล้วน ๆ
       standby 0.5 ms เพื่อให้ฟิลเตอร์ได้ข้อมูลใหม่เร็วที่สุด */
    bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                    MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                    FILTERS[i], MASSMORE_BME280_STANDBY_0_5_MS);

    /* ฟิลเตอร์ต้องการเวลาเข้าที่ ทิ้งค่าแรก ๆ ไปก่อน
       จำนวนรอบที่ต้องทิ้งประมาณเท่ากับค่าสัมประสิทธิ์ของฟิลเตอร์ */
    for (uint8_t warmUp = 0; warmUp < 30; warmUp++) {
      massmore_bme280_reading_t discard;
      bme.read(discard);
      delay(20);
    }

    float mean = NAN;
    float sd = pressureNoise(&mean);

    Serial.print(F("  "));
    Serial.print(MassmoreBME280::filterToString(FILTERS[i]));
    Serial.print(F("        "));
    Serial.print(mean, 3);
    Serial.print(F(" hPa      "));
    Serial.print(sd, 4);
    Serial.println(F(" hPa"));
  }

  Serial.println();
  Serial.println(F("ลองทดสอบการตอบสนอง : เป่าลมใส่บอร์ดเบา ๆ ระหว่างที่วัดอยู่"));
  Serial.println(F("  ฟิลเตอร์ปิด  ค่าจะกระโดดทันทีแล้วกลับเร็ว"));
  Serial.println(F("  ฟิลเตอร์ 16  ค่าจะค่อย ๆ ขยับและค่อย ๆ กลับ"));
  Serial.println();
  Serial.println(F("เลือกยังไงดี"));
  Serial.println(F("  สถานีวัดอากาศกลางแจ้ง   ฟิลเตอร์ 16 (ตัดลมกระโชก)"));
  Serial.println(F("  วัดความสูงในอาคาร        ฟิลเตอร์ 16 + ความดัน x16"));
  Serial.println(F("  วัดอุณหภูมิห้องทั่วไป     ฟิลเตอร์ 2 หรือ 4 ก็พอ"));
  Serial.println(F("  วัดแบบ forced นาน ๆ ครั้ง ปิดฟิลเตอร์ (ไม่งั้นค่าแรกจะเพี้ยน)"));
  Serial.println();

  /* แสดงตารางเวลาพักให้ดูด้วย */
  Serial.println(F("เวลาพักระหว่างรอบในโหมด normal ที่เลือกได้"));
  for (uint8_t i = 0; i < 8; i++) {
    massmore_bme280_standby_t sb = (massmore_bme280_standby_t)i;
    Serial.print(F("  ค่า "));
    Serial.print(i);
    Serial.print(F(" = "));
    Serial.println(MassmoreBME280::standbyToString(sb));
  }
  Serial.println();

  delay(15000);
}
