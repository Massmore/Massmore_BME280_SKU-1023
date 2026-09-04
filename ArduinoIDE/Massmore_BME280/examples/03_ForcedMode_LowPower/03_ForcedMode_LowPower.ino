/*
  03_ForcedMode_LowPower - สถานีวัดอากาศประหยัดไฟด้วย forced mode + deep sleep

  แนวคิด
    งานวัดอากาศไม่จำเป็นต้องวัดทุกวินาที วัดนาทีละครั้งก็เกินพอ
    ดังนั้นให้ทั้งเซ็นเซอร์และ ESP32 หลับเป็นส่วนใหญ่

    เซ็นเซอร์  ใช้โหมด forced = วัดครั้งเดียวตามคำสั่งแล้วหลับเอง
               ระหว่างหลับกินไฟ 0.1 uA (ดาต้าชีตตาราง 1)
    ESP32      ใช้ deep sleep ระหว่างรอบ กินไฟราว 10 uA

    ถ้าใช้ normal mode ตลอดเวลา เซ็นเซอร์จะกินราว 340 uA ต่อเนื่อง
    ต่างกันเป็นพันเท่า มีผลกับอายุแบตเตอรี่มาก

  ค่าที่วัดได้จะถูกเก็บไว้ใน RTC memory ซึ่งรอดจาก deep sleep
  ทำให้เปรียบเทียบกับรอบก่อนหน้าได้แม้ซีพียูจะรีบูตใหม่ทุกครั้ง

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

/* วัดทุกกี่วินาที */
#define SLEEP_SECONDS 60

MassmoreBME280 bme;

/* ประกาศล่วงหน้า เพื่อให้ไฟล์นี้คอมไพล์ได้ทั้งใน Arduino IDE และ PlatformIO */
void runOnce();
void goSleep();

#if defined(ARDUINO_ARCH_ESP32)
/* ตัวแปรที่ประกาศด้วย RTC_DATA_ATTR จะอยู่ใน RTC memory
   ข้อมูลจึงไม่หายตอน deep sleep (แต่หายตอนกดปุ่ม reset หรือถอดไฟ) */
RTC_DATA_ATTR uint32_t g_bootCount = 0;
RTC_DATA_ATTR float g_lastTemperature = 0.0f;
RTC_DATA_ATTR float g_lastPressure = 0.0f;
RTC_DATA_ATTR bool g_hasPrevious = false;
#else
static uint32_t g_bootCount = 0;
static float g_lastTemperature = 0.0f;
static float g_lastPressure = 0.0f;
static bool g_hasPrevious = false;
#endif

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    ;
  }

  Wire.begin(PIN_SDA, PIN_SCL);
  runOnce();
  goSleep();
}

void runOnce() {
  g_bootCount++;

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.print(F("  Massmore BME280 - รอบวัดที่ "));
  Serial.println(g_bootCount);
  Serial.println(F("=================================================="));

  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  /* ชุดตั้งค่าที่ Bosch แนะนำสำหรับสถานีวัดอากาศ (ดาต้าชีตตาราง 7)
       โหมด forced, oversampling x1 ทุกช่อง, ปิดฟิลเตอร์
     ปิดฟิลเตอร์เพราะฟิลเตอร์ IIR ต้องการการวัดต่อเนื่องหลายรอบถึงจะเข้าที่
     ถ้าวัดนาทีละครั้งแล้วเปิดฟิลเตอร์ไว้ ค่าแรกจะเพี้ยนเสมอ */
  bme.useWeatherStationPreset();

  /* สั่งวัดหนึ่งครั้งแล้วรอ ใช้เวลาประมาณ 10 ms เท่านั้น */
  massmore_bme280_reading_t reading;
  if (!bme.read(reading)) {
    Serial.print(F("อ่านค่าไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  Serial.print(F("อุณหภูมิ  "));
  Serial.print(reading.temperature, 2);
  Serial.println(F(" C"));
  Serial.print(F("ความชื้น  "));
  Serial.print(reading.humidity, 2);
  Serial.println(F(" %RH"));
  Serial.print(F("ความดัน   "));
  Serial.print(reading.pressure, 2);
  Serial.println(F(" hPa"));
  Serial.print(F("จุดน้ำค้าง "));
  Serial.print(MassmoreBME280::dewPoint(reading.temperature, reading.humidity), 2);
  Serial.println(F(" C"));

  if (g_hasPrevious) {
    float deltaT = reading.temperature - g_lastTemperature;
    float deltaP = reading.pressure - g_lastPressure;

    Serial.print(F("เทียบรอบก่อน : อุณหภูมิ "));
    Serial.print(deltaT >= 0 ? F("+") : F(""));
    Serial.print(deltaT, 2);
    Serial.print(F(" C   ความดัน "));
    Serial.print(deltaP >= 0 ? F("+") : F(""));
    Serial.print(deltaP, 2);
    Serial.println(F(" hPa"));

    /* ความดันที่ตกเร็วกว่า 1 hPa ต่อชั่วโมงเป็นสัญญาณว่าพายุกำลังมา
       ที่นี่วัดนาทีละครั้ง จึงคิดเทียบเป็นต่อชั่วโมง */
    float trendPerHour = deltaP * (3600.0f / (float)SLEEP_SECONDS);
    if (trendPerHour < -1.0f) {
      Serial.println(F(">>> ความดันกำลังตกเร็ว อากาศอาจแปรปรวน <<<"));
    } else if (trendPerHour > 1.0f) {
      Serial.println(F(">>> ความดันกำลังขึ้นเร็ว อากาศกำลังจะดีขึ้น <<<"));
    }
  }

  g_lastTemperature = reading.temperature;
  g_lastPressure = reading.pressure;
  g_hasPrevious = true;
}

void goSleep() {
  Serial.print(F("หลับต่อ "));
  Serial.print(SLEEP_SECONDS);
  Serial.println(F(" วินาที"));
  Serial.flush();

#if defined(ARDUINO_ARCH_ESP32)
  /* เซ็นเซอร์อยู่ในโหมด sleep เองอยู่แล้วหลังวัดเสร็จในโหมด forced
     จึงสั่ง ESP32 หลับได้เลย */
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
  /* โค้ดหลังบรรทัดนี้จะไม่ถูกรัน ESP32 จะบูตใหม่ที่ setup() */
#else
  /* บอร์ดที่ไม่มี deep sleep ก็ใช้ delay ธรรมดาแทน แล้ววนใน loop() */
  delay((uint32_t)SLEEP_SECONDS * 1000UL);
#endif
}

void loop() {
  /* บน ESP32 บรรทัดนี้ไม่ถูกเรียกเลย เพราะ setup() จบด้วยการเข้า deep sleep
     ซึ่งทำให้ชิปบูตใหม่ที่ setup() ทุกครั้ง */
  runOnce();
  goSleep();
}
