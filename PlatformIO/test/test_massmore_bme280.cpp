/*!
 * @file test_massmore_bme280.cpp
 * @brief ชุดทดสอบไลบรารี Massmore_BME280 ที่รันบนเครื่อง PC ได้เลย
 *
 * ไม่ต้องมีบอร์ด ไม่ต้องมี PlatformIO ใช้แค่ g++
 *   cd PlatformIO/test && make
 *
 * สิ่งที่ทดสอบ
 *   - แผนที่รีจิสเตอร์ตรงกับดาต้าชีต
 *   - การถอดค่าชดเชย 32 ตัว โดยเฉพาะ dig_H4 / dig_H5 ที่ใช้ไบต์ร่วมกัน
 *   - สูตรชดเชยให้ผลตรงกับตัวเลขตัวอย่างในดาต้าชีต
 *   - ลำดับการเขียนรีจิสเตอร์ (config -> ctrl_hum -> ctrl_meas)
 *   - สูตรเวลาที่ใช้วัดตามหัวข้อ 9.1
 *   - เส้นทางที่ผิดพลาดทุกทาง
 *   - การแยก BME280 ออกจาก BMP280
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../lib/Massmore_BME280/src/Massmore_BME280.h"
#include "massmore_bme280_host_shim.h"

/* ------------------------------------------------------------------ */
/* โครงชุดทดสอบเล็ก ๆ                                                   */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;
static const char *g_group = "";

static void group(const char *name) {
  g_group = name;
  printf("\n--- %s ---\n", name);
}

static void check(bool condition, const char *what) {
  if (condition) {
    g_pass++;
    printf("  [ OK ] %s\n", what);
  } else {
    g_fail++;
    printf("  [FAIL] %s  (กลุ่ม %s)\n", what, g_group);
  }
}

static void checkEqI(long actual, long expected, const char *what) {
  if (actual == expected) {
    g_pass++;
    printf("  [ OK ] %s\n", what);
  } else {
    g_fail++;
    printf("  [FAIL] %s : ได้ %ld ควรได้ %ld\n", what, actual, expected);
  }
}

static void checkNear(double actual, double expected, double tolerance, const char *what) {
  if (fabs(actual - expected) <= tolerance) {
    g_pass++;
    printf("  [ OK ] %s\n", what);
  } else {
    g_fail++;
    printf("  [FAIL] %s : ได้ %.4f ควรได้ %.4f +/- %.4f\n", what, actual, expected,
           tolerance);
  }
}

/*! หาว่ารีจิสเตอร์ reg ถูกเขียนเป็นลำดับที่เท่าไรใน log (-1 = ไม่พบ) */
static int firstWriteIndex(uint8_t reg) {
  for (uint8_t i = 0; i < g_fakeBme.writeLogLength; i++) {
    if (g_fakeBme.writeLog[i] == reg) {
      return (int)i;
    }
  }
  return -1;
}

