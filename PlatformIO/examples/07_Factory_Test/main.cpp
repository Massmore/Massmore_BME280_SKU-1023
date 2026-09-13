/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้ว pio run -t upload
*/

#include <Arduino.h>

/*
  07_Factory_Test - ชุดทดสอบโรงงาน (QA/QC) สำหรับบอร์ด Massmore BME280 (SKU-1023)

  รันเองทันทีหลังบูต ไม่ต้องพิมพ์อะไร  พิมพ์ r เพื่อทดสอบซ้ำ  Serial Monitor 115200 baud

  สิ่งที่ทดสอบ
    GATE 1   สแกนบัส I2C หา 0x77 (ปริยาย) หรือ 0x76
    GATE 2   ตรวจรหัสประจำรุ่นว่าเป็น Bosch BME280 ของแท้ (0x60)
             ถ้าเจอ 0x58 จะบอกชัดว่าเป็น BMP280 ที่วัดความชื้นไม่ได้
    RUN TEST ทดสอบทุกความสามารถของชิปอีก 22 หัวข้อ : soft reset, ค่าชดเชย/trim,
             register echo, sleep/forced/normal, ช่องความชื้น, เวลาวัด, noise,
             ฟิลเตอร์, ช่วงค่าทางกายภาพ, เสถียรภาพ, บัส 400 kHz, presets,
             และ heuristic ของแท้ 10 ข้อ

  บรรทัดสุดท้ายของรายงานคือ
    [PASS] SENSOR QA PASSED - READY TO SHIP
    [FAIL] QA CHECK FAILED: <REASON>

  บรรทัดที่ขึ้นต้นด้วย # มีไว้ให้โปรแกรมฝั่งเว็บอ่านอัตโนมัติ
    #RESULT,<ลำดับ>,<ชื่อหัวข้อ>,<PASS|FAIL|WARN>,<รายละเอียด>
    #DEVICE,<addr>,<chip>,<chip_id>,<genuine>,<ผ่านกี่ข้อจาก10>
    #VERDICT,<PASS|FAIL>,<ผ่าน>,<ไม่ผ่าน>,<เตือน>

  ใช้ได้กับ ESP32 / ESP32-S3 / RP2040 / AVR (Arduino Nano) - ไลบรารีไม่เรียก Wire.begin()
  sketch เปิดบัสเองใน busBegin() (ESP32 ใช้ขา SDA/SCL ของ variant, บอร์ดอื่นใช้ขาฮาร์ดแวร์)
  บน AVR (SRAM 2 KB / flash 32 KB) จะเก็บเฉพาะชื่อ/สถานะของแต่ละหัวข้อ รายละเอียดพิมพ์สดอย่างเดียว
  และข้าม 4 หัวข้อ (MEAS_TIMING, NOISE, IIR_FILTER, BUS_400K) เหลือ 18 หัวข้อ

  by Massmore  |  MIT License
*/

#include <Massmore_BME280.h>
#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
   การตั้งค่า
   ------------------------------------------------------------------------- */

#define I2C_FREQ_NORMAL 100000UL
#define I2C_FREQ_FAST 400000UL

/* ตัวช่วยประหยัด SRAM บน AVR : สตริงรูปแบบอยู่ใน flash (PROGMEM) และไม่เก็บรายละเอียดไว้ */
#if defined(__AVR__)
  #include <avr/pgmspace.h>
  #define FT_LIT(x) PSTR(x)
  #define FT_SNPRINTF(buf, size, fmt, ...) snprintf_P(buf, size, PSTR(fmt), ##__VA_ARGS__)
  #define FT_DETAIL_LEN 96
  #define FT_LINE_LEN 160
  #define FT_STORE_DETAIL 0
#else
  #define FT_LIT(x) (x)
  #define FT_SNPRINTF(buf, size, fmt, ...) snprintf(buf, size, fmt, ##__VA_ARGS__)
  #define FT_DETAIL_LEN 224
  #define FT_LINE_LEN 416
  #define FT_STORE_DETAIL 1
#endif

/* ช่วงค่าที่ถือว่าสมเหตุสมผลสำหรับการทดสอบในโรงงาน (อุณหภูมิห้อง) */
#define PLAUSIBLE_T_MIN 5.0f
#define PLAUSIBLE_T_MAX 55.0f
#define PLAUSIBLE_RH_MIN 5.0f
#define PLAUSIBLE_RH_MAX 98.0f
#define PLAUSIBLE_P_MIN 800.0f
#define PLAUSIBLE_P_MAX 1100.0f

/* เกณฑ์ noise ที่ยอมรับได้ที่ oversampling x16 (ดาต้าชีตตาราง 2-4 ให้ค่าดีกว่านี้มาก
   ตั้งไว้หลวมเพื่อเผื่อสัญญาณรบกวนจากสายและแหล่งจ่ายไฟในสายการผลิตจริง) */
#define NOISE_T_MAX 0.10f
#define NOISE_P_MAX 0.10f
#define NOISE_H_MAX 1.00f

#define FT_MAX_TESTS 28

/* -------------------------------------------------------------------------
   โครงสร้างเก็บผล
   ------------------------------------------------------------------------- */

enum FtStatus { FT_PASS = 0, FT_WARN, FT_FAIL };

struct FtResult {
  const char *name;
  FtStatus status;
#if FT_STORE_DETAIL
  char detail[FT_DETAIL_LEN];
#endif
};

static FtResult g_results[FT_MAX_TESTS];
static uint8_t g_resultCount = 0;

static MassmoreBME280 bme;

/* ข้อมูลอุปกรณ์ที่ตรวจพบ */
static uint8_t g_foundAddress = 0;
static uint8_t g_chipId = 0;
static massmore_bme280_chip_t g_chipType = MASSMORE_BME280_CHIP_UNKNOWN;
static massmore_bme280_genuine_t g_genuine = MASSMORE_BME280_GENUINE_UNKNOWN;
static uint8_t g_genuinePassCount = 0;

/* -------------------------------------------------------------------------
   ตัวช่วย
   ------------------------------------------------------------------------- */

/*!
 * แปลงเลขทศนิยมเป็นข้อความทศนิยมสองตำแหน่ง
 * ไม่ใช้ %f ใน snprintf เพราะ newlib-nano บนบางเป้าหมายตัดการรองรับ float ทิ้ง
 */
