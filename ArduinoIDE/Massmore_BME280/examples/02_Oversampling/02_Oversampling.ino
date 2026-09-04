/*
  02_Oversampling - เปรียบเทียบ oversampling ทั้งห้าระดับด้วยตัวเลขจริง

  oversampling คือการให้ชิปวัดซ้ำหลายครั้งภายในรอบเดียวแล้วเฉลี่ย
  ยิ่งวัดซ้ำมาก สัญญาณรบกวนยิ่งต่ำ แต่ใช้เวลาและพลังงานมากขึ้นเป็นเชิงเส้น

  ตัวอย่างนี้จะวัดจริง 32 ครั้งในแต่ละระดับ แล้วรายงาน
    - ค่าเฉลี่ย
    - ส่วนเบี่ยงเบนมาตรฐาน (ตัวเลขนี้แหละคือ noise)
    - เวลาที่ใช้จริงต่อหนึ่งรอบวัด

  เอาไว้ตัดสินใจว่างานของคุณควรใช้ระดับไหน ไม่ต้องเดา

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define SAMPLES_PER_LEVEL 32

MassmoreBME280 bme;

/* ระดับที่จะทดสอบ เรียงจากเบาไปหนัก */
static const massmore_bme280_sampling_t LEVELS[5] = {
    MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X2,
    MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_X8,
    MASSMORE_BME280_SAMPLING_X16};

/*!
 * วัดซ้ำ n ครั้งแล้วคำนวณค่าเฉลี่ยกับส่วนเบี่ยงเบนมาตรฐานของทั้งสามค่า
 */
static void measureNoise(uint8_t samples, float *meanT, float *sdT, float *meanP,
                         float *sdP, float *meanH, float *sdH, uint32_t *elapsedUs) {
  float sumT = 0, sumP = 0, sumH = 0;
  float sumT2 = 0, sumP2 = 0, sumH2 = 0;
  uint8_t good = 0;

  uint32_t start = micros();
  for (uint8_t i = 0; i < samples; i++) {
    massmore_bme280_reading_t r;
    if (!bme.read(r)) {
      continue;
    }
    sumT += r.temperature;
    sumT2 += r.temperature * r.temperature;
    sumP += r.pressure;
    sumP2 += r.pressure * r.pressure;
    sumH += r.humidity;
    sumH2 += r.humidity * r.humidity;
    good++;
  }
  *elapsedUs = (micros() - start) / (good > 0 ? good : 1);

  if (good < 2) {
    *meanT = *sdT = *meanP = *sdP = *meanH = *sdH = NAN;
    return;
  }

  float n = (float)good;
  *meanT = sumT / n;
  *meanP = sumP / n;
  *meanH = sumH / n;
  /* ส่วนเบี่ยงเบนมาตรฐานแบบประชากร : sqrt(E[x^2] - E[x]^2) */
  *sdT = sqrtf(fabsf((sumT2 / n) - (*meanT) * (*meanT)));
  *sdP = sqrtf(fabsf((sumP2 / n) - (*meanP) * (*meanP)));
  *sdH = sqrtf(fabsf((sumH2 / n) - (*meanH) * (*meanH)));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Oversampling"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("วัดระดับละ "));
  Serial.print(SAMPLES_PER_LEVEL);
  Serial.println(F(" ครั้ง กรุณาอย่าเป่าลมหรือขยับบอร์ดระหว่างทดสอบ"));
  Serial.println();
}

void loop() {
  Serial.println(F("ระดับ  เวลา/รอบ   อุณหภูมิ (sd)      ความดัน (sd)         ความชื้น (sd)"));
  Serial.println(F("---------------------------------------------------------------------------"));

  for (uint8_t i = 0; i < 5; i++) {
    /* ปิดฟิลเตอร์ IIR ระหว่างทดสอบ เพื่อให้เห็น noise ดิบ ๆ ของ oversampling จริง ๆ
       ถ้าเปิดฟิลเตอร์ไว้ ตัวเลข sd จะถูกกดลงจนเทียบกันไม่ออก */
    bme.setSampling(MASSMORE_BME280_MODE_FORCED, LEVELS[i], LEVELS[i], LEVELS[i],
                    MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);

    float meanT, sdT, meanP, sdP, meanH, sdH;
    uint32_t elapsedUs;
    measureNoise(SAMPLES_PER_LEVEL, &meanT, &sdT, &meanP, &sdP, &meanH, &sdH, &elapsedUs);

    Serial.print(F(" "));
    Serial.print(MassmoreBME280::samplingToString(LEVELS[i]));
    Serial.print(F("    "));
    Serial.print((float)elapsedUs / 1000.0f, 1);
    Serial.print(F(" ms    "));

    Serial.print(meanT, 2);
    Serial.print(F(" ("));
    Serial.print(sdT, 3);
    Serial.print(F(")   "));

    Serial.print(meanP, 2);
    Serial.print(F(" ("));
    Serial.print(sdP, 3);
    Serial.print(F(")   "));

    Serial.print(meanH, 2);
    Serial.print(F(" ("));
    Serial.print(sdH, 3);
    Serial.println(F(")"));
  }

  Serial.println();
  Serial.println(F("อ่านตารางนี้ยังไง"));
  Serial.println(F("  ตัวเลขในวงเล็บคือ noise ยิ่งน้อยยิ่งนิ่ง"));
  Serial.println(F("  ปกติ x1 -> x4 จะเห็นผลชัดที่สุด หลังจากนั้นเริ่มคุ้มน้อยลง"));
  Serial.println(F("  ถ้าใช้แบตเตอรี่ ให้เลือกจุดที่ noise พอรับได้ด้วยเวลาที่น้อยที่สุด"));
  Serial.println(F("  ถ้าอยากได้นิ่งกว่านี้อีกโดยไม่เพิ่มเวลา ให้เปิดฟิลเตอร์ IIR แทน"));
  Serial.println(F("  (ดูตัวอย่าง 04_IIR_Filter_Standby)"));
  Serial.println();

  delay(10000);
}