static int lastWriteIndex(uint8_t reg) {
  int found = -1;
  for (uint8_t i = 0; i < g_fakeBme.writeLogLength; i++) {
    if (g_fakeBme.writeLog[i] == reg) {
      found = (int)i;
    }
  }
  return found;
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 1 : ค่าคงที่และแผนที่รีจิสเตอร์                                  */
/* ------------------------------------------------------------------ */

static void testConstants() {
  group("ค่าคงที่และแผนที่รีจิสเตอร์");

  checkEqI(MASSMORE_BME280_I2C_ADDR_A, 0x76, "address A (SDO=GND) คือ 0x76");
  checkEqI(MASSMORE_BME280_I2C_ADDR_B, 0x77, "address B (SDO=VDDIO) คือ 0x77");
  checkEqI(MASSMORE_BME280_I2C_ADDR_DEFAULT, 0x77, "address ปริยายของบอร์ด SKU-1023 คือ 0x77");
  checkEqI(MASSMORE_BME280_REG_CHIP_ID, 0xD0, "รีจิสเตอร์ chip id อยู่ที่ 0xD0");
  checkEqI(MASSMORE_BME280_CHIP_ID_BME280, 0x60, "รหัส BME280 คือ 0x60");
  checkEqI(MASSMORE_BME280_CHIP_ID_BMP280, 0x58, "รหัส BMP280 คือ 0x58");
  checkEqI(MASSMORE_BME280_REG_RESET, 0xE0, "รีจิสเตอร์ reset อยู่ที่ 0xE0");
  checkEqI(MASSMORE_BME280_RESET_MAGIC, 0xB6, "ค่าที่ใช้สั่ง reset คือ 0xB6");
  checkEqI(MASSMORE_BME280_REG_CTRL_HUM, 0xF2, "ctrl_hum อยู่ที่ 0xF2");
  checkEqI(MASSMORE_BME280_REG_STATUS, 0xF3, "status อยู่ที่ 0xF3");
  checkEqI(MASSMORE_BME280_REG_CTRL_MEAS, 0xF4, "ctrl_meas อยู่ที่ 0xF4");
  checkEqI(MASSMORE_BME280_REG_CONFIG, 0xF5, "config อยู่ที่ 0xF5");
  checkEqI(MASSMORE_BME280_REG_DATA, 0xF7, "ข้อมูลดิบเริ่มที่ 0xF7");
  checkEqI(MASSMORE_BME280_DATA_LEN, 8, "ข้อมูลดิบยาว 8 ไบต์");
  checkEqI(MASSMORE_BME280_CALIB_TP_LEN, 26, "ค่าชดเชยชุด T/P ยาว 26 ไบต์");
  checkEqI(MASSMORE_BME280_CALIB_H_LEN, 7, "ค่าชดเชยชุด H ยาว 7 ไบต์");
  checkEqI(MASSMORE_BME280_STATUS_MEASURING, 0x08, "บิต measuring คือบิต 3");
  checkEqI(MASSMORE_BME280_STATUS_IM_UPDATE, 0x01, "บิต im_update คือบิต 0");
  check(strcmp(MassmoreBME280::getLibraryVersion(), "1.1.0") == 0, "เวอร์ชันไลบรารี 1.1.0");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 2 : การถอดค่าชดเชย                                              */
/* ------------------------------------------------------------------ */

static void testCalibration() {
  group("การถอดค่าชดเชยจากรีจิสเตอร์");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  check(bme.begin(), "begin() สำเร็จกับชิปจำลอง");

  massmore_bme280_calib_t c;
  bme.getCalibration(c);

  checkEqI(c.dig_T1, 27504, "dig_T1 ถอดถูก (uint16 little endian)");
  checkEqI(c.dig_T2, 26435, "dig_T2 ถอดถูก (int16)");
  checkEqI(c.dig_T3, -1000, "dig_T3 เป็นค่าลบได้ถูกต้อง");
  checkEqI(c.dig_P1, 36477, "dig_P1 ถอดถูก");
  checkEqI(c.dig_P2, -10685, "dig_P2 เป็นค่าลบได้ถูกต้อง");
  checkEqI(c.dig_P6, -7, "dig_P6 ค่าลบเล็ก ๆ ถอดถูก");
  checkEqI(c.dig_P9, 6000, "dig_P9 ถอดถูก");
  checkEqI(c.dig_H1, 75, "dig_H1 อ่านจาก 0xA1 ถูก");
  checkEqI(c.dig_H2, 366, "dig_H2 ถอดถูก");
  checkEqI(c.dig_H4, 301, "dig_H4 ประกอบจาก 0xE4 กับครึ่งล่างของ 0xE5 ถูก");
  checkEqI(c.dig_H5, 50, "dig_H5 ประกอบจาก 0xE6 กับครึ่งบนของ 0xE5 ถูก");
  checkEqI(c.dig_H6, 30, "dig_H6 ถอดถูก");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 3 : สูตรชดเชยตามดาต้าชีต                                        */
/* ------------------------------------------------------------------ */

static void testCompensation() {
  group("สูตรชดเชยเทียบกับตัวเลขตัวอย่างในดาต้าชีต");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  bme.begin();

  /* ดาต้าชีตหัวข้อ 4.2.3 ระบุว่า adc_T = 519888 กับค่าชดเชยชุดนี้
     ต้องได้ t_fine = 128422 และ T = 2508 (25.08 องศาเซลเซียส) */
  int32_t t = bme.compensateTemperature(519888L);
  checkEqI(t, 2508, "compensateTemperature(519888) = 2508 (25.08 องศา)");
  checkEqI(bme.getTFine(), 128422, "t_fine = 128422 ตามดาต้าชีต");

  /* ดาต้าชีตระบุว่า adc_P = 415148 ให้ผลราว 100653.27 Pa
     สูตรจำนวนเต็ม 64 บิตให้ค่า 25767233 ในรูป Q24.8 = 100653.254 Pa
     ตัวเลข 25767236 ที่ดาต้าชีตเขียนไว้มาจากสูตรทศนิยม ต่างกัน 0.02 Pa
     ซึ่งน้อยกว่าความละเอียดของชิป (0.18 Pa) หลายเท่า */
  uint32_t p = bme.compensatePressure(415148L);
  checkEqI((long)p, 25767233L, "compensatePressure(415148) = 25767233 (Q24.8)");
  checkNear((double)p / 256.0, 100653.27, 0.5, "แปลงแล้วได้ 100653.27 Pa ตามดาต้าชีต");

  /* ความชื้น : ดาต้าชีตไม่ได้ให้ตัวเลขตัวอย่างไว้ จึงตรวจว่าอยู่ในช่วงที่เป็นไปได้
     และตรวจว่าฟังก์ชันคุมขอบเขตบน 419430400 ได้จริง */
  uint32_t h = bme.compensateHumidity(25000L);
  double rh = (double)h / 1024.0;
  check(rh > 0.0 && rh <= 100.0, "compensateHumidity ให้ค่าในช่วง 0-100 %RH");
  checkNear(rh, 31.7, 1.5, "compensateHumidity(25000) อยู่แถว 31.7 %RH");

  uint32_t hMax = bme.compensateHumidity(65535L);
  check(((double)hMax / 1024.0) <= 100.0, "ค่าดิบสูงสุดถูกคุมไม่ให้เกิน 100 %RH");

  uint32_t hMin = bme.compensateHumidity(0L);
  checkEqI((long)hMin, 0L, "ค่าดิบ 0 ถูกคุมไม่ให้ติดลบ");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 4 : begin และเส้นทางที่ผิดพลาด                                   */
/* ------------------------------------------------------------------ */

static void testBeginPaths() {
  group("begin() และเส้นทางที่ผิดพลาด");

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(bme.begin(), "เจอชิปที่ 0x77 (ปริยาย) โดยไม่ต้องส่งพารามิเตอร์");
    checkEqI(bme.getBus(), MASSMORE_BME280_BUS_I2C, "บัสที่ใช้คือ I2C");
    checkEqI(Wire.beginCount, 0, "ไลบรารีไม่เรียก Wire.begin() เอง");
    checkEqI(bme.getChipID(), 0x60, "อ่าน chip id ได้ 0x60");
    check(bme.hasHumidity(), "รู้ว่าชิปมีเซ็นเซอร์ความชื้น");
    checkEqI(bme.lastError(), MASSMORE_BME280_OK, "ไม่มีข้อผิดพลาดค้าง");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(!bme.begin(0x76, &Wire), "ไม่เจอชิปที่ 0x76 เมื่อบอร์ดตั้งไว้ 0x77");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_NO_DEVICE, "รายงานว่าไม่มีอุปกรณ์");
  }

  {
    hostResetFakeBme280();
    g_fakeBme.address = 0x76;
    MassmoreBME280 bme;
    check(bme.beginAuto(&Wire), "beginAuto() ไล่หาเจอที่ 0x76 (หลังลอง 0x77 ก่อน)");
    checkEqI(bme.getAddress(), 0x76, "จำ address ที่เจอไว้ถูก");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(!bme.begin(0x50, Wire), "ปฏิเสธ address ที่ไม่ใช่ 0x76 หรือ 0x77");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_BAD_ARG, "รายงานว่าพารามิเตอร์ผิด");
  }

  {
    hostRemoveFakeDevice();
    MassmoreBME280 bme;
    check(!bme.begin(), "บัสว่างเปล่าแล้ว begin() ต้องไม่ผ่าน");
  }

  {
    hostMakeFakeBmp280();
    MassmoreBME280 bme;
    check(!bme.begin(), "เจอ BMP280 แล้ว begin() ต้องไม่ผ่าน");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_WRONG_CHIP, "รายงานว่าเป็นชิปผิดรุ่น");
    checkEqI(bme.getChipType(), MASSMORE_BME280_CHIP_BMP280, "ระบุรุ่นได้ว่าเป็น BMP280");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    massmore_bme280_reading_t r;
    check(!bme.read(r), "อ่านค่าก่อน begin() ต้องไม่สำเร็จ");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_NOT_BEGUN, "รายงานว่ายังไม่ begin()");
    check(isnan(bme.readTemperature()), "readTemperature() คืน NAN เมื่อยังไม่ begin()");
  }
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 5 : การตั้งค่าและลำดับการเขียนรีจิสเตอร์                          */
/* ------------------------------------------------------------------ */