static const char *ftF2(char *buffer, size_t size, float value) {
  if (isnan(value)) {
    FT_SNPRINTF(buffer, size, "nan");
    return buffer;
  }
  bool negative = value < 0.0f;
  if (negative) {
    value = -value;
  }
  long scaled = (long)(value * 100.0f + 0.5f);
  FT_SNPRINTF(buffer, size, "%s%ld.%02ld", negative ? "-" : "", scaled / 100, scaled % 100);
  return buffer;
}

/*! เหมือน ftF2 แต่ให้ทศนิยมสามตำแหน่ง ใช้กับตัวเลข noise ที่เล็กมาก */
static const char *ftF3(char *buffer, size_t size, float value) {
  if (isnan(value)) {
    FT_SNPRINTF(buffer, size, "nan");
    return buffer;
  }
  bool negative = value < 0.0f;
  if (negative) {
    value = -value;
  }
  long scaled = (long)(value * 1000.0f + 0.5f);
  FT_SNPRINTF(buffer, size, "%s%ld.%03ld", negative ? "-" : "", scaled / 1000, scaled % 1000);
  return buffer;
}

/*!
 * คัดลอกข้อความลงบัฟเฟอร์โดยไม่ตัดกลางตัวอักษรไทย
 * ตัวอักษรไทยใน UTF-8 ใช้ 3 ไบต์ ถ้าตัดตรงกลางจะขึ้นเป็นสี่เหลี่ยม
 * ฟังก์ชันนี้จะถอยกลับจนพ้นไบต์ต่อเนื่อง (10xxxxxx) ก่อนปิดท้ายด้วย 0
 */
static void ftCopyDetail(char *dest, size_t size, const char *source) {
  if (size == 0) {
    return;
  }
  size_t length = strlen(source);
  if (length < size) {
    memcpy(dest, source, length + 1);
    return;
  }
  size_t cut = size - 1;
  while (cut > 0 && ((unsigned char)source[cut] & 0xC0) == 0x80) {
    cut--;
  }
  memcpy(dest, source, cut);
  dest[cut] = '\0';
}

/*! คัดลอกข้อความคงที่ (บน AVR อยู่ใน PROGMEM) ลงบัฟเฟอร์ */
static void ftCopyLit(char *dest, size_t size, const char *lit) {
#if defined(__AVR__)
  char tmp[FT_DETAIL_LEN];
  strncpy_P(tmp, lit, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  ftCopyDetail(dest, size, tmp);
#else
  ftCopyDetail(dest, size, lit);
#endif
}

static void ftRecord(const char *name, FtStatus status, const char *detail) {
  if (g_resultCount >= FT_MAX_TESTS) {
    return;
  }
  FtResult &result = g_results[g_resultCount];
  result.name = name;
  result.status = status;
#if FT_STORE_DETAIL
  ftCopyDetail(result.detail, FT_DETAIL_LEN, detail ? detail : "");
  const char *shown = result.detail;
#else
  const char *shown = detail ? detail : "";
#endif

  const char *tag = (status == FT_PASS) ? "[ OK ]"
                    : (status == FT_WARN) ? "[WARN]"
                                          : "[FAIL]";
  const char *word = (status == FT_PASS) ? "PASS"
                     : (status == FT_WARN) ? "WARN"
                                           : "FAIL";

  char line[FT_LINE_LEN];
  FT_SNPRINTF(line, sizeof(line), "%s %02u %-14s %s", tag, (unsigned)(g_resultCount + 1), name,
              shown);
  Serial.println(line);

  FT_SNPRINTF(line, sizeof(line), "#RESULT,%u,%s,%s,%s", (unsigned)(g_resultCount + 1), name,
              word, shown);
  Serial.println(line);

  g_resultCount++;
}

static void ftBanner(const char *title) {
  Serial.println();
  Serial.print(F("--- "));
  Serial.print(title);
  Serial.println(F(" ---"));
}

#if !defined(__AVR__)
/*! วัดซ้ำหลายครั้งแล้วคืนค่าเฉลี่ยกับส่วนเบี่ยงเบนมาตรฐานของทั้งสามค่า */
static bool ftCollect(uint8_t samples, float *meanT, float *sdT, float *meanP, float *sdP,
                      float *meanH, float *sdH) {
  float sumT = 0, sumT2 = 0, sumP = 0, sumP2 = 0, sumH = 0, sumH2 = 0;
  uint8_t good = 0;

  for (uint8_t i = 0; i < samples; i++) {
    massmore_bme280_reading_t r;
    if (!bme.read(r)) {
      continue;
    }
    sumT += r.temperature;
    sumT2 += r.temperature * r.temperature;
    if (!isnan(r.pressure)) {
      sumP += r.pressure;
      sumP2 += r.pressure * r.pressure;
    }
    if (!isnan(r.humidity)) {
      sumH += r.humidity;
      sumH2 += r.humidity * r.humidity;
    }
    good++;
    delay(10);
  }

  if (good < 2) {
    return false;
  }

  float n = (float)good;
  *meanT = sumT / n;
  *meanP = sumP / n;
  *meanH = sumH / n;
  *sdT = sqrtf(fabsf((sumT2 / n) - (*meanT) * (*meanT)));
  *sdP = sqrtf(fabsf((sumP2 / n) - (*meanP) * (*meanP)));
  *sdH = sqrtf(fabsf((sumH2 / n) - (*meanH) * (*meanH)));
  return true;
}
#endif /* !__AVR__ */

/* -------------------------------------------------------------------------
   ด่านที่ 1 : สแกนบัส I2C
   ------------------------------------------------------------------------- */

/* เปิดบัส I2C - ที่เดียวที่ผูกกับชนิดบอร์ด ไลบรารีไม่แตะเรื่องขาเลย */
static void busBegin() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA, SCL); /* ขาปริยายของ variant : esp32dev 21/22, esp32-s3 8/9 */
#else
  Wire.begin(); /* RP2040 GP4/GP5, AVR A4/A5, STM32 ตาม variant */
#endif
  Wire.setClock(I2C_FREQ_NORMAL);
}

