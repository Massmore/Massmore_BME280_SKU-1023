/*
  05_LowPower_ForcedMode - สถานีวัดอากาศประหยัดไฟด้วย forced mode (+ deep sleep บน ESP32)

  แนวคิด
    วัดนาทีละครั้งก็เกินพอ ให้ทั้งเซ็นเซอร์และ MCU หลับเป็นส่วนใหญ่
      เซ็นเซอร์  forced mode = วัดครั้งเดียวแล้วหลับเอง กินไฟ 0.1 uA (ดาต้าชีตตาราง 1)
      ESP32      deep sleep ระหว่างรอบ ~10 uA  (บอร์ดอื่นใช้ delay แทน)
    ถ้าใช้ normal mode ตลอดเวลา เซ็นเซอร์กินราว 340 uA ต่างกันเป็นพันเท่า

  บน ESP32 ค่ารอบก่อนเก็บใน RTC memory ซึ่งรอดจาก deep sleep
  จึงเทียบแนวโน้มความดันได้ (ตกเร็ว = พายุกำลังมา)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define SLEEP_SECONDS 60

MassmoreBME280 bme;

void runOnce();
void goSleep();

#if defined(ARDUINO_ARCH_ESP32)
RTC_DATA_ATTR uint32_t g_bootCount = 0;
RTC_DATA_ATTR float g_lastPressure = 0.0f;
RTC_DATA_ATTR bool g_hasPrevious = false;
#else
static uint32_t g_bootCount = 0;
static float g_lastPressure = 0.0f;
static bool g_hasPrevious = false;
#endif

static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA, SCL);
#else
  Wire.begin();
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    ;
  }
  busBegin();
  runOnce();
  goSleep();
}

void runOnce() {
  g_bootCount++;
  Serial.println();
  Serial.print(F("=== Massmore BME280 - รอบวัดที่ "));
  Serial.print(g_bootCount);
  Serial.println(F(" ==="));

  if (!bme.begin()) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  /* ชุดที่ Bosch แนะนำสำหรับสถานีวัดอากาศ : forced, x1 ทุกช่อง, ปิดฟิลเตอร์
     (ฟิลเตอร์ IIR ต้องการวัดต่อเนื่องหลายรอบถึงจะเข้าที่ ไม่เหมาะกับวัดนาน ๆ ครั้ง) */
  bme.useWeatherStationPreset();

  massmore_bme280_reading_t r;
  if (!bme.read(r)) { /* ในโหมด forced read() จะสั่งวัดหนึ่งครั้งให้เอง (~10 ms) */
    Serial.print(F("อ่านค่าไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  Serial.print(F("อุณหภูมิ  "));
  Serial.print(r.temperature, 2);
  Serial.println(F(" C"));
  Serial.print(F("ความชื้น  "));
  Serial.print(r.humidity, 2);
  Serial.println(F(" %RH"));
  Serial.print(F("ความดัน   "));
  Serial.print(r.pressure, 2);
  Serial.println(F(" hPa"));

  if (g_hasPrevious) {
    float trendPerHour = (r.pressure - g_lastPressure) * (3600.0f / (float)SLEEP_SECONDS);
    Serial.print(F("แนวโน้มความดัน "));
    Serial.print(trendPerHour, 2);
    Serial.println(F(" hPa/ชม."));
    if (trendPerHour < -1.0f) {
      Serial.println(F(">>> ความดันกำลังตกเร็ว อากาศอาจแปรปรวน <<<"));
    }
  }
  g_lastPressure = r.pressure;
  g_hasPrevious = true;
}

void goSleep() {
  Serial.print(F("หลับต่อ "));
  Serial.print(SLEEP_SECONDS);
  Serial.println(F(" วินาที"));
  Serial.flush();
#if defined(ARDUINO_ARCH_ESP32)
  /* เซ็นเซอร์อยู่ใน sleep เองแล้วหลังวัดเสร็จในโหมด forced */
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start(); /* บูตใหม่ที่ setup() เมื่อตื่น */
#else
  delay((uint32_t)SLEEP_SECONDS * 1000UL);
#endif
}

void loop() {
  /* ESP32 ไม่มาถึงบรรทัดนี้ (deep sleep รีบูตที่ setup()) บอร์ดอื่นวนตรงนี้ */
  runOnce();
  goSleep();
}