static void testSettings() {
  group("การตั้งค่าและลำดับการเขียนรีจิสเตอร์");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  bme.begin();

  g_fakeBme.writeLogLength = 0;
  check(bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X2,
                        MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X1,
                        MASSMORE_BME280_FILTER_16, MASSMORE_BME280_STANDBY_0_5_MS),
        "setSampling() สำเร็จ");

  int idxConfig = firstWriteIndex(0xF5);
  int idxHum = firstWriteIndex(0xF2);
  int idxMeas = lastWriteIndex(0xF4);
  check(idxConfig >= 0 && idxHum >= 0 && idxMeas >= 0, "เขียนครบทั้ง config, ctrl_hum, ctrl_meas");
  check(idxConfig < idxHum, "เขียน config ก่อน ctrl_hum");
  check(idxHum < idxMeas, "เขียน ctrl_hum ก่อน ctrl_meas (ไม่งั้น osrs_h จะไม่มีผล)");

  checkEqI(g_fakeBme.regs[0xF2], 0x01, "ctrl_hum = 0x01 (ความชื้น x1)");
  checkEqI(g_fakeBme.regs[0xF4], (0x02 << 5) | (0x05 << 2) | 0x03,
           "ctrl_meas ประกอบบิตถูก (osrs_t x2, osrs_p x16, normal)");
  checkEqI(g_fakeBme.regs[0xF5], (0x00 << 5) | (0x04 << 2), "config ประกอบบิตถูก (standby 0.5 ms, filter 16)");
  checkEqI(g_fakeBme.latchedOsrsH, 0x01, "ชิปจำลอง latch ค่า osrs_h ไว้แล้ว");

  checkEqI(bme.getTemperatureOversampling(), MASSMORE_BME280_SAMPLING_X2, "จำค่า osrs_t ไว้ถูก");
  checkEqI(bme.getPressureOversampling(), MASSMORE_BME280_SAMPLING_X16, "จำค่า osrs_p ไว้ถูก");
  checkEqI(bme.getFilter(), MASSMORE_BME280_FILTER_16, "จำค่าฟิลเตอร์ไว้ถูก");
  checkEqI(bme.getStandbyTime(), MASSMORE_BME280_STANDBY_0_5_MS, "จำค่า standby ไว้ถูก");
  checkEqI(bme.getMode(), MASSMORE_BME280_MODE_NORMAL, "จำโหมดไว้ถูก");

  /* ปิดช่องความชื้นแล้วค่าดิบต้องกลายเป็น skipped */
  bme.setHumidityOversampling(MASSMORE_BME280_SAMPLING_NONE);
  massmore_bme280_raw_t raw;
  check(bme.readRawADC(raw), "อ่านค่าดิบสำเร็จ");
  checkEqI(raw.adc_H, 0x8000L, "ปิดช่องความชื้นแล้วค่าดิบเป็น 0x8000");

  massmore_bme280_reading_t r;
  check(bme.read(r), "อ่านค่าสำเร็จแม้ปิดช่องความชื้น");
  check(isnan(r.humidity), "ความชื้นเป็น NAN เมื่อปิดช่องไว้");
  check(!isnan(r.temperature), "อุณหภูมิยังอ่านได้ปกติ");

  /* ชุดตั้งค่าสำเร็จรูป */
  check(bme.useIndoorNavigationPreset(), "ชุดสำเร็จรูป indoor navigation ตั้งได้");
  checkEqI(bme.getPressureOversampling(), MASSMORE_BME280_SAMPLING_X16,
           "indoor navigation ใช้ความดัน x16");
  checkEqI(bme.getFilter(), MASSMORE_BME280_FILTER_16, "indoor navigation เปิดฟิลเตอร์ 16");

  check(bme.useWeatherStationPreset(), "ชุดสำเร็จรูป weather station ตั้งได้");
  checkEqI(bme.getMode(), MASSMORE_BME280_MODE_FORCED, "weather station ใช้โหมด forced");

  check(bme.useGamingPreset(), "ชุดสำเร็จรูป gaming ตั้งได้");
  checkEqI(bme.getHumidityOversampling(), MASSMORE_BME280_SAMPLING_NONE,
           "gaming ปิดช่องความชื้น");

  check(bme.useHumiditySensingPreset(), "ชุดสำเร็จรูป humidity sensing ตั้งได้");
  checkEqI(bme.getPressureOversampling(), MASSMORE_BME280_SAMPLING_NONE,
           "humidity sensing ปิดช่องความดัน");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 6 : สูตรเวลาที่ใช้วัด (ดาต้าชีตหัวข้อ 9.1)                        */
