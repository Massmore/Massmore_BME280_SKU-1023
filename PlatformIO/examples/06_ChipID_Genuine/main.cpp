/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้ว pio run -t upload
*/

#include <Arduino.h>

/*
  06_ChipID_Genuine - ตรวจว่าเป็นชิป Bosch BME280 ของแท้หรือไม่

  ทำไมต้องตรวจ
    ในตลาดมีบอร์ดที่สกรีนว่า BME280 แต่ข้างในเป็น BMP280 (วัดความชื้นไม่ได้) อยู่มาก
    หน้าตาบอร์ดเหมือนกันเป๊ะ วิธีเดียวที่แยกออกคือถามตัวชิปเอง

  ด่านแรก - รหัสประจำรุ่นที่รีจิสเตอร์ 0xD0
    0x60 BME280 / 0x58 BMP280 / 0x61 BME680 / 0x00, 0xFF ไม่มีอะไรตอบ
  ด่านสอง - verifyChip() ทดสอบพฤติกรรมจริงของซิลิคอนอีก 9 ข้อ
    (ช่วงค่าชดเชยที่ Bosch ใช้จริง, กลไก latch ของ ctrl_hum, บิต measuring ฯลฯ)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

MassmoreBME280 bme;

static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA, SCL);
#else
  Wire.begin();
#endif
}

static void printCheck(const __FlashStringHelper *name, bool passed) {
  Serial.print(passed ? F("  [ OK ] ") : F("  [FAIL] "));
  Serial.println(name);
}

/* สแกนบัสแล้วคืน address ของ BME280 ตัวแรกที่เจอ (0 = ไม่เจอ) */
static uint8_t scanBus() {
  uint8_t found = 0;
  Serial.println(F("[ สแกนบัส I2C ]"));
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  พบอุปกรณ์ที่ 0x"));
      Serial.println(address, HEX);
      if (found == 0 && (address == MASSMORE_BME280_I2C_ADDR_A ||
                         address == MASSMORE_BME280_I2C_ADDR_B)) {
        found = address;
      }
    }
  }
  if (found == 0) {
    Serial.println(F("  ไม่พบ BME280 ที่ 0x77 หรือ 0x76 ตรวจสาย VIN GND SDI SCK"));
  }
  return found;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Chip ID / Genuine"));
  Serial.println(F("=================================================="));
  busBegin();
}

void loop() {
  uint8_t address = scanBus();
  if (address == 0) {
    delay(5000);
    return;
  }

  /* ด่านที่ 1 : อ่าน chip id ตรง ๆ โดยไม่ผ่าน begin() เพื่อให้เห็นค่าจริงแม้ไม่ใช่ BME280 */
  Serial.println(F("[ ด่านที่ 1 : รหัสประจำรุ่น ]"));
  uint8_t id = 0;
  Wire.beginTransmission(address);
  Wire.write(MASSMORE_BME280_REG_CHIP_ID);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(address, (uint8_t)1) == 1) {
    id = (uint8_t)Wire.read();
  }
  Serial.print(F("  0xD0 = 0x"));
  Serial.println(id, HEX);

  if (id != MASSMORE_BME280_CHIP_ID_BME280) {
    if (id == MASSMORE_BME280_CHIP_ID_BMP280 || id == MASSMORE_BME280_CHIP_ID_BMP280_S1 ||
        id == MASSMORE_BME280_CHIP_ID_BMP280_S2) {
      Serial.println(F("  -> BMP280 : ไม่มีเซ็นเซอร์ความชื้น ถ้าบอร์ดสกรีน BME280 แสดงว่าติดฉลากผิด"));
    } else if (id == MASSMORE_BME280_CHIP_ID_BME680) {
      Serial.println(F("  -> BME680 : คนละตระกูล ต้องใช้ไลบรารีของ BME680"));
    } else {
      Serial.println(F("  -> ไม่รู้จัก อาจเป็นของเลียนแบบหรือชิปเสีย"));
    }
    delay(10000);
    return;
  }
  Serial.println(F("  -> 0x60 BME280"));

  /* ด่านที่ 2 : ทดสอบพฤติกรรมจริง */
  Serial.println(F("[ ด่านที่ 2 : ทดสอบพฤติกรรมจริงของชิป ]"));
  if (!bme.begin(address)) {
    Serial.print(F("  begin() ไม่ผ่าน : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    delay(10000);
    return;
  }

  massmore_bme280_identity_t idn;
  massmore_bme280_genuine_t verdict = bme.verifyChip(&idn);

  printCheck(F("CHIP_ID     รหัสชิปเป็น 0x60"), idn.chipIdOk);
  printCheck(F("CALIB_T     ค่าชดเชยอุณหภูมิอยู่ในช่วงที่ Bosch ใช้"), idn.calibTempOk);
  printCheck(F("CALIB_P     ค่าชดเชยความดันอยู่ในช่วงที่ Bosch ใช้"), idn.calibPressOk);
  printCheck(F("CALIB_H     มีค่าชดเชยความชื้นครบ"), idn.calibHumOk);
  printCheck(F("CALIB_UNIQ  ค่าชดเชยไม่ซ้ำแบบตารางปลอม"), idn.calibUniqueOk);
  printCheck(F("SOFT_RESET  รีเซ็ตแล้วรีจิสเตอร์กลับเป็นค่าโรงงาน"), idn.resetOk);
  printCheck(F("HUM_LATCH   ช่องความชื้นเปิดปิดตาม osrs_h ได้จริง"), idn.ctrlHumLatchOk);
  printCheck(F("REG_ECHO    เขียน config แล้วอ่านกลับได้ครบทุกบิต"), idn.registerEchoOk);
  printCheck(F("MEAS_BIT    บิต measuring ขึ้นลงตามเวลาจริง"), idn.measuringBitOk);
  printCheck(F("HUM_LIVE    ค่าความชื้นที่ได้อยู่ในโลกความจริง"), idn.humidityLiveOk);

  Serial.print(F("  ผ่าน "));
  Serial.print(idn.passCount);
  Serial.print(F("/10  สรุป : "));
  Serial.println(MassmoreBME280::genuineToString(verdict));

  /* ลายนิ้วมือของชิป ไม่ซ้ำกันระหว่างชิปสองตัว */
  massmore_bme280_calib_t c;
  bme.getCalibration(c);
  Serial.print(F("  ลายนิ้วมือ dig_T1/P1/H1 : "));
  Serial.print(c.dig_T1);
  Serial.print('/');
  Serial.print(c.dig_P1);
  Serial.print('/');
  Serial.println(c.dig_H1);

  Serial.println(F("ทดสอบซ้ำใน 15 วินาที"));
  Serial.println();
  delay(15000);
}