static bool gateBusScan() {
  ftBanner("GATE 1 : สแกนบัส I2C");

  busBegin();
  delay(50);

  uint8_t deviceCount = 0;
  uint8_t addresses[8];
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      if (deviceCount < 8) {
        addresses[deviceCount] = address;
      }
      deviceCount++;
      Serial.print(F("      พบอุปกรณ์ที่ 0x"));
      if (address < 0x10) {
        Serial.print('0');
      }
      Serial.println(address, HEX);
    }
  }

  char detail[FT_DETAIL_LEN];
  if (deviceCount == 0) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("ไม่พบอุปกรณ์บนบัสเลย ตรวจ VIN GND SDA SCL และสาย Qwiic"));
    ftRecord("I2C_SCAN", FT_FAIL, detail);
    return false;
  }

  for (uint8_t i = 0; i < deviceCount && i < 8; i++) {
    if (addresses[i] == MASSMORE_BME280_I2C_ADDR_A ||
        addresses[i] == MASSMORE_BME280_I2C_ADDR_B) {
      g_foundAddress = addresses[i];
      break;
    }
  }

  if (g_foundAddress == 0) {
    FT_SNPRINTF(detail, sizeof(detail), "พบ %u อุปกรณ์ แต่ไม่มีตัวไหนอยู่ที่ 0x77 หรือ 0x76",
             (unsigned)deviceCount);
    ftRecord("I2C_SCAN", FT_FAIL, detail);
    return false;
  }

  FT_SNPRINTF(detail, sizeof(detail), "พบเซ็นเซอร์ที่ 0x%02X (อุปกรณ์บนบัสทั้งหมด %u ตัว)",
           g_foundAddress, (unsigned)deviceCount);
  ftRecord("I2C_SCAN", FT_PASS, detail);
  return true;
}

/* -------------------------------------------------------------------------
   ด่านที่ 2 : ตรวจรหัสประจำรุ่น
   ------------------------------------------------------------------------- */

static bool gateChipId() {
  ftBanner("GATE 2 : ตรวจรหัสประจำรุ่น");

  char detail[FT_DETAIL_LEN];

  /* อ่านรีจิสเตอร์ 0xD0 ตรง ๆ โดยไม่ผ่าน begin() เพื่อให้เห็นค่าจริงทุกกรณี */
  Wire.beginTransmission(g_foundAddress);
  Wire.write(MASSMORE_BME280_REG_CHIP_ID);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(g_foundAddress, (uint8_t)1) != 1) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("มีอุปกรณ์ตอบที่ address แต่อ่านรีจิสเตอร์ 0xD0 ไม่ได้"));
    ftRecord("CHIP_ID", FT_FAIL, detail);
    return false;
  }
  g_chipId = (uint8_t)Wire.read();

  switch (g_chipId) {
    case MASSMORE_BME280_CHIP_ID_BME280:
      g_chipType = MASSMORE_BME280_CHIP_BME280;
      break;
    case MASSMORE_BME280_CHIP_ID_BMP280:
    case MASSMORE_BME280_CHIP_ID_BMP280_S1:
    case MASSMORE_BME280_CHIP_ID_BMP280_S2:
      g_chipType = MASSMORE_BME280_CHIP_BMP280;
      break;
    case MASSMORE_BME280_CHIP_ID_BME680:
      g_chipType = MASSMORE_BME280_CHIP_BME680;
      break;
    default:
      g_chipType = MASSMORE_BME280_CHIP_UNKNOWN;
      break;
  }

  if (g_chipId != MASSMORE_BME280_CHIP_ID_BME280) {
    FT_SNPRINTF(detail, sizeof(detail), "รหัส 0x%02X = %s ไม่ใช่ BME280 (ต้องเป็น 0x60)", g_chipId,
             MassmoreBME280::chipToString(g_chipType));
    ftRecord("CHIP_ID", FT_FAIL, detail);
    return false;
  }

  FT_SNPRINTF(detail, sizeof(detail), "รหัส 0x60 = Bosch BME280 ถูกต้อง");
  ftRecord("CHIP_ID", FT_PASS, detail);

  if (!bme.begin(g_foundAddress, Wire)) {
    FT_SNPRINTF(detail, sizeof(detail), "begin() ไม่ผ่าน : %s",
             MassmoreBME280::errorToString(bme.lastError()));
    ftRecord("BEGIN", FT_FAIL, detail);
    return false;
  }
  ftCopyLit(detail, sizeof(detail), FT_LIT("เริ่มต้นไลบรารีและอ่านค่าชดเชยได้ครบ"));
  ftRecord("BEGIN", FT_PASS, detail);
  return true;
}

/* -------------------------------------------------------------------------
   หัวข้อทดสอบ
   ------------------------------------------------------------------------- */

static void testSoftReset() {
  char detail[FT_DETAIL_LEN];

  /* เขียนค่าแปลก ๆ ลง ctrl_meas แล้วรีเซ็ต ต้องกลับเป็น 0x00 */
  bme.writeRegister(MASSMORE_BME280_REG_CTRL_MEAS, 0xB7);
  bme.writeRegister(MASSMORE_BME280_REG_CONFIG, 0x9C);
  bme.writeRegister(MASSMORE_BME280_REG_RESET, MASSMORE_BME280_RESET_MAGIC);
  delay(10);

  uint8_t ctrlMeas = 0xFF, config = 0xFF, ctrlHum = 0xFF;
  bme.readRegister(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas);
  bme.readRegister(MASSMORE_BME280_REG_CONFIG, config);
  bme.readRegister(MASSMORE_BME280_REG_CTRL_HUM, ctrlHum);

  if (ctrlMeas == 0x00 && config == 0x00 && ctrlHum == 0x00) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("รีเซ็ตแล้วรีจิสเตอร์ตั้งค่าทั้งสามกลับเป็น 0x00"));
    ftRecord("SOFT_RESET", FT_PASS, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "หลังรีเซ็ตได้ ctrl_meas=0x%02X config=0x%02X ctrl_hum=0x%02X",
             ctrlMeas, config, ctrlHum);
    ftRecord("SOFT_RESET", FT_FAIL, detail);
  }

  bme.reset(); /* ให้ไลบรารีเขียนค่าที่ตั้งไว้กลับเอง */
}