/* ------------------------------------------------------------------ */

static void testMeasurementTime() {
  group("สูตรเวลาที่ใช้วัด");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  bme.begin();

  /* x1 ทั้งสามช่อง : 1 + 2 + 2.5 + 2.5 = 8 ms */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  checkEqI(bme.measurementTimeMs(), 8, "x1 ทุกช่อง ใช้เวลาทั่วไป 8 ms");
  checkEqI(bme.measurementTimeMaxMs(), 10, "x1 ทุกช่อง กรณีแย่สุด 10 ms");

  /* x16 ทั้งสามช่อง : 1 + 32 + 32.5 + 32.5 = 98 ms */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  checkEqI(bme.measurementTimeMs(), 98, "x16 ทุกช่อง ใช้เวลาทั่วไป 98 ms");
  checkEqI(bme.measurementTimeMaxMs(), 113, "x16 ทุกช่อง กรณีแย่สุด 112.8 -> 113 ms");

  /* ปิดช่องความดันและความชื้น เหลือแต่อุณหภูมิ x1 : 1 + 2 = 3 ms */
  bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_NONE, MASSMORE_BME280_SAMPLING_NONE,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_125_MS);
  checkEqI(bme.measurementTimeMs(), 3, "เหลือแต่อุณหภูมิ x1 ใช้เวลา 3 ms");

  /* ชุด indoor navigation ของดาต้าชีต : t x2, p x16, h x1
     1 + 4 + 32.5 + 2.5 = 40 ms */
  bme.useIndoorNavigationPreset();
  checkEqI(bme.measurementTimeMs(), 40, "ชุด indoor navigation ใช้เวลา 40 ms");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 7 : โหมด forced และโหมดไม่บล็อก                                 */
/* ------------------------------------------------------------------ */

