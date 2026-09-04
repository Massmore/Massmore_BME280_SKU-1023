/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  08_NonBlocking - อ่านค่าโดยไม่ทำให้โปรแกรมค้างแม้แต่มิลลิวินาทีเดียว

  ปัญหาที่ตัวอย่างนี้แก้
    ถ้าใช้ bme.read() ในโหมด forced โปรแกรมจะหยุดรอชิปวัดเสร็จ
    ที่ oversampling x16 ทุกช่อง เวลารอคือ 113 ms
    ระหว่างนั้นไฟจะกะพริบไม่ตรงจังหวะ ปุ่มจะกดไม่ติด งาน WiFi จะสะดุด

  วิธีแก้
    แยกเป็นสามขั้น  สั่งวัด -> ทำอย่างอื่นไปเรื่อย ๆ -> พอพร้อมค่อยอ่าน

      bme.startForcedMeasurement();   // สั่งแล้วคืนค่าทันที
      ...ทำงานอื่น...
      if (bme.isMeasurementReady()) { // ไม่บล็อก คืนค่าทันที
        bme.read(reading);            // ตอนนี้ค่าพร้อมแล้ว อ่านได้เร็ว
      }

  ตัวอย่างนี้จะกะพริบไฟบนบอร์ดทุก 100 ms พร้อมกับวัดค่าไปด้วย
  ถ้าไฟกะพริบสม่ำเสมอไม่มีสะดุด แปลว่าโค้ดไม่บล็อกจริง
  และจะวัดเวลาที่ loop() ใช้ต่อรอบให้ดูด้วย

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

/* ไฟบนบอร์ด ESP32 DevKit ส่วนใหญ่อยู่ที่ GPIO 2 ถ้าบอร์ดคุณไม่มีก็ไม่เป็นไร */
#define PIN_LED 2

#define BLINK_INTERVAL_MS 100
#define MEASURE_INTERVAL_MS 1000

MassmoreBME280 bme;

/* สถานะของเครื่องสถานะ (state machine) */
enum MeasureState { STATE_IDLE, STATE_WAITING };
static MeasureState g_state = STATE_IDLE;

static uint32_t g_lastMeasureStart = 0;
static uint32_t g_lastBlink = 0;
static bool g_ledOn = false;

/* ตัวเลขสำหรับพิสูจน์ว่าไม่บล็อกจริง */
static uint32_t g_loopCount = 0;
static uint32_t g_longestLoopUs = 0;
static uint32_t g_lastReport = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  pinMode(PIN_LED, OUTPUT);

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Non-Blocking"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  /* ตั้งใจใช้ x16 ทุกช่อง เพื่อให้เวลาวัดยาวที่สุด (113 ms)
     จะได้เห็นชัดว่าถ้าเขียนแบบบล็อกจะแย่แค่ไหน */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);

  Serial.print(F("หนึ่งรอบวัดใช้เวลาตามดาต้าชีต "));
  Serial.print(bme.measurementTimeMs());
  Serial.print(F(" ms (กรณีแย่สุด "));
  Serial.print(bme.measurementTimeMaxMs());
  Serial.println(F(" ms)"));
  Serial.println(F("ถ้าเขียนแบบบล็อก loop() จะค้างนานเท่านี้ทุกครั้งที่วัด"));
  Serial.println();

  g_lastMeasureStart = millis();
  g_lastBlink = millis();
  g_lastReport = millis();
}

void loop() {
  uint32_t loopStart = micros();
  uint32_t now = millis();
  g_loopCount++;

  /* ---- งานที่ 1 : กะพริบไฟให้ตรงจังหวะเป๊ะ ๆ ---- */
  if (now - g_lastBlink >= BLINK_INTERVAL_MS) {
    g_lastBlink += BLINK_INTERVAL_MS;
    g_ledOn = !g_ledOn;
    digitalWrite(PIN_LED, g_ledOn ? HIGH : LOW);
  }

  /* ---- งานที่ 2 : เครื่องสถานะสำหรับการวัด ---- */
  switch (g_state) {
    case STATE_IDLE:
      /* ถึงเวลาวัดรอบใหม่หรือยัง */
      if (now - g_lastMeasureStart >= MEASURE_INTERVAL_MS) {
        g_lastMeasureStart = now;
        if (bme.startForcedMeasurement()) {
          g_state = STATE_WAITING;
        } else {
          Serial.print(F("สั่งวัดไม่สำเร็จ : "));
          Serial.println(MassmoreBME280::errorToString(bme.lastError()));
        }
      }
      break;

    case STATE_WAITING:
      /* ถามว่าพร้อมหรือยัง ฟังก์ชันนี้คืนค่าทันทีไม่รอ */
      if (bme.isMeasurementReady()) {
        massmore_bme280_reading_t reading;
        if (bme.read(reading)) {
          Serial.print(F("["));
          Serial.print(now);
          Serial.print(F(" ms]  "));
          Serial.print(reading.temperature, 2);
          Serial.print(F(" C   "));
          Serial.print(reading.humidity, 2);
          Serial.print(F(" %RH   "));
          Serial.print(reading.pressure, 2);
          Serial.print(F(" hPa   ใช้เวลารอ "));
          Serial.print(millis() - g_lastMeasureStart);
          Serial.println(F(" ms"));
        }
        g_state = STATE_IDLE;
      }
      break;
  }

  /* ---- งานที่ 3 : รายงานว่า loop() เร็วแค่ไหน ---- */
  uint32_t loopUs = micros() - loopStart;
  if (loopUs > g_longestLoopUs) {
    g_longestLoopUs = loopUs;
  }

  if (now - g_lastReport >= 10000) {
    g_lastReport = now;
    Serial.print(F("  >> ใน 10 วินาทีที่ผ่านมา loop() วนไป "));
    Serial.print(g_loopCount);
    Serial.print(F(" รอบ   รอบที่ช้าที่สุดใช้ "));
    Serial.print(g_longestLoopUs);
    Serial.println(F(" ไมโครวินาที"));
    Serial.println(F("     (ถ้าเขียนแบบบล็อกจะเห็นตัวเลขนี้แตะ 113000 ไมโครวินาที)"));
    g_loopCount = 0;
    g_longestLoopUs = 0;
  }

  /* ไม่มี delay() ในลูปนี้เลย นี่คือหัวใจของการเขียนแบบไม่บล็อก */
}