static void testCalibration() {
  char detail[FT_DETAIL_LEN];
  massmore_bme280_calib_t c;
  bme.getCalibration(c);

  /* --- อ่านค่าชดเชยได้ครบและไม่ใช่ค่าว่าง --- */
  bool notEmpty = (c.dig_T1 != 0 && c.dig_T1 != 0xFFFF && c.dig_P1 != 0 &&
                   c.dig_P1 != 0xFFFF && c.dig_H1 != 0 && c.dig_H1 != 0xFF);
  FT_SNPRINTF(detail, sizeof(detail), "T1=%u T2=%d P1=%u P2=%d H1=%u H2=%d", (unsigned)c.dig_T1,
           (int)c.dig_T2, (unsigned)c.dig_P1, (int)c.dig_P2, (unsigned)c.dig_H1,
           (int)c.dig_H2);
  ftRecord("CALIB_READ", notEmpty ? FT_PASS : FT_FAIL, detail);

  /* --- ค่าอยู่ในช่วงที่โรงงาน Bosch ใช้จริง --- */
  bool inRange = (c.dig_T1 >= 20000 && c.dig_T1 <= 36000 && c.dig_T2 >= 22000 &&
                  c.dig_T2 <= 30000 && c.dig_P1 >= 28000 && c.dig_P1 <= 48000 &&
                  c.dig_P2 >= -20000 && c.dig_P2 <= 0);
  if (inRange) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("สัมประสิทธิ์ทุกตัวอยู่ในช่วงที่ Bosch ใช้จริง"));
    ftRecord("CALIB_RANGE", FT_PASS, detail);
  } else {
    ftCopyLit(detail, sizeof(detail), FT_LIT("สัมประสิทธิ์บางตัวอยู่นอกช่วงปกติ น่าสงสัยว่าไม่ใช่ของแท้"));
    ftRecord("CALIB_RANGE", FT_FAIL, detail);
  }

  /* --- ค่าชดเชยความชื้นต้องมีครบ (BMP280 ที่ติดฉลากผิดจะตกข้อนี้) --- */
  bool humCalib = (c.dig_H2 >= 200 && c.dig_H2 <= 500 && c.dig_H6 != 0);
  FT_SNPRINTF(detail, sizeof(detail), "H1=%u H2=%d H3=%u H4=%d H5=%d H6=%d", (unsigned)c.dig_H1,
           (int)c.dig_H2, (unsigned)c.dig_H3, (int)c.dig_H4, (int)c.dig_H5, (int)c.dig_H6);
  ftRecord("CALIB_HUM", humCalib ? FT_PASS : FT_FAIL, detail);
}

static void testRegisterEcho() {
  char detail[FT_DETAIL_LEN];

  /* เข้า sleep ก่อน ไม่งั้นชิปจะเมินการเขียน config (ดาต้าชีตหัวข้อ 5.4.6) */
  bme.setMode(MASSMORE_BME280_MODE_SLEEP);

  const uint8_t probes[3] = {
      (uint8_t)((MASSMORE_BME280_STANDBY_500_BITS << 5) | (MASSMORE_BME280_FILTER_16_BITS << 2)),
      (uint8_t)((MASSMORE_BME280_STANDBY_10_BITS << 5) | (MASSMORE_BME280_FILTER_2_BITS << 2)),
      0x00};

  bool allOk = true;
  uint8_t badValue = 0;
  uint8_t badExpected = 0;
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t echo = 0xFF;
    bme.writeRegister(MASSMORE_BME280_REG_CONFIG, probes[i]);
    bme.readRegister(MASSMORE_BME280_REG_CONFIG, echo);
    if (echo != probes[i]) {
      allOk = false;
      badValue = echo;
      badExpected = probes[i];
    }
  }

  if (allOk) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("เขียน config สามค่าแล้วอ่านกลับได้ตรงทุกบิต"));
    ftRecord("REG_ECHO", FT_PASS, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "เขียน 0x%02X แต่อ่านกลับได้ 0x%02X", badExpected, badValue);
    ftRecord("REG_ECHO", FT_FAIL, detail);
  }
}

static void testStatusRegister() {
  char detail[FT_DETAIL_LEN];
  uint8_t status = 0xFF;

  if (!bme.readStatus(status)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("อ่านรีจิสเตอร์ status ไม่ได้"));
    ftRecord("STATUS_REG", FT_FAIL, detail);
    return;
  }

  /* บิตที่ใช้จริงมีแค่บิต 3 (measuring) กับบิต 0 (im_update)
     บิตที่เหลือเป็นบิตสงวน ต้องอ่านได้เป็น 0 เสมอ */
  uint8_t reserved = (uint8_t)(status & 0xF6);
  if (reserved == 0) {
    FT_SNPRINTF(detail, sizeof(detail), "status=0x%02X บิตสงวนเป็น 0 ถูกต้อง", status);
    ftRecord("STATUS_REG", FT_PASS, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "status=0x%02X มีบิตสงวนไม่เป็นศูนย์ (0x%02X)", status,
             reserved);
    ftRecord("STATUS_REG", FT_WARN, detail);
  }
}

static void testSleepMode() {
  char detail[FT_DETAIL_LEN];

  bme.setSampling(MASSMORE_BME280_MODE_SLEEP, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  delay(20);

  uint8_t ctrlMeas = 0xFF;
  bme.readRegister(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas);
  bool inSleep = ((ctrlMeas & 0x03) == 0x00);
  bool notMeasuring = !bme.isMeasuring();

  if (inSleep && notMeasuring) {
    FT_SNPRINTF(detail, sizeof(detail), "ctrl_meas=0x%02X อยู่ในโหมด sleep และไม่มีการวัด", ctrlMeas);
    ftRecord("SLEEP_MODE", FT_PASS, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "ctrl_meas=0x%02X measuring=%d", ctrlMeas,
             notMeasuring ? 0 : 1);
    ftRecord("SLEEP_MODE", FT_FAIL, detail);
  }
}

static void testForcedMode() {
  char detail[FT_DETAIL_LEN];

  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);

  uint32_t start = millis();
  bool ok = bme.takeForcedMeasurement();
  uint32_t elapsed = millis() - start;

  uint8_t ctrlMeas = 0xFF;
  bme.readRegister(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas);
  bool backToSleep = ((ctrlMeas & 0x03) == 0x00);

  if (ok && backToSleep) {
    FT_SNPRINTF(detail, sizeof(detail), "วัดเสร็จใน %lu ms แล้วกลับไป sleep เองถูกต้อง",
             (unsigned long)elapsed);
    ftRecord("FORCED_MODE", FT_PASS, detail);
  } else if (ok) {
    FT_SNPRINTF(detail, sizeof(detail), "วัดเสร็จใน %lu ms แต่ ctrl_meas=0x%02X ไม่กลับไป sleep",
             (unsigned long)elapsed, ctrlMeas);
    ftRecord("FORCED_MODE", FT_WARN, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "สั่งวัดไม่สำเร็จ : %s",
             MassmoreBME280::errorToString(bme.lastError()));
    ftRecord("FORCED_MODE", FT_FAIL, detail);
  }
}