static void testForcedMode() {
  group("โหมด forced และโหมดไม่บล็อก");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  bme.begin();
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);

  check(bme.takeForcedMeasurement(), "สั่งวัดแบบ forced สำเร็จ");
  checkEqI(g_fakeBme.regs[0xF4] & 0x03, 0x00, "วัดเสร็จแล้วชิปกลับไปโหมด sleep เอง");

  massmore_bme280_reading_t r;
  check(bme.read(r), "อ่านค่าในโหมด forced สำเร็จ (สะกิดให้วัดเองอัตโนมัติ)");
  check(r.valid, "ข้อมูลที่ได้ถูกทำเครื่องหมายว่าใช้ได้");
  checkNear(r.temperature, 25.08, 0.02, "อุณหภูมิตรงกับตัวอย่างดาต้าชีต 25.08 องศา");
  checkNear(r.pressure, 1006.53, 0.05, "ความดันตรงกับตัวอย่างดาต้าชีต 1006.53 hPa");
  check(r.humidity > 25.0f && r.humidity < 40.0f, "ความชื้นอยู่ในช่วงที่สมเหตุสมผล");
  check(r.timestamp > 0, "มีเวลาประทับติดมาด้วย");

  /* โหมดไม่บล็อก */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);
  check(bme.startForcedMeasurement(), "สั่งวัดแบบไม่บล็อกสำเร็จ");
  check(!bme.isMeasurementReady(), "ทันทีหลังสั่ง ผลยังไม่พร้อม");
  hostAdvanceMillis(5);
  check(!bme.isMeasurementReady(), "ผ่านไป 5 ms ผลยังไม่พร้อม (x16 ใช้ 113 ms)");
  hostAdvanceMillis(200);
  check(bme.isMeasurementReady(), "ผ่านไปเกินเวลาที่ต้องใช้แล้ว ผลพร้อม");

  /* ความสูง */
  bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                  MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);
  float alt = bme.readAltitude(1013.25f);
  checkNear(alt, 56.0, 6.0, "ความดัน 1006.53 hPa ให้ความสูงราว 56 เมตร");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 8 : การตรวจตัวตนของชิป                                          */
/* ------------------------------------------------------------------ */

static void testVerifyChip() {
  group("การตรวจตัวตนของชิป");

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();

    massmore_bme280_identity_t id;
    massmore_bme280_genuine_t verdict = bme.verifyChip(&id);

    check(id.chipIdOk, "ข้อ 1 รหัสชิปเป็น 0x60");
    check(id.calibTempOk, "ข้อ 2 ค่าชดเชยอุณหภูมิอยู่ในช่วงที่ Bosch ใช้จริง");
    check(id.calibPressOk, "ข้อ 3 ค่าชดเชยความดันอยู่ในช่วงที่ Bosch ใช้จริง");
    check(id.calibHumOk, "ข้อ 4 มีค่าชดเชยความชื้นครบ");
    check(id.calibUniqueOk, "ข้อ 5 ค่าชดเชยไม่ซ้ำแบบตารางปลอม");
    check(id.resetOk, "ข้อ 6 soft reset ล้างรีจิสเตอร์ได้จริง");
    check(id.ctrlHumLatchOk, "ข้อ 7 ช่องความชื้นเปิดปิดตาม osrs_h ได้จริง");
    check(id.registerEchoOk, "ข้อ 8 เขียน config แล้วอ่านกลับได้ครบทุกบิต");
    check(id.measuringBitOk, "ข้อ 9 บิต measuring ขึ้นลงตามจริง");
    check(id.humidityLiveOk, "ข้อ 10 ค่าความชื้นที่คำนวณได้อยู่ในโลกความจริง");
    checkEqI(id.passCount, 10, "ผ่านครบ 10 ข้อ");
    checkEqI(verdict, MASSMORE_BME280_GENUINE_YES, "สรุปว่าเป็นของแท้");

    /* ตรวจว่าหลังตรวจตัวตนแล้ว ค่าที่ผู้ใช้ตั้งไว้ยังอยู่ครบ */
    massmore_bme280_reading_t r;
    check(bme.read(r), "หลัง verifyChip() ยังอ่านค่าได้ตามปกติ");
    check(r.valid, "ค่าที่อ่านหลังตรวจยังใช้ได้");
  }

  {
    /* ชิปที่ค่าชดเชยความชื้นหายไป (BMP280 ที่ถูกสกรีนเป็น BME280) */
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();

    g_fakeBme.regs[0xA1] = 0x00; /* ล้าง dig_H1 */
    g_fakeBme.adcH = 0x8000L;    /* ไม่มีช่องความชื้นจริง */

    massmore_bme280_identity_t id;
    massmore_bme280_genuine_t verdict = bme.verifyChip(&id);
    check(!id.calibHumOk, "จับได้ว่าค่าชดเชยความชื้นหายไป");
    check(!id.ctrlHumLatchOk, "จับได้ว่าช่องความชื้นไม่ตอบสนอง");
    checkEqI(verdict, MASSMORE_BME280_GENUINE_NO, "สรุปว่าไม่ผ่าน");
  }

  {
    /* BMP280 แท้ ๆ ที่รหัสชิปเป็น 0x58 */
    hostMakeFakeBmp280();
    MassmoreBME280 bme;
    bme.begin(); /* จะไม่ผ่านอยู่แล้ว แต่ยังตรวจตัวตนได้ */

    massmore_bme280_identity_t id;
    massmore_bme280_genuine_t verdict = bme.verifyChip(&id);
    check(!id.chipIdOk, "รหัสชิป 0x58 ไม่ผ่านข้อแรก");
    checkEqI(verdict, MASSMORE_BME280_GENUINE_NO, "สรุปว่าไม่ใช่ BME280");
  }

  {
    /* ค่าชดเชยเป็นตารางซ้ำ ๆ แบบที่ของปลอมชอบใช้ */
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();
    for (uint8_t reg = 0x88; reg <= 0xA1; reg++) {
      g_fakeBme.regs[reg] = 0x55;
    }
    massmore_bme280_identity_t id;
    massmore_bme280_genuine_t verdict = bme.verifyChip(&id);
    check(!id.calibTempOk || !id.calibPressOk, "จับได้ว่าค่าชดเชยผิดปกติ");
    check(verdict != MASSMORE_BME280_GENUINE_YES, "ไม่สรุปว่าเป็นของแท้");
  }
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 9 : ค่าที่คำนวณต่อ                                               */
/* ------------------------------------------------------------------ */

