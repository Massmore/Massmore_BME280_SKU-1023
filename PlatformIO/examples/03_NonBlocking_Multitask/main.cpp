/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้ว pio run -t upload
*/

#include <Arduino.h>

/*
  03_NonBlocking_Multitask - วัดค่าโดยไม่ทำให้ loop() ค้าง พร้อมทำงานอื่นไปด้วย

  ปัญหา
    bme.read() ในโหมด forced จะรอชิปวัดเสร็จ ที่ oversampling x16 คือ 113 ms
    ระหว่างนั้นไฟจะกะพริบไม่ตรงจังหวะ ปุ่มกดไม่ติด งาน WiFi สะดุด

  วิธีแก้ - FSM ไม่บล็อกของไลบรารี (ชื่อมาตรฐาน Massmore)
    bme.requestConversion();   สั่งวัดแล้วคืนค่าทันที
    bme.update();              เรียกทุกรอบ loop() ไม่มี delay ข้างใน
    bme.isDataReady();         ผลพร้อมหรือยัง
    bme.getReadings(r);        รับผลแล้ว FSM กลับสู่ IDLE

  งานที่รันพร้อมกันในตัวอย่างนี้
    งานที่ 1  กะพริบ LED ทุก 100 ms ด้วย millis()
    งานที่ 2  วัดค่าทุก 1 วินาทีผ่าน FSM
    งานที่ 3  วัดว่า loop() รอบที่ช้าที่สุดกินกี่ไมโครวินาที (พิสูจน์ว่าไม่บล็อกจริง)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 /* ESP32 DevKit ส่วนใหญ่อยู่ที่ GPIO2 */
#endif

#define BLINK_INTERVAL_MS 100
#define MEASURE_INTERVAL_MS 1000

MassmoreBME280 bme;

static uint32_t g_lastMeasureStart = 0;
static uint32_t g_lastBlink = 0;
static bool g_ledOn = false;
static uint32_t g_loopCount = 0;
static uint32_t g_longestLoopUs = 0;
static uint32_t g_lastReport = 0;

static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA, SCL);
#else
  Wire.begin();
#endif
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Non-Blocking Multitask"));
  Serial.println(F("=================================================="));

  busBegin();
  if (!bme.begin()) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  /* ตั้งใจใช้ x16 ทุกช่องในโหมด forced ให้เวลาวัดยาวที่สุด (113 ms)
     จะได้เห็นชัดว่า loop() ยังลื่นอยู่ */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);

  Serial.print(F("หนึ่งรอบวัดใช้เวลาตามดาต้าชีต "));
  Serial.print(bme.measurementTimeMs());
  Serial.print(F(" ms (กรณีแย่สุด "));
  Serial.print(bme.measurementTimeMaxMs());
  Serial.println(F(" ms)"));
  Serial.println();

  g_lastMeasureStart = millis();
  g_lastBlink = millis();
  g_lastReport = millis();
}

void loop() {
  uint32_t loopStart = micros();
  uint32_t now = millis();
  g_loopCount++;

  /* ---- งานที่ 1 : กะพริบไฟให้ตรงจังหวะ ---- */
  if (now - g_lastBlink >= BLINK_INTERVAL_MS) {
    g_lastBlink += BLINK_INTERVAL_MS;
    g_ledOn = !g_ledOn;
    digitalWrite(LED_BUILTIN, g_ledOn ? HIGH : LOW);
  }

  /* ---- งานที่ 2 : FSM วัดค่า ---- */
  bme.update(); /* เดิน FSM หนึ่งก้าว คืนค่าทันที */

  if (bme.isDataReady()) {
    massmore_bme280_reading_t r;
    if (bme.getReadings(r)) {
      Serial.print(F("["));
      Serial.print(now);
      Serial.print(F(" ms]  "));
      Serial.print(r.temperature, 2);
      Serial.print(F(" C   "));
      Serial.print(r.humidity, 2);
      Serial.print(F(" %RH   "));
      Serial.print(r.pressure, 2);
      Serial.print(F(" hPa   ใช้เวลารอ "));
      Serial.print(now - g_lastMeasureStart);
      Serial.println(F(" ms"));
    }
  } else if (bme.getState() == MASSMORE_BME280_STATE_ERROR) {
    Serial.print(F("FSM ผิดพลาด : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
  }

  if (bme.getState() != MASSMORE_BME280_STATE_MEASURING &&
      now - g_lastMeasureStart >= MEASURE_INTERVAL_MS) {
    g_lastMeasureStart = now;
    bme.requestConversion(); /* สั่งวัดรอบใหม่ คืนค่าทันที */
  }

  /* ---- งานที่ 3 : รายงานว่า loop() เร็วแค่ไหน ---- */
  uint32_t loopUs = micros() - loopStart;
  if (loopUs > g_longestLoopUs) {
    g_longestLoopUs = loopUs;
  }
  if (now - g_lastReport >= 10000) {
    g_lastReport = now;
    Serial.print(F("  >> 10 วินาทีที่ผ่านมา loop() วน "));
    Serial.print(g_loopCount);
    Serial.print(F(" รอบ  รอบที่ช้าสุด "));
    Serial.print(g_longestLoopUs);
    Serial.println(F(" us  (ถ้าบล็อกจะเห็นแตะ 113000)"));
    g_loopCount = 0;
    g_longestLoopUs = 0;
  }
  /* ไม่มี delay() ในลูปนี้เลย */
}