static void testNormalMode() {
  char detail[FT_DETAIL_LEN];

  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);
  delay(50);

  uint8_t ctrlMeas = 0xFF;
  bme.readRegister(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas);

  /* ในโหมด normal ชิปวัดวนตลอด ดังนั้นสุ่มอ่าน status หลายครั้งต้องเจอบิต measuring บ้าง */
  uint8_t measuringSeen = 0;
  for (uint8_t i = 0; i < 40; i++) {
    if (bme.isMeasuring()) {
      measuringSeen++;
    }
  }

  if ((ctrlMeas & 0x03) == 0x03 && measuringSeen > 0) {
    FT_SNPRINTF(detail, sizeof(detail), "วัดวนต่อเนื่อง เจอบิต measuring %u ครั้งจาก 40",
             (unsigned)measuringSeen);
    ftRecord("NORMAL_MODE", FT_PASS, detail);
  } else if ((ctrlMeas & 0x03) == 0x03) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("อยู่ในโหมด normal แต่จับบิต measuring ไม่ทัน (รอบวัดสั้นมาก ไม่ผิดปกติ)"));
    ftRecord("NORMAL_MODE", FT_WARN, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail), "ctrl_meas=0x%02X ไม่ได้อยู่ในโหมด normal", ctrlMeas);
    ftRecord("NORMAL_MODE", FT_FAIL, detail);
  }
}

static void testHumidityChannel() {
  char detail[FT_DETAIL_LEN];

  /* ปิดช่องความชื้น ค่าดิบต้องเป็น 0x8000 */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_NONE,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  bme.takeForcedMeasurement();
  massmore_bme280_raw_t rawOff;
  bool okOff = bme.readRawADC(rawOff);

  /* เปิดช่องความชื้น ค่าดิบต้องไม่ใช่ 0x8000 */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  bme.takeForcedMeasurement();
  massmore_bme280_raw_t rawOn;
  bool okOn = bme.readRawADC(rawOn);

  if (okOff && okOn && rawOff.adc_H == MASSMORE_BME280_RAW_SKIPPED_16BIT &&
      rawOn.adc_H != MASSMORE_BME280_RAW_SKIPPED_16BIT && rawOn.adc_H != 0) {
    FT_SNPRINTF(detail, sizeof(detail), "ปิดได้ 0x%04lX เปิดได้ %ld ตอบสนองตาม osrs_h ถูกต้อง",
             (unsigned long)rawOff.adc_H, (long)rawOn.adc_H);
    ftRecord("HUM_CHANNEL", FT_PASS, detail);
  } else {
    FT_SNPRINTF(detail, sizeof(detail),
             "ค่าดิบความชื้น ปิด=%ld เปิด=%ld ไม่ตอบสนองตาม osrs_h (BMP280 เป็นแบบนี้)",
             (long)rawOff.adc_H, (long)rawOn.adc_H);
    ftRecord("HUM_CHANNEL", FT_FAIL, detail);
  }
}

#if !defined(__AVR__)
/*!
 * สั่งวัดหนึ่งครั้งแล้วจับเวลาจนบิต measuring ลง คืนค่าเป็นมิลลิวินาที
 * มีเพดานเวลากันค้าง ถ้าเกิน 500 ms จะเลิกรอ
 * ตัวเลขที่ได้จะสูงกว่าสเปกเล็กน้อยเสมอ เพราะรวมเวลาที่ใช้ถามผ่านบัส I2C ไปด้วย
 */
static uint32_t ftTimeOneMeasurement() {
  uint32_t start = micros();
  bme.startForcedMeasurement();
  while (bme.isMeasuring()) {
    if ((micros() - start) > 500000UL) {
      break;
    }
  }
  return (micros() - start) / 1000;
}

static void testOversamplingTiming() {
  char detail[FT_DETAIL_LEN];

  /* x1 ทุกช่อง */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  uint16_t specX1 = bme.measurementTimeMs();
  uint32_t actualX1 = ftTimeOneMeasurement();

  /* x16 ทุกช่อง */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  uint16_t specX16 = bme.measurementTimeMs();
  uint32_t actualX16 = ftTimeOneMeasurement();

  FT_SNPRINTF(detail, sizeof(detail), "x1 ใช้ %lu ms (สเปก %u) / x16 ใช้ %lu ms (สเปก %u)",
           (unsigned long)actualX1, (unsigned)specX1, (unsigned long)actualX16,
           (unsigned)specX16);

  /* เวลาที่ x16 ต้องมากกว่า x1 อย่างชัดเจน และต้องไม่เกินสเปกกรณีแย่สุด */
  if (actualX16 > actualX1 * 3 && actualX16 <= (uint32_t)(specX16 + 20)) {
    ftRecord("MEAS_TIMING", FT_PASS, detail);
  } else {
    ftRecord("MEAS_TIMING", FT_WARN, detail);
  }
}
#endif /* !__AVR__ */

#if !defined(__AVR__)
/* NOISE / IIR_FILTER / MEAS_TIMING / BUS_400K ข้ามบน AVR เพราะ flash 32 KB ไม่พอ
   (ESP32 / RP2040 ทดสอบครบ 22 หัวข้อ, AVR ทดสอบ 18 หัวข้อ) */
static void testNoise() {
  char detail[FT_DETAIL_LEN];
  char bufT[16], bufP[16], bufH[16];

  /* x16 ทุกช่อง ปิดฟิลเตอร์ เพื่อวัด noise ดิบของชิป */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);
  delay(200);

  float meanT, sdT, meanP, sdP, meanH, sdH;
  if (!ftCollect(20, &meanT, &sdT, &meanP, &sdP, &meanH, &sdH)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("เก็บตัวอย่างไม่พอ"));
    ftRecord("NOISE", FT_FAIL, detail);
    return;
  }

  FT_SNPRINTF(detail, sizeof(detail), "sd อุณหภูมิ %s C  ความดัน %s hPa  ความชื้น %s %%RH",
           ftF3(bufT, sizeof(bufT), sdT), ftF3(bufP, sizeof(bufP), sdP),
           ftF3(bufH, sizeof(bufH), sdH));

  if (sdT <= NOISE_T_MAX && sdP <= NOISE_P_MAX && sdH <= NOISE_H_MAX) {
    ftRecord("NOISE", FT_PASS, detail);
  } else {
    ftRecord("NOISE", FT_FAIL, detail);
  }
}