static void testDerived() {
  group("ค่าที่คำนวณต่อ");

  /* จุดน้ำค้างที่ 100 %RH ต้องเท่ากับอุณหภูมิเอง */
  checkNear(MassmoreBME280::dewPoint(25.0f, 100.0f), 25.0, 0.1,
            "ที่ความชื้น 100 %RH จุดน้ำค้างเท่ากับอุณหภูมิ");
  /* ค่าอ้างอิงที่รู้กันทั่วไป 25 องศา 60 %RH ได้ราว 16.7 องศา */
  checkNear(MassmoreBME280::dewPoint(25.0f, 60.0f), 16.7, 0.3,
            "25 องศา 60 %RH ได้จุดน้ำค้างราว 16.7 องศา");
  check(MassmoreBME280::dewPoint(25.0f, 30.0f) < MassmoreBME280::dewPoint(25.0f, 60.0f),
        "ความชื้นน้อยลง จุดน้ำค้างต้องต่ำลง");
  check(isnan(MassmoreBME280::dewPoint(25.0f, 0.0f)), "ความชื้น 0 %RH คืน NAN");

  /* ความดันไออิ่มตัวที่ 20 องศา ราว 23.4 hPa */
  checkNear(MassmoreBME280::saturationVaporPressure(20.0f), 23.4, 0.3,
            "ความดันไออิ่มตัวที่ 20 องศา ราว 23.4 hPa");

  /* ความชื้นสัมบูรณ์ที่ 25 องศา 60 %RH ราว 13.8 กรัมต่อลูกบาศก์เมตร */
  checkNear(MassmoreBME280::absoluteHumidity(25.0f, 60.0f), 13.8, 0.5,
            "25 องศา 60 %RH ได้ราว 13.8 g/m3");

  /* seaLevelForAltitude กับ readAltitude ต้องเป็นฟังก์ชันผกผันกัน */
  float sea = MassmoreBME280::seaLevelForAltitude(100.0f, 1001.3f);
  checkNear(sea, 1013.25, 1.0, "อยู่สูง 100 เมตร วัดได้ 1001.3 hPa แปลว่าระดับน้ำทะเลราว 1013 hPa");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 10 : ข้อความและตัวช่วย                                           */
/* ------------------------------------------------------------------ */

static void testStrings() {
  group("ข้อความและตัวช่วย");

  check(strlen(MassmoreBME280::errorToString(MASSMORE_BME280_OK)) > 0, "ข้อความ OK ไม่ว่าง");
  check(strlen(MassmoreBME280::errorToString(MASSMORE_BME280_ERR_WRONG_CHIP)) > 0,
        "ข้อความชิปผิดรุ่นไม่ว่าง");
  check(strcmp(MassmoreBME280::chipToString(MASSMORE_BME280_CHIP_BME280), "BME280") == 0,
        "chipToString(BME280) = BME280");
  check(strcmp(MassmoreBME280::chipToString(MASSMORE_BME280_CHIP_BMP280), "BMP280") == 0,
        "chipToString(BMP280) = BMP280");
  check(strcmp(MassmoreBME280::samplingToString(MASSMORE_BME280_SAMPLING_X16), "x16") == 0,
        "samplingToString(X16) = x16");
  check(strcmp(MassmoreBME280::filterToString(MASSMORE_BME280_FILTER_16), "16") == 0,
        "filterToString(FILTER_16) = 16");
  check(strcmp(MassmoreBME280::standbyToString(MASSMORE_BME280_STANDBY_125_MS), "125 ms") == 0,
        "standbyToString(125 ms) ถูกต้อง");
  check(strlen(MassmoreBME280::genuineToString(MASSMORE_BME280_GENUINE_YES)) > 0,
        "ข้อความผลตรวจของแท้ไม่ว่าง");

  /* ทุกรหัสข้อผิดพลาดต้องมีข้อความของตัวเอง ไม่ตกไปที่ default */
  bool allHaveText = true;
  for (int i = 0; i <= (int)MASSMORE_BME280_ERR_NO_HUMIDITY; i++) {
    const char *text = MassmoreBME280::errorToString((massmore_bme280_error_t)i);
    if (text == NULL || strlen(text) == 0 || strcmp(text, "ข้อผิดพลาดที่ไม่รู้จัก") == 0) {
      allHaveText = false;
    }
  }
  check(allHaveText, "รหัสข้อผิดพลาดทุกตัวมีคำอธิบายภาษาไทยของตัวเอง");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 11 : ค่าชดเชยที่ผู้ใช้ตั้งเอง                                     */
/* ------------------------------------------------------------------ */

static void testOffsets() {
  group("ค่าชดเชยที่ผู้ใช้ตั้งเอง");

  hostResetFakeBme280();
  MassmoreBME280 bme;
  bme.begin();

  massmore_bme280_reading_t before;
  bme.read(before);

  bme.setTemperatureOffset(-2.0f);
  bme.setHumidityOffset(3.0f);
  bme.setPressureOffset(1.5f);

  massmore_bme280_reading_t after;
  bme.read(after);

  checkNear(after.temperature - before.temperature, -2.0, 0.01, "ค่าชดเชยอุณหภูมิถูกบวกเข้าไป");
  checkNear(after.pressure - before.pressure, 1.5, 0.01, "ค่าชดเชยความดันถูกบวกเข้าไป");
  checkNear(after.humidity - before.humidity, 3.0, 0.01, "ค่าชดเชยความชื้นถูกบวกเข้าไป");
  checkNear(bme.getTemperatureOffset(), -2.0, 0.001, "อ่านค่าชดเชยที่ตั้งไว้กลับมาได้");

  bme.setTemperatureOffset(0.0f);
  bme.setHumidityOffset(0.0f);
  bme.setPressureOffset(0.0f);

  /* ความดันที่ระดับน้ำทะเลที่ตั้งเอง มีผลกับความสูงใน read() */
  bme.setSeaLevelPressure(1006.53f);
  massmore_bme280_reading_t r;
  bme.read(r);
  checkNear(r.altitude, 0.0, 1.0, "ตั้งระดับน้ำทะเลเท่าค่าที่วัดได้ ความสูงต้องเป็น 0");
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 12 : บัส SPI                                                    */
/* ------------------------------------------------------------------ */

static void testSPI() {
  group("บัส SPI 4 สาย");

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(bme.beginSPI(10, SPI), "beginSPI(10, SPI) สำเร็จกับชิปจำลอง");
    check(bme.isSPI(), "isSPI() เป็นจริง");
    checkEqI(bme.getCSPin(), 10, "จำขา CS ไว้ถูก");
    checkEqI(SPI.beginCount, 0, "ไลบรารีไม่เรียก SPI.begin() เอง");
    checkEqI(hostGetPinState(10), HIGH, "หลังคุยเสร็จ CS ถูกปล่อยเป็น HIGH");
    checkEqI(SPI.lastMode, SPI_MODE0, "ใช้ SPI mode 0");
    checkEqI((long)SPI.lastClock, 1000000L, "ความถี่ปริยาย 1 MHz");
    check(!SPI.inTransaction, "ปิด transaction ทุกครั้งหลังคุยเสร็จ");

    massmore_bme280_calib_t c;
    bme.getCalibration(c);
    checkEqI(c.dig_T1, 27504, "อ่านค่าชดเชยผ่าน SPI ได้ถูกต้อง (dig_T1)");
    checkEqI(c.dig_H4, 301, "อ่านค่าชดเชยผ่าน SPI ได้ถูกต้อง (dig_H4)");

    g_fakeBme.writeLogLength = 0;
    check(bme.setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X2,
                          MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X1,
                          MASSMORE_BME280_FILTER_16, MASSMORE_BME280_STANDBY_0_5_MS),
          "setSampling() ผ่าน SPI สำเร็จ");
    check(firstWriteIndex(0xF5) < firstWriteIndex(0xF2) &&
              firstWriteIndex(0xF2) < lastWriteIndex(0xF4),
          "ลำดับเขียน config -> ctrl_hum -> ctrl_meas ผ่าน SPI ถูกต้อง");
    checkEqI(g_fakeBme.regs[0xF5] & 0x01, 0, "บิต spi3w_en เป็น 0 (ใช้ 4 สาย)");

    massmore_bme280_reading_t r;
    check(bme.read(r), "อ่านค่าผ่าน SPI สำเร็จ");
    checkNear(r.temperature, 25.08, 0.02, "อุณหภูมิผ่าน SPI ตรงดาต้าชีต 25.08 องศา");
    checkNear(r.pressure, 1006.53, 0.05, "ความดันผ่าน SPI ตรงดาต้าชีต 1006.53 hPa");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(bme.beginSPI(10, SPI, 20000000UL), "ขอ 20 MHz แล้ว begin ได้");
    checkEqI((long)SPI.lastClock, 10000000L, "ความถี่ถูกลดเหลือ 10 MHz ตามดาต้าชีต");
  }

  {
    hostRemoveFakeDevice();
    MassmoreBME280 bme;
    check(!bme.beginSPI(10, SPI), "ไม่มีชิปบน SPI (MISO อ่านได้ 0xFF) begin ต้องไม่ผ่าน");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_NO_DEVICE, "รายงานว่าไม่มีอุปกรณ์");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    check(!bme.beginSPI(-1, SPI), "ขา CS = -1 ถูกปฏิเสธ");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_BAD_ARG, "รายงานว่าพารามิเตอร์ผิด");
  }
}

