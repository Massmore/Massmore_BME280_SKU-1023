/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  07_ChipID_Genuine - ตรวจว่าเป็นชิป Bosch BME280 ของแท้หรือไม่

  ทำไมต้องตรวจ
    ในตลาดมีบอร์ดที่สกรีนว่า BME280 แต่ข้างในเป็น BMP280 อยู่เยอะมาก
    BMP280 วัดความชื้นไม่ได้ ใช้งานแทนกันไม่ได้ แต่หน้าตาบอร์ดเหมือนกันเป๊ะ
    วิธีเดียวที่แยกออกคือถามตัวชิปเอง

  ด่านแรก - รหัสประจำรุ่นที่รีจิสเตอร์ 0xD0
    0x60  BME280   วัดได้ครบสามค่า
    0x58  BMP280   ไม่มีเซ็นเซอร์ความชื้น
    0x61  BME680   คนละตระกูล ใช้ไลบรารีนี้ไม่ได้
    0x00 / 0xFF    ไม่มีอะไรตอบ หรือสายขาด

  แต่รหัสชิปอย่างเดียวไม่พอ เพราะของปลอมคัดลอกตัวเลขนี้ได้ง่าย
  verifyChip() จึงทดสอบพฤติกรรมจริงของซิลิคอนอีก 9 ข้อ เช่น
    - ช่วงค่าของสัมประสิทธิ์ชดเชยที่โรงงาน Bosch ใช้จริง
    - กลไก latch ของ ctrl_hum ที่มีเฉพาะใน BME280
    - บิต measuring ต้องขึ้นลงตามเวลาจริงที่ใช้วัด

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreBME280 bme;

void printCheck(const char *name, bool passed, const char *meaning) {
  Serial.print(passed ? F("  [ OK ] ") : F("  [FAIL] "));
  Serial.print(name);
  Serial.print(F("  "));
  Serial.println(meaning);
}

void scanBus() {
  Serial.println(F("[ สแกนบัส I2C ]"));
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  พบอุปกรณ์ที่ 0x"));
      if (address < 0x10) {
        Serial.print('0');
      }
      Serial.print(address, HEX);
      if (address == MASSMORE_BME280_I2C_ADDR_A) {
        Serial.print(F("  <- ตำแหน่งปริยายของบอร์ด Massmore BME280"));
      } else if (address == MASSMORE_BME280_I2C_ADDR_B) {
        Serial.print(F("  <- ตำแหน่งเมื่อบัดกรีจัมเปอร์ ADDR"));
      }
      Serial.println();
      found++;
    }
  }
  if (found == 0) {
    Serial.println(F("  ไม่พบอุปกรณ์ใดเลย ตรวจสาย VCC GND SDA SCL"));
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - ตรวจชิปของแท้"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  scanBus();
}