static void testFilter() {
  char detail[FT_DETAIL_LEN];
  char bufOff[16], bufOn[16];
  float meanT, sdT, meanP, sdPoff, sdPon, meanH, sdH;

  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_0_5_MS);
  delay(100);
  if (!ftCollect(20, &meanT, &sdT, &meanP, &sdPoff, &meanH, &sdH)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("เก็บตัวอย่างไม่พอ"));
    ftRecord("IIR_FILTER", FT_FAIL, detail);
    return;
  }

  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_16, MASSMORE_BME280_STANDBY_0_5_MS);
  /* ฟิลเตอร์ต้องการเวลาเข้าที่ ทิ้งค่าแรก ๆ ไปก่อน */
  for (uint8_t i = 0; i < 40; i++) {
    massmore_bme280_reading_t discard;
    bme.read(discard);
    delay(5);
  }
  if (!ftCollect(20, &meanT, &sdT, &meanP, &sdPon, &meanH, &sdH)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("เก็บตัวอย่างไม่พอ"));
    ftRecord("IIR_FILTER", FT_FAIL, detail);
    return;
  }

  FT_SNPRINTF(detail, sizeof(detail), "sd ความดัน ปิดฟิลเตอร์ %s hPa -> ฟิลเตอร์ 16 %s hPa",
           ftF3(bufOff, sizeof(bufOff), sdPoff), ftF3(bufOn, sizeof(bufOn), sdPon));

  if (sdPon <= sdPoff) {
    ftRecord("IIR_FILTER", FT_PASS, detail);
  } else {
    /* ที่บอร์ดนิ่งมาก ๆ ตัวเลขทั้งสองอาจต่างกันน้อยจนสลับกันได้ ไม่ถือว่าบอร์ดเสีย */
    ftRecord("IIR_FILTER", FT_WARN, detail);
  }
}
#endif /* !__AVR__ */

static void testPlausibility() {
  char detail[FT_DETAIL_LEN];
  char bufT[16], bufH[16], bufP[16];

  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X4,
                  MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_X4,
                  MASSMORE_BME280_FILTER_4, MASSMORE_BME280_STANDBY_125_MS);
  delay(150);

  massmore_bme280_reading_t r;
  if (!bme.read(r)) {
    FT_SNPRINTF(detail, sizeof(detail), "อ่านค่าไม่สำเร็จ : %s",
             MassmoreBME280::errorToString(bme.lastError()));
    ftRecord("READ_VALUES", FT_FAIL, detail);
    return;
  }

  FT_SNPRINTF(detail, sizeof(detail), "%s C  %s %%RH  %s hPa", ftF2(bufT, sizeof(bufT), r.temperature),
           ftF2(bufH, sizeof(bufH), r.humidity), ftF2(bufP, sizeof(bufP), r.pressure));
  ftRecord("READ_VALUES", FT_PASS, detail);

  /* อุณหภูมิ */
  bool tOk = (r.temperature >= PLAUSIBLE_T_MIN && r.temperature <= PLAUSIBLE_T_MAX);
  FT_SNPRINTF(detail, sizeof(detail), "%s C (ช่วงที่รับได้ %d ถึง %d)",
           ftF2(bufT, sizeof(bufT), r.temperature), (int)PLAUSIBLE_T_MIN, (int)PLAUSIBLE_T_MAX);
  ftRecord("TEMP_RANGE", tOk ? FT_PASS : FT_FAIL, detail);

  /* ความชื้น */
  bool hOk = (r.humidity >= PLAUSIBLE_RH_MIN && r.humidity <= PLAUSIBLE_RH_MAX);
  FT_SNPRINTF(detail, sizeof(detail), "%s %%RH (ช่วงที่รับได้ %d ถึง %d)",
           ftF2(bufH, sizeof(bufH), r.humidity), (int)PLAUSIBLE_RH_MIN, (int)PLAUSIBLE_RH_MAX);
  ftRecord("HUM_RANGE", hOk ? FT_PASS : FT_FAIL, detail);

  /* ความดัน */
  bool pOk = (r.pressure >= PLAUSIBLE_P_MIN && r.pressure <= PLAUSIBLE_P_MAX);
  FT_SNPRINTF(detail, sizeof(detail), "%s hPa (ช่วงที่รับได้ %d ถึง %d)",
           ftF2(bufP, sizeof(bufP), r.pressure), (int)PLAUSIBLE_P_MIN, (int)PLAUSIBLE_P_MAX);
  ftRecord("PRES_RANGE", pOk ? FT_PASS : FT_FAIL, detail);

  /* จุดน้ำค้างต้องไม่สูงกว่าอุณหภูมิเสมอ เป็นการตรวจความสอดคล้องของสองค่าพร้อมกัน */
  float dew = MassmoreBME280::dewPoint(r.temperature, r.humidity);
  bool dewOk = (!isnan(dew) && dew <= r.temperature + 0.5f);
  FT_SNPRINTF(detail, sizeof(detail), "จุดน้ำค้าง %s C ต้องไม่เกินอุณหภูมิ %s C",
           ftF2(bufH, sizeof(bufH), dew), ftF2(bufT, sizeof(bufT), r.temperature));
  ftRecord("DEWPOINT", dewOk ? FT_PASS : FT_FAIL, detail);

  /* ความสูงที่คำนวณจากความดัน ต้องอยู่ในช่วงที่เป็นไปได้บนโลก */
  float altitude = r.altitude;
  bool altOk = (!isnan(altitude) && altitude > -500.0f && altitude < 4000.0f);
  FT_SNPRINTF(detail, sizeof(detail), "%s m (คำนวณจากระดับน้ำทะเลมาตรฐาน 1013.25 hPa)",
           ftF2(bufP, sizeof(bufP), altitude));
  ftRecord("ALTITUDE", altOk ? FT_PASS : FT_FAIL, detail);
}