/* ------------------------------------------------------------------ */
/* กลุ่ม 13 : FSM ไม่บล็อกชื่อมาตรฐาน                                     */
/* ------------------------------------------------------------------ */

static void testFSM() {
  group("FSM ไม่บล็อก requestConversion / update / isDataReady / getReadings");

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    massmore_bme280_reading_t r;
    check(!bme.requestConversion(), "requestConversion() ก่อน begin() ต้องไม่ผ่าน");
    check(!bme.getReadings(r), "getReadings() ก่อนมีผลต้องไม่ผ่าน");
    checkEqI(bme.getState(), MASSMORE_BME280_STATE_IDLE, "เริ่มต้นอยู่สถานะ IDLE");
  }

  {
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();
    bme.setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X16,
                    MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X16,
                    MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);

    check(bme.requestConversion(), "requestConversion() ในโหมด forced สำเร็จ");
    checkEqI(bme.getState(), MASSMORE_BME280_STATE_MEASURING, "เข้าสถานะ MEASURING");
    check(!bme.requestConversion(), "สั่งซ้ำระหว่างวัดถูกปฏิเสธ");
    checkEqI(bme.lastError(), MASSMORE_BME280_ERR_NOT_READY, "รายงานว่ายังไม่พร้อม");

    massmore_bme280_reading_t r;
    check(!bme.read(r), "read() แบบบล็อกระหว่าง FSM วัดอยู่ถูกปฏิเสธ");

    bme.update();
    check(!bme.isDataReady(), "ทันทีหลังสั่ง ยังไม่พร้อม");
    hostAdvanceMillis(5);
    bme.update();
    check(!bme.isDataReady(), "ผ่านไป 5 ms ยังไม่พร้อม (x16 ใช้ 113 ms)");
    hostAdvanceMillis(200);
    bme.update();
    check(bme.isDataReady(), "ผ่านไปเกินเวลาวัดแล้ว isDataReady() เป็นจริง");
    checkEqI(bme.getState(), MASSMORE_BME280_STATE_READY, "สถานะ READY");

    check(bme.getReadings(r), "getReadings() คืนผล");
    check(r.valid, "ผลถูกทำเครื่องหมายว่าใช้ได้");
    checkNear(r.temperature, 25.08, 0.02, "อุณหภูมิจาก FSM ตรงดาต้าชีต");
    checkEqI(bme.getState(), MASSMORE_BME280_STATE_IDLE, "รับผลแล้วกลับสู่ IDLE");
    check(!bme.isDataReady(), "รับผลแล้ว isDataReady() กลับเป็นเท็จ");
  }

  {
    /* โหมด normal : ชิปวัดอยู่แล้ว update() ครั้งเดียวได้ผลทันที */
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();
    check(bme.requestConversion(), "requestConversion() ในโหมด normal สำเร็จ");
    bme.update();
    check(bme.isDataReady(), "โหมด normal พร้อมทันทีหลัง update()");
    massmore_bme280_reading_t r;
    check(bme.getReadings(r) && r.valid, "getReadings() ในโหมด normal ได้ผลใช้ได้");
  }

  {
    /* timeout : ถอดชิปออกระหว่างวัด */
    hostResetFakeBme280();
    MassmoreBME280 bme;
    bme.begin();
    bme.setSampling(MASSMORE_BME280_MODE_FORCED);
    bme.requestConversion();
    g_fakeBme.present = false;
    hostAdvanceMillis(1000);
    bme.update();
    checkEqI(bme.getState(), MASSMORE_BME280_STATE_ERROR, "ถอดชิประหว่างวัด -> สถานะ ERROR");
    check(bme.lastError() == MASSMORE_BME280_ERR_TIMEOUT ||
              bme.lastError() == MASSMORE_BME280_ERR_I2C_WRITE,
          "รายงาน timeout หรือบัสล้มเหลว");
    g_fakeBme.present = true;
    check(bme.requestConversion(), "หลัง ERROR สั่งวัดใหม่ได้");
  }
}

/* ------------------------------------------------------------------ */

int main(void) {
  printf("==========================================================\n");
  printf("  ชุดทดสอบไลบรารี Massmore_BME280 %s\n", MassmoreBME280::getLibraryVersion());
  printf("  รันบนเครื่อง PC ไม่ต้องใช้บอร์ด\n");
  printf("==========================================================\n");

  testConstants();
  testCalibration();
  testCompensation();
  testBeginPaths();
  testSettings();
  testMeasurementTime();
  testForcedMode();
  testVerifyChip();
  testDerived();
  testStrings();
  testOffsets();
  testSPI();
  testFSM();

  printf("\n==========================================================\n");
  printf("  ผ่าน %d ข้อ   ไม่ผ่าน %d ข้อ   รวม %d ข้อ\n", g_pass, g_fail, g_pass + g_fail);
  printf("==========================================================\n");

  return (g_fail == 0) ? 0 : 1;
}