void loop() {
  Serial.println(F("[ ด่านที่ 1 : รหัสประจำรุ่น ]"));

  /* อ่าน chip id ตรง ๆ โดยไม่ผ่าน begin() เพื่อให้เห็นค่าจริงแม้จะไม่ใช่ BME280 */
  uint8_t id = 0;
  Wire.beginTransmission(MASSMORE_BME280_I2C_ADDR_A);
  Wire.write(MASSMORE_BME280_REG_CHIP_ID);
  bool ok = (Wire.endTransmission(false) == 0);
  if (ok && Wire.requestFrom((uint8_t)MASSMORE_BME280_I2C_ADDR_A, (uint8_t)1) == 1) {
    id = (uint8_t)Wire.read();
  } else {
    ok = false;
  }

  if (!ok) {
    Serial.println(F("  อ่านรหัสชิปไม่ได้ ไม่มีอุปกรณ์ตอบที่ 0x76"));
    Serial.println(F("  ถ้าบัดกรีจัมเปอร์ ADDR ไว้ ให้แก้โค้ดเป็น 0x77"));
    delay(5000);
    return;
  }

  Serial.print(F("  รหัสที่อ่านได้ : 0x"));
  if (id < 0x10) {
    Serial.print('0');
  }
  Serial.println(id, HEX);

  switch (id) {
    case MASSMORE_BME280_CHIP_ID_BME280:
      Serial.println(F("  -> BME280 ของแท้ วัดได้ทั้งอุณหภูมิ ความชื้น และความดัน"));
      break;
    case MASSMORE_BME280_CHIP_ID_BMP280:
    case MASSMORE_BME280_CHIP_ID_BMP280_S1:
    case MASSMORE_BME280_CHIP_ID_BMP280_S2:
      Serial.println(F("  -> BMP280 ตัวนี้ไม่มีเซ็นเซอร์ความชื้น"));
      Serial.println(F("     ถ้าบอร์ดสกรีนว่า BME280 แสดงว่าติดฉลากผิดรุ่น"));
      Serial.println(F("     ให้ตรวจช่องติ๊กบนบอร์ดว่าติ๊ก BME280 หรือ BMP280"));
      delay(10000);
      return;
    case MASSMORE_BME280_CHIP_ID_BME680:
      Serial.println(F("  -> BME680 คนละตระกูล ต้องใช้ไลบรารีของ BME680"));
      delay(10000);
      return;
    default:
      Serial.println(F("  -> รหัสไม่ตรงกับรุ่นใดที่รู้จัก อาจเป็นของเลียนแบบหรือชิปเสีย"));
      delay(10000);
      return;
  }

  Serial.println();
  Serial.println(F("[ ด่านที่ 2 : ทดสอบพฤติกรรมจริงของชิป ]"));

  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("  begin() ไม่ผ่าน : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    delay(10000);
    return;
  }

  massmore_bme280_identity_t identity;
  massmore_bme280_genuine_t verdict = bme.verifyChip(&identity);

  printCheck("CHIP_ID    ", identity.chipIdOk, "รหัสชิปเป็น 0x60");
  printCheck("CALIB_T    ", identity.calibTempOk, "ค่าชดเชยอุณหภูมิอยู่ในช่วงที่ Bosch ใช้");
  printCheck("CALIB_P    ", identity.calibPressOk, "ค่าชดเชยความดันอยู่ในช่วงที่ Bosch ใช้");
  printCheck("CALIB_H    ", identity.calibHumOk, "มีค่าชดเชยความชื้นครบ");
  printCheck("CALIB_UNIQ ", identity.calibUniqueOk, "ค่าชดเชยไม่ซ้ำแบบตารางปลอม");
  printCheck("SOFT_RESET ", identity.resetOk, "รีเซ็ตแล้วรีจิสเตอร์กลับเป็นค่าโรงงาน");
  printCheck("HUM_LATCH  ", identity.ctrlHumLatchOk, "ช่องความชื้นเปิดปิดตาม osrs_h ได้จริง");
  printCheck("REG_ECHO   ", identity.registerEchoOk, "เขียน config แล้วอ่านกลับได้ครบทุกบิต");
  printCheck("MEAS_BIT   ", identity.measuringBitOk, "บิต measuring ขึ้นลงตามเวลาจริง");
  printCheck("HUM_LIVE   ", identity.humidityLiveOk, "ค่าความชื้นที่ได้อยู่ในโลกความจริง");

  Serial.println();
  Serial.print(F("  ผ่าน "));
  Serial.print(identity.passCount);
  Serial.print(F(" จาก 10 ข้อ   สรุป : "));
  Serial.println(MassmoreBME280::genuineToString(verdict));

  if (verdict == MASSMORE_BME280_GENUINE_YES) {
    Serial.println(F("\n  >>> ชิปตัวนี้เป็น Bosch BME280 ของแท้ ใช้งานได้เต็มที่ <<<"));
  } else if (verdict == MASSMORE_BME280_GENUINE_SUSPECT) {
    Serial.println(F("\n  >>> มีบางข้อไม่ผ่าน อาจเป็นชิปเกรดรอง หรือมีสัญญาณรบกวนบนบัส"));
    Serial.println(F("      ลองย้ายไปสาย Qwiic ที่สั้นลง แล้วทดสอบใหม่ <<<"));
  } else {
    Serial.println(F("\n  >>> ไม่ผ่าน ชิปตัวนี้ไม่ใช่ BME280 ของแท้ <<<"));
  }

  /* ลายนิ้วมือของชิป ใช้เทียบว่าบอร์ดสองแผ่นเป็นคนละตัวจริง */
  massmore_bme280_calib_t c;
  bme.getCalibration(c);
  Serial.print(F("\n  ลายนิ้วมือ (dig_T1/P1/H1) : "));
  Serial.print(c.dig_T1);
  Serial.print(F(" / "));
  Serial.print(c.dig_P1);
  Serial.print(F(" / "));
  Serial.println(c.dig_H1);
  Serial.println(F("  ค่าชุดนี้ไม่ซ้ำกันระหว่างชิปสองตัว จดไว้เทียบภายหลังได้"));

  Serial.println();
  Serial.println(F("ทดสอบซ้ำอีกครั้งใน 15 วินาที"));
  delay(15000);
}
