/*
  05_Altitude_SeaLevel - วัดความสูงให้แม่น ด้วยการปรับเทียบระดับน้ำทะเล

  ความจริงที่ต้องเข้าใจก่อน
    BME280 วัด "ความดันอากาศ" ไม่ได้วัดความสูงโดยตรง
    การแปลงความดันเป็นความสูงต้องรู้ว่าความดันที่ระดับน้ำทะเล ณ เวลานั้นเป็นเท่าไร
    ซึ่งเปลี่ยนทุกวันตามสภาพอากาศ (ปกติ 1000-1025 hPa)

    ถ้าใช้ค่ามาตรฐาน 1013.25 ไปเลย ความสูงจะคลาดเคลื่อนได้ถึง +/- 100 เมตร
    แต่ถ้าปรับเทียบให้ถูก จะเหลือความคลาดเคลื่อนระดับ 1 เมตร

  ตัวอย่างนี้มีสองโหมด
    โหมดปรับเทียบ  บอกความสูงจริงของจุดที่ตั้งบอร์ด ระบบจะคำนวณระดับน้ำทะเลให้
    โหมดวัด        วัดความสูงเทียบกับจุดอ้างอิงที่ตั้งไว้ (วัดความสูงสัมพัทธ์ได้ละเอียดมาก)

  วิธีใช้ - พิมพ์คำสั่งใน Serial Monitor แล้วกด Enter
    c<ตัวเลข>   ปรับเทียบด้วยความสูงจริงเป็นเมตร  เช่น  c12.5
    s<ตัวเลข>   ตั้งความดันระดับน้ำทะเลตรง ๆ       เช่น  s1008.4
    z           ตั้งจุดนี้เป็นศูนย์ (วัดความสูงสัมพัทธ์)
    ?           แสดงค่าที่ตั้งไว้ทั้งหมด

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreBME280 bme;

static float g_zeroAltitude = NAN; /* ความสูงที่ถือเป็นศูนย์ */

void printHelp() {
  Serial.println(F("คำสั่งที่ใช้ได้"));
  Serial.println(F("  c12.5      ปรับเทียบโดยบอกว่าจุดนี้สูงจากระดับน้ำทะเล 12.5 เมตร"));
  Serial.println(F("  s1008.4    ตั้งความดันระดับน้ำทะเลเป็น 1008.4 hPa โดยตรง"));
  Serial.println(F("  z          ตั้งจุดปัจจุบันเป็นศูนย์ (วัดความสูงสัมพัทธ์)"));
  Serial.println(F("  ?          แสดงค่าที่ตั้งไว้"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Altitude"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  /* งานวัดความสูงต้องการความดันที่นิ่งที่สุด จึงใช้ชุด indoor navigation
     ที่ Bosch แนะนำ : ความดัน x16, อุณหภูมิ x2, ฟิลเตอร์ 16, พัก 0.5 ms
     ชุดนี้ให้ noise ต่ำถึงระดับ 0.02 hPa ซึ่งเทียบเท่าความสูงราว 17 เซนติเมตร */
  bme.useIndoorNavigationPreset();

  Serial.println(F("ใช้ชุดตั้งค่า indoor navigation (ความดัน x16 + ฟิลเตอร์ 16)"));
  Serial.println(F("รอสักครู่ให้ฟิลเตอร์เข้าที่..."));
  for (uint8_t i = 0; i < 40; i++) {
    massmore_bme280_reading_t discard;
    bme.read(discard);
    delay(25);
  }
  Serial.println();
  printHelp();
  Serial.println();
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  char command = line.charAt(0);
  float value = line.substring(1).toFloat();

  if (command == 'c' || command == 'C') {
    massmore_bme280_reading_t r;
    if (!bme.read(r)) {
      Serial.println(F("อ่านค่าไม่สำเร็จ ลองใหม่"));
      return;
    }
    /* คำนวณย้อนกลับ : รู้ความสูงจริงกับความดันที่วัดได้ หาความดันระดับน้ำทะเล */
    float sea = MassmoreBME280::seaLevelForAltitude(value, r.pressure);
    bme.setSeaLevelPressure(sea);
    Serial.print(F("ปรับเทียบแล้ว : ที่ความสูง "));
    Serial.print(value, 1);
    Serial.print(F(" m วัดได้ "));
    Serial.print(r.pressure, 2);
    Serial.print(F(" hPa จึงตั้งระดับน้ำทะเลเป็น "));
    Serial.print(sea, 2);
    Serial.println(F(" hPa"));
  } else if (command == 's' || command == 'S') {
    if (value < 800.0f || value > 1200.0f) {
      Serial.println(F("ค่าที่ใส่อยู่นอกช่วงที่เป็นไปได้ (800-1200 hPa)"));
      return;
    }
    bme.setSeaLevelPressure(value);
    Serial.print(F("ตั้งระดับน้ำทะเลเป็น "));
    Serial.print(value, 2);
    Serial.println(F(" hPa"));
  } else if (command == 'z' || command == 'Z') {
    massmore_bme280_reading_t r;
    if (bme.read(r)) {
      g_zeroAltitude = r.altitude;
      Serial.print(F("ตั้งจุดนี้เป็นศูนย์แล้ว (ความสูงสัมบูรณ์ "));
      Serial.print(g_zeroAltitude, 2);
      Serial.println(F(" m)"));
    }
  } else if (command == '?') {
    Serial.print(F("ระดับน้ำทะเลที่ตั้งไว้ : "));
    Serial.print(bme.getSeaLevelPressure(), 2);
    Serial.println(F(" hPa"));
    Serial.print(F("จุดศูนย์ : "));
    if (isnan(g_zeroAltitude)) {
      Serial.println(F("ยังไม่ได้ตั้ง"));
    } else {
      Serial.print(g_zeroAltitude, 2);
      Serial.println(F(" m"));
    }
  } else {
    printHelp();
  }
}

void loop() {
  if (Serial.available()) {
    handleCommand(Serial.readStringUntil('\n'));
  }

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 1000) {
    return;
  }
  lastPrint = millis();

  massmore_bme280_reading_t r;
  if (!bme.read(r)) {
    Serial.print(F("อ่านค่าไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  Serial.print(F("ความดัน "));
  Serial.print(r.pressure, 3);
  Serial.print(F(" hPa   ความสูง "));
  Serial.print(r.altitude, 2);
  Serial.print(F(" m"));

  if (!isnan(g_zeroAltitude)) {
    float relative = r.altitude - g_zeroAltitude;
    Serial.print(F("   เทียบจุดศูนย์ "));
    Serial.print(relative >= 0 ? F("+") : F(""));
    Serial.print(relative, 2);
    Serial.print(F(" m"));
  }

  Serial.print(F("   อุณหภูมิ "));
  Serial.print(r.temperature, 2);
  Serial.println(F(" C"));
}