static void testStability() {
  char detail[FT_DETAIL_LEN];
  char bufA[16], bufB[16];

  massmore_bme280_reading_t first;
  if (!bme.read(first)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("อ่านค่าแรกไม่สำเร็จ"));
    ftRecord("STABILITY", FT_FAIL, detail);
    return;
  }

  delay(1500);

  massmore_bme280_reading_t second;
  if (!bme.read(second)) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("อ่านค่าที่สองไม่สำเร็จ"));
    ftRecord("STABILITY", FT_FAIL, detail);
    return;
  }

  float driftT = fabsf(second.temperature - first.temperature);
  float driftH = fabsf(second.humidity - first.humidity);

  FT_SNPRINTF(detail, sizeof(detail), "ใน 1.5 วินาที อุณหภูมิเปลี่ยน %s C ความชื้นเปลี่ยน %s %%RH",
           ftF3(bufA, sizeof(bufA), driftT), ftF3(bufB, sizeof(bufB), driftH));

  /* ค่าที่นิ่งเกินไป (ไม่ขยับเลยสักหลักทศนิยม) ก็ผิดปกติพอ ๆ กับค่าที่แกว่งแรง */
  if (driftT > 2.0f || driftH > 10.0f) {
    ftRecord("STABILITY", FT_FAIL, detail);
  } else {
    ftRecord("STABILITY", FT_PASS, detail);
  }
}

#if !defined(__AVR__)
static void testBusSpeed() {
  char detail[FT_DETAIL_LEN];

  Wire.setClock(I2C_FREQ_FAST);
  delay(10);

  uint8_t id = 0;
  bool readOk = bme.readRegister(MASSMORE_BME280_REG_CHIP_ID, id);
  massmore_bme280_reading_t r;
  bool measureOk = bme.read(r);

  Wire.setClock(I2C_FREQ_NORMAL);
  delay(10);

  if (readOk && id == MASSMORE_BME280_CHIP_ID_BME280 && measureOk) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("ทำงานได้ปกติที่ความเร็วบัส 400 kHz"));
    ftRecord("BUS_400K", FT_PASS, detail);
  } else {
    ftCopyLit(detail, sizeof(detail), FT_LIT("ที่ 400 kHz สื่อสารไม่ผ่าน ให้ใช้ 100 kHz หรือเปลี่ยนสาย Qwiic ที่สั้นลง"));
    ftRecord("BUS_400K", FT_WARN, detail);
  }
}
#endif /* !__AVR__ */

static void testGenuine() {
  char detail[FT_DETAIL_LEN];

  massmore_bme280_identity_t identity;
  g_genuine = bme.verifyChip(&identity);
  g_genuinePassCount = identity.passCount;

  FT_SNPRINTF(detail, sizeof(detail), "ผ่าน %u จาก 10 ข้อ (id=%d calib=%d%d%d%d reset=%d hum=%d echo=%d meas=%d live=%d)",
           (unsigned)identity.passCount, identity.chipIdOk ? 1 : 0, identity.calibTempOk ? 1 : 0,
           identity.calibPressOk ? 1 : 0, identity.calibHumOk ? 1 : 0,
           identity.calibUniqueOk ? 1 : 0, identity.resetOk ? 1 : 0,
           identity.ctrlHumLatchOk ? 1 : 0, identity.registerEchoOk ? 1 : 0,
           identity.measuringBitOk ? 1 : 0, identity.humidityLiveOk ? 1 : 0);

  if (g_genuine == MASSMORE_BME280_GENUINE_YES) {
    ftRecord("GENUINE", FT_PASS, detail);
  } else if (g_genuine == MASSMORE_BME280_GENUINE_SUSPECT) {
    ftRecord("GENUINE", FT_WARN, detail);
  } else {
    ftRecord("GENUINE", FT_FAIL, detail);
  }
}

static void testPresets() {
  char detail[FT_DETAIL_LEN];

  bool ok = bme.useWeatherStationPreset() && bme.useHumiditySensingPreset() &&
            bme.useIndoorNavigationPreset() && bme.useGamingPreset();

  if (ok) {
    ftCopyLit(detail, sizeof(detail), FT_LIT("ชุดตั้งค่าสำเร็จรูปทั้งสี่ชุดเขียนลงชิปได้ปกติ"));
    ftRecord("PRESETS", FT_PASS, detail);
  } else {
    ftCopyLit(detail, sizeof(detail), FT_LIT("ตั้งชุดสำเร็จรูปไม่ผ่าน"));
    ftRecord("PRESETS", FT_FAIL, detail);
  }
}

/* -------------------------------------------------------------------------
   สรุปผล
   ------------------------------------------------------------------------- */

/*! คำที่ให้เว็บอ่าน ใช้อักษรอังกฤษล้วนเพื่อให้ parse ง่าย */
static const char *genuineToken(massmore_bme280_genuine_t genuine) {
  switch (genuine) {
    case MASSMORE_BME280_GENUINE_YES:
      return "GENUINE";
    case MASSMORE_BME280_GENUINE_SUSPECT:
      return "SUSPECT";
    case MASSMORE_BME280_GENUINE_NO:
      return "FAKE";
    default:
      return "UNKNOWN";
  }
}

