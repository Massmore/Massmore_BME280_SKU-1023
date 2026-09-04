/*
  06_RawData_Calibration - เปิดฝาดูข้างในชิป ค่าดิบ ADC และค่าชดเชยทั้ง 32 ตัว

  ชิป BME280 ไม่ได้ส่งค่าอุณหภูมิเป็นองศามาให้ตรง ๆ
  สิ่งที่ชิปส่งมาคือเลข ADC ดิบ 20 บิต แล้วเราต้องเอาไปเข้าสูตรชดเชย
  ร่วมกับสัมประสิทธิ์ 32 ตัวที่โรงงานเบิร์นไว้ใน NVM ของชิปแต่ละตัว

  ค่าชดเชยชุดนี้ไม่ซ้ำกันเลยระหว่างชิปสองตัว จึงใช้เป็น "ลายนิ้วมือ" ได้
  ถ้าเจอบอร์ดสองแผ่นที่ค่าชดเชยตรงกันเป๊ะ แปลว่าอย่างน้อยแผ่นหนึ่งไม่ใช่ของแท้

  ตัวอย่างนี้แสดง
    - ค่าชดเชยทั้ง 32 ตัว พร้อมตำแหน่งรีจิสเตอร์ที่มันอยู่
    - ค่าดิบ ADC ของทั้งสามช่อง
    - ค่า t_fine ที่เป็นตัวกลางเชื่อมสูตรทั้งสาม
    - ผลลัพธ์แต่ละขั้น ตั้งแต่เลขดิบจนเป็นองศา

  การต่อสาย : SDA -> GPIO 21, SCL -> GPIO 22 (หรือเสียบสาย Qwiic)

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreBME280 bme;

void printCalibration() {
  massmore_bme280_calib_t c;
  bme.getCalibration(c);

  Serial.println(F("[ ค่าชดเชยจากโรงงาน - ชุดอุณหภูมิ ]"));
  Serial.print(F("  dig_T1  0x88  = "));
  Serial.println(c.dig_T1);
  Serial.print(F("  dig_T2  0x8A  = "));
  Serial.println(c.dig_T2);
  Serial.print(F("  dig_T3  0x8C  = "));
  Serial.println(c.dig_T3);

  Serial.println(F("\n[ ค่าชดเชยจากโรงงาน - ชุดความดัน ]"));
  Serial.print(F("  dig_P1  0x8E  = "));
  Serial.println(c.dig_P1);
  Serial.print(F("  dig_P2  0x90  = "));
  Serial.println(c.dig_P2);
  Serial.print(F("  dig_P3  0x92  = "));
  Serial.println(c.dig_P3);
  Serial.print(F("  dig_P4  0x94  = "));
  Serial.println(c.dig_P4);
  Serial.print(F("  dig_P5  0x96  = "));
  Serial.println(c.dig_P5);
  Serial.print(F("  dig_P6  0x98  = "));
  Serial.println(c.dig_P6);
  Serial.print(F("  dig_P7  0x9A  = "));
  Serial.println(c.dig_P7);
  Serial.print(F("  dig_P8  0x9C  = "));
  Serial.println(c.dig_P8);
  Serial.print(F("  dig_P9  0x9E  = "));
  Serial.println(c.dig_P9);

  Serial.println(F("\n[ ค่าชดเชยจากโรงงาน - ชุดความชื้น ]"));
  Serial.print(F("  dig_H1  0xA1  = "));
  Serial.println(c.dig_H1);
  Serial.print(F("  dig_H2  0xE1  = "));
  Serial.println(c.dig_H2);
  Serial.print(F("  dig_H3  0xE3  = "));
  Serial.println(c.dig_H3);
  Serial.print(F("  dig_H4  0xE4  = "));
  Serial.print(c.dig_H4);
  Serial.println(F("   (12 บิต ใช้ไบต์ 0xE5 ครึ่งล่างร่วมด้วย)"));
  Serial.print(F("  dig_H5  0xE6  = "));
  Serial.print(c.dig_H5);
  Serial.println(F("   (12 บิต ใช้ไบต์ 0xE5 ครึ่งบนร่วมด้วย)"));
  Serial.print(F("  dig_H6  0xE7  = "));
  Serial.println(c.dig_H6);
}

void printRawWalkthrough() {
  massmore_bme280_raw_t raw;
  if (!bme.readRawADC(raw)) {
    Serial.print(F("อ่านค่าดิบไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    return;
  }

  Serial.println(F("\n[ ค่าดิบจาก ADC (รีจิสเตอร์ 0xF7-0xFE) ]"));
  Serial.print(F("  adc_T = "));
  Serial.print(raw.adc_T);
  Serial.print(F("  (0x"));
  Serial.print(raw.adc_T, HEX);
  Serial.print(F(")"));
  if (raw.adc_T == MASSMORE_BME280_RAW_SKIPPED_20BIT) {
    Serial.print(F("  <- ช่องนี้ถูกปิดอยู่"));
  }
  Serial.println();

  Serial.print(F("  adc_P = "));
  Serial.print(raw.adc_P);
  Serial.print(F("  (0x"));
  Serial.print(raw.adc_P, HEX);
  Serial.print(F(")"));
  if (raw.adc_P == MASSMORE_BME280_RAW_SKIPPED_20BIT) {
    Serial.print(F("  <- ช่องนี้ถูกปิดอยู่"));
  }
  Serial.println();

  Serial.print(F("  adc_H = "));
  Serial.print(raw.adc_H);
  Serial.print(F("  (0x"));
  Serial.print(raw.adc_H, HEX);
  Serial.print(F(")"));
  if (raw.adc_H == MASSMORE_BME280_RAW_SKIPPED_16BIT) {
    Serial.print(F("  <- ช่องนี้ถูกปิดอยู่"));
  }
  Serial.println();

  Serial.println(F("\n[ เดินตามสูตรชดเชยทีละขั้น ]"));

  int32_t t = bme.compensateTemperature(raw.adc_T);
  Serial.print(F("  1. compensateTemperature(adc_T) = "));
  Serial.print(t);
  Serial.print(F("  -> "));
  Serial.print((float)t / 100.0f, 2);
  Serial.println(F(" องศาเซลเซียส  (ผลลัพธ์เป็นหน่วย 0.01 องศา)"));

  Serial.print(F("  2. t_fine = "));
  Serial.print(bme.getTFine());
  Serial.println(F("  <- ตัวกลางที่สูตรความดันและความชื้นต้องใช้ต่อ"));

  uint32_t p = bme.compensatePressure(raw.adc_P);
  Serial.print(F("  3. compensatePressure(adc_P) = "));
  Serial.print(p);
  Serial.print(F("  (Q24.8)  -> "));
  Serial.print((float)p / 256.0f, 2);
  Serial.print(F(" Pa  = "));
  Serial.print((float)p / 25600.0f, 2);
  Serial.println(F(" hPa"));

  uint32_t h = bme.compensateHumidity(raw.adc_H);
  Serial.print(F("  4. compensateHumidity(adc_H) = "));
  Serial.print(h);
  Serial.print(F("  (Q22.10) -> "));
  Serial.print((float)h / 1024.0f, 2);
  Serial.println(F(" %RH"));
}

void printRegisterDump() {
  Serial.println(F("\n[ รีจิสเตอร์ตั้งค่าปัจจุบัน ]"));

  struct RegInfo {
    uint8_t address;
    const char *name;
  };
  static const RegInfo regs[5] = {{0xD0, "id       "},
                                  {0xF2, "ctrl_hum "},
                                  {0xF3, "status   "},
                                  {0xF4, "ctrl_meas"},
                                  {0xF5, "config   "}};

  for (uint8_t i = 0; i < 5; i++) {
    uint8_t value = 0;
    if (bme.readRegister(regs[i].address, value)) {
      Serial.print(F("  0x"));
      Serial.print(regs[i].address, HEX);
      Serial.print(F("  "));
      Serial.print(regs[i].name);
      Serial.print(F(" = 0x"));
      if (value < 0x10) {
        Serial.print('0');
      }
      Serial.print(value, HEX);
      Serial.print(F("   0b"));
      for (int8_t bit = 7; bit >= 0; bit--) {
        Serial.print((value >> bit) & 0x01);
      }
      Serial.println();
    }
  }

  Serial.println(F("\n  วิธีอ่าน ctrl_meas : บิต 7-5 = osrs_t, บิต 4-2 = osrs_p, บิต 1-0 = mode"));
  Serial.println(F("  วิธีอ่าน config    : บิต 7-5 = t_sb,   บิต 4-2 = filter, บิต 0 = spi3w_en"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Raw Data"));
  Serial.println(F("=================================================="));

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(MASSMORE_BME280_I2C_ADDR_A, &Wire)) {
    Serial.print(F("เริ่มต้นเซ็นเซอร์ไม่สำเร็จ : "));
    Serial.println(MassmoreBME280::errorToString(bme.lastError()));
    while (true) {
      delay(1000);
    }
  }

  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X4,
                  MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_X4,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_250_MS);
  delay(100);

  printCalibration();
  printRegisterDump();

  Serial.println();
  Serial.println(F("ค่าชดเชยด้านบนคือลายนิ้วมือของชิปตัวนี้ จดไว้เทียบภายหลังได้"));
  Serial.println(F("ต่อจากนี้จะแสดงค่าดิบทุก 5 วินาที"));
}

void loop() {
  Serial.println(F("\n=========================================="));
  printRawWalkthrough();
  delay(5000);
}