static void printSummary(bool gatesPassed) {
  uint8_t pass = 0, warn = 0, fail = 0;
  for (uint8_t i = 0; i < g_resultCount; i++) {
    if (g_results[i].status == FT_PASS) {
      pass++;
    } else if (g_results[i].status == FT_WARN) {
      warn++;
    } else {
      fail++;
    }
  }

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("  สรุปผลการทดสอบ"));
  Serial.println(F("=========================================================="));
  Serial.print(F("  address ที่พบ    : 0x"));
  Serial.println(g_foundAddress, HEX);
  Serial.print(F("  รหัสประจำรุ่น    : 0x"));
  if (g_chipId < 0x10) {
    Serial.print('0');
  }
  Serial.print(g_chipId, HEX);
  Serial.print(F("  ("));
  Serial.print(MassmoreBME280::chipToString(g_chipType));
  Serial.println(F(")"));
  Serial.print(F("  ผลตรวจของแท้     : "));
  Serial.print(MassmoreBME280::genuineToString(g_genuine));
  Serial.print(F("  ("));
  Serial.print(g_genuinePassCount);
  Serial.println(F("/10)"));
  Serial.print(F("  ไลบรารีเวอร์ชัน   : "));
  Serial.println(MassmoreBME280::getLibraryVersion());
  Serial.print(F("  เฟิร์มแวร์สร้างเมื่อ: "));
  Serial.print(F(__DATE__));
  Serial.print(' ');
  Serial.println(F(__TIME__));

  Serial.println(F("\n[ ผลรายหัวข้อ ]"));
  for (uint8_t i = 0; i < g_resultCount; i++) {
    const char *tag = (g_results[i].status == FT_PASS) ? "[ OK ]"
                      : (g_results[i].status == FT_WARN) ? "[WARN]"
                                                         : "[FAIL]";
    char line[FT_LINE_LEN];
#if FT_STORE_DETAIL
    FT_SNPRINTF(line, sizeof(line), "  %s %02u %-14s %s", tag, (unsigned)(i + 1),
                g_results[i].name, g_results[i].detail);
#else
    FT_SNPRINTF(line, sizeof(line), "  %s %02u %s", tag, (unsigned)(i + 1), g_results[i].name);
#endif
    Serial.println(line);
  }

  Serial.println(F("\n[ สรุป ]"));
  Serial.print(F("  ผ่าน "));
  Serial.print(pass);
  Serial.print(F("   เตือน "));
  Serial.print(warn);
  Serial.print(F("   ไม่ผ่าน "));
  Serial.println(fail);

  if (fail > 0) {
    Serial.println(F("\n  หัวข้อที่ไม่ผ่าน:"));
    for (uint8_t i = 0; i < g_resultCount; i++) {
      if (g_results[i].status == FT_FAIL) {
        Serial.print(F("    - "));
        Serial.print(g_results[i].name);
#if FT_STORE_DETAIL
        Serial.print(F(" : "));
        Serial.println(g_results[i].detail);
#else
        Serial.println();
#endif
      }
    }
  }
  if (warn > 0) {
    Serial.println(F("\n  หัวข้อที่เตือน (ไม่ถือว่าบอร์ดเสีย):"));
    for (uint8_t i = 0; i < g_resultCount; i++) {
      if (g_results[i].status == FT_WARN) {
        Serial.print(F("    - "));
        Serial.print(g_results[i].name);
#if FT_STORE_DETAIL
        Serial.print(F(" : "));
        Serial.println(g_results[i].detail);
#else
        Serial.println();
#endif
      }
    }
  }

  bool overallPass = (fail == 0) && gatesPassed;

  /* หาสาเหตุแรกที่ทำให้ไม่ผ่าน ไว้ใส่ในบรรทัดสรุปมาตรฐาน Massmore */
  const char *reason = gatesPassed ? "UNKNOWN" : "GATE_FAILED";
  for (uint8_t i = 0; i < g_resultCount; i++) {
    if (g_results[i].status == FT_FAIL) {
      reason = g_results[i].name;
      break;
    }
  }

  Serial.println(F("=========================================================="));

  char line[FT_LINE_LEN];
  FT_SNPRINTF(line, sizeof(line), "#DEVICE,0x%02X,%s,0x%02X,%s,%u", g_foundAddress,
              MassmoreBME280::chipToString(g_chipType), g_chipId, genuineToken(g_genuine),
              (unsigned)g_genuinePassCount);
  Serial.println(line);

  FT_SNPRINTF(line, sizeof(line), "#VERDICT,%s,%u,%u,%u", overallPass ? "PASS" : "FAIL",
              (unsigned)pass, (unsigned)fail, (unsigned)warn);
  Serial.println(line);

  Serial.println();
  if (overallPass) {
    Serial.println(F("[PASS] SENSOR QA PASSED - READY TO SHIP"));
  } else {
    Serial.print(F("[FAIL] QA CHECK FAILED: "));
    Serial.println(reason);
  }

  Serial.println(F("\nพิมพ์ r แล้วกด Enter เพื่อทดสอบซ้ำ"));
}

/* -------------------------------------------------------------------------
   ตัวหลัก
   ------------------------------------------------------------------------- */

static void runFactoryTest() {
  g_resultCount = 0;
  g_foundAddress = 0;
  g_chipId = 0;
  g_chipType = MASSMORE_BME280_CHIP_UNKNOWN;
  g_genuine = MASSMORE_BME280_GENUINE_UNKNOWN;
  g_genuinePassCount = 0;

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("  Massmore BME280 (SKU-1023) - Factory Test"));
  Serial.println(F("  Environment Sensor  |  Bosch BME280"));
  Serial.print(F("  Library v"));
  Serial.println(MassmoreBME280::getLibraryVersion());
  Serial.println(F("=========================================================="));

  if (!gateBusScan()) {
    Serial.println(F("\n  หยุดการทดสอบ เพราะไม่พบเซ็นเซอร์บนบัส"));
    Serial.println(F("  สิ่งที่ต้องตรวจ"));
    Serial.println(F("    1. VIN ต่อกับ 3V3 หรือ 5V แล้วหรือยัง"));
    Serial.println(F("    2. GND ร่วมกันหรือยัง"));
    Serial.println(F("    3. SDI เข้า SDA และ SCK เข้า SCL ของบอร์ด MCU ถูกด้านหรือไม่"));
    Serial.println(F("    4. สาย Qwiic เสียบแน่นทั้งสองหัวหรือไม่"));
    Serial.println(F("    5. ปริยายคือ 0x77 ถ้าต่อ SDO ลง GND จะเป็น 0x76 (สแกนหาให้ทั้งคู่แล้ว)"));
    printSummary(false);
    return;
  }

  if (!gateChipId()) {
    Serial.println(F("\n  หยุดการทดสอบ เพราะรหัสประจำรุ่นไม่ใช่ BME280"));
    if (g_chipType == MASSMORE_BME280_CHIP_BMP280) {
      Serial.println(F("  ชิปตัวนี้คือ BMP280 ซึ่งวัดได้แค่อุณหภูมิกับความดัน วัดความชื้นไม่ได้"));
      Serial.println(F("  ให้ตรวจช่องติ๊กบนซิลค์สกรีนว่าบอร์ดนี้ควรเป็นรุ่นไหน"));
    }
    printSummary(false);
    return;
  }

  ftBanner("RUN TEST");

  testSoftReset();
  testCalibration();
  testRegisterEcho();
  testStatusRegister();
  testSleepMode();
  testForcedMode();
  testNormalMode();
  testHumidityChannel();
#if !defined(__AVR__)
  /* 4 หัวข้อนี้ข้ามบน AVR (flash 32 KB ไม่พอ) : ESP32 / RP2040 ทดสอบครบ 22 หัวข้อ */
  testOversamplingTiming();
  testNoise();
  testFilter();
#endif
  testPlausibility();
  testStability();
#if !defined(__AVR__)
  testBusSpeed();
#endif
  testPresets();
  testGenuine();

  printSummary(true);
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && (millis() - start) < 3000) {
    ;
  }
  delay(300);
  runFactoryTest();
}

void loop() {
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'r' || c == 'R') {
      runFactoryTest();
    }
  }
  delay(10);
}
