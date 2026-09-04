/*!
 * @file Massmore_BME280.cpp
 * @brief ตัวขับเซ็นเซอร์ Bosch BME280 สำหรับบอร์ด Massmore SKU-1023
 *
 * สูตรชดเชยทุกสูตรในไฟล์นี้คัดลอกโครงมาจากดาต้าชีต BST-BME280-DS002
 * หัวข้อ 4.2.3 "Compensation formulas" ตรง ๆ โดยไม่ดัดแปลงลำดับการคำนวณ
 * เพราะการสลับลำดับหรือเปลี่ยนชนิดตัวแปรจะทำให้ผลต่างจากที่ Bosch รับรองไว้
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include "Massmore_BME280.h"

#include <math.h>

/* ========================================================================= */
/* ตัวสร้าง                                                                  */
/* ========================================================================= */

MassmoreBME280::MassmoreBME280()
    : _wire(NULL),
      _address(MASSMORE_BME280_I2C_ADDR_A),
      _begun(false),
      _hasHumidity(false),
      _chipId(0),
      _tFine(0),
      _mode(MASSMORE_BME280_MODE_SLEEP),
      _osrsT(MASSMORE_BME280_SAMPLING_X1),
      _osrsP(MASSMORE_BME280_SAMPLING_X1),
      _osrsH(MASSMORE_BME280_SAMPLING_X1),
      _filter(MASSMORE_BME280_FILTER_OFF),
      _standby(MASSMORE_BME280_STANDBY_125_MS),
      _offsetT(0.0f),
      _offsetP(0.0f),
      _offsetH(0.0f),
      _seaLevelhPa(MASSMORE_BME280_SEALEVEL_HPA_DEFAULT),
      _forcedStartMs(0),
      _forcedPending(false),
      _lastError(MASSMORE_BME280_OK) {
  /* ล้างค่าชดเชยให้เป็นศูนย์ทั้งหมด กัน static analyzer บ่นเรื่องค่าไม่ถูกกำหนด */
  _calib.dig_T1 = 0;
  _calib.dig_T2 = 0;
  _calib.dig_T3 = 0;
  _calib.dig_P1 = 0;
  _calib.dig_P2 = 0;
  _calib.dig_P3 = 0;
  _calib.dig_P4 = 0;
  _calib.dig_P5 = 0;
  _calib.dig_P6 = 0;
  _calib.dig_P7 = 0;
  _calib.dig_P8 = 0;
  _calib.dig_P9 = 0;
  _calib.dig_H1 = 0;
  _calib.dig_H2 = 0;
  _calib.dig_H3 = 0;
  _calib.dig_H4 = 0;
  _calib.dig_H5 = 0;
  _calib.dig_H6 = 0;
}

/* ========================================================================= */
/* การสื่อสารระดับล่าง                                                       */
/* ========================================================================= */

bool MassmoreBME280::writeReg(uint8_t reg, uint8_t value) {
  if (_wire == NULL) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _wire->write(value);
  if (_wire->endTransmission() != 0) {
    _lastError = MASSMORE_BME280_ERR_I2C_WRITE;
    return false;
  }
  return true;
}

bool MassmoreBME280::readRegs(uint8_t reg, uint8_t *buffer, uint8_t length) {
  if (_wire == NULL) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }
  _wire->beginTransmission(_address);
  _wire->write(reg);
  /* ใช้ repeated start (ไม่ปล่อย stop) ตามที่ไดรเวอร์ของ Bosch เองใช้
     ทำให้ไม่มีใครแทรกคิวบัสมาเปลี่ยนตัวชี้รีจิสเตอร์ระหว่างกลางได้ */
  if (_wire->endTransmission(false) != 0) {
    _lastError = MASSMORE_BME280_ERR_I2C_WRITE;
    return false;
  }
  uint8_t got = _wire->requestFrom(_address, length);
  if (got != length) {
    _lastError = MASSMORE_BME280_ERR_I2C_READ;
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = (uint8_t)_wire->read();
  }
  return true;
}

bool MassmoreBME280::readReg8(uint8_t reg, uint8_t &value) {
  uint8_t buffer[1];
  if (!readRegs(reg, buffer, 1)) {
    return false;
  }
  value = buffer[0];
  return true;
}

uint16_t MassmoreBME280::readU16LE(const uint8_t *buffer, uint8_t offset) {
  return (uint16_t)((uint16_t)buffer[offset] | ((uint16_t)buffer[offset + 1] << 8));
}

int16_t MassmoreBME280::readS16LE(const uint8_t *buffer, uint8_t offset) {
  return (int16_t)readU16LE(buffer, offset);
}

/* ========================================================================= */
/* เริ่มต้นใช้งาน                                                            */
/* ========================================================================= */

bool MassmoreBME280::begin(uint8_t address, TwoWire *wire) {
  if (address != MASSMORE_BME280_I2C_ADDR_A && address != MASSMORE_BME280_I2C_ADDR_B) {
    _lastError = MASSMORE_BME280_ERR_BAD_ARG;
    return false;
  }
  _wire = wire;
  _address = address;
  _begun = false;

  /* ถ้าผู้ใช้ยังไม่ได้เรียก Wire.begin() เอง ให้เรียกด้วยขาปริยายของบอร์ดให้
     (ESP32 ปริยายคือ SDA 21 / SCL 22 ซึ่งตรงกับหัว Qwiic ของบอร์ด Massmore) */
  if (!isConnected()) {
    _wire->begin();
    delay(10);
    if (!isConnected()) {
      _lastError = MASSMORE_BME280_ERR_NO_DEVICE;
      return false;
    }
  }

  if (!readReg8(MASSMORE_BME280_REG_CHIP_ID, _chipId)) {
    _lastError = MASSMORE_BME280_ERR_NO_DEVICE;
    return false;
  }

  if (_chipId != MASSMORE_BME280_CHIP_ID_BME280) {
    /* ไม่ใช่ BME280 - บอกให้ชัดว่าเจออะไรแทน จะได้ไม่ต้องเดา */
    _lastError = MASSMORE_BME280_ERR_WRONG_CHIP;
    return false;
  }
  _hasHumidity = true;

  /* รีเซ็ตให้เริ่มจากสถานะเดียวกันทุกครั้ง แล้วรอชิปคัดลอกค่าชดเชยจาก NVM */
  if (!writeReg(MASSMORE_BME280_REG_RESET, MASSMORE_BME280_RESET_MAGIC)) {
    return false;
  }
  delay(10);

  uint32_t start = millis();
  while (isUpdatingNVM()) {
    if ((millis() - start) > 100) {
      _lastError = MASSMORE_BME280_ERR_TIMEOUT;
      return false;
    }
    delay(1);
  }

  if (!readCalibration()) {
    return false;
  }

  _begun = true;

  /* ค่าเริ่มต้นแบบ "ใช้ทั่วไป" อ่านได้ทันทีหลัง begin() โดยไม่ต้องตั้งอะไรเพิ่ม */
  _mode = MASSMORE_BME280_MODE_NORMAL;
  _osrsT = MASSMORE_BME280_SAMPLING_X1;
  _osrsP = MASSMORE_BME280_SAMPLING_X1;
  _osrsH = MASSMORE_BME280_SAMPLING_X1;
  _filter = MASSMORE_BME280_FILTER_OFF;
  _standby = MASSMORE_BME280_STANDBY_125_MS;
  if (!applySettings()) {
    _begun = false;
    return false;
  }

  /* รอให้รอบวัดแรกเสร็จ ผู้ใช้จะได้ไม่เจอค่า 0 ในลูปแรก */
  delay(measurementTimeMaxMs() + 5);

  _lastError = MASSMORE_BME280_OK;
  return true;
}

bool MassmoreBME280::beginAuto(TwoWire *wire) {
  if (begin(MASSMORE_BME280_I2C_ADDR_A, wire)) {
    return true;
  }
  return begin(MASSMORE_BME280_I2C_ADDR_B, wire);
}

bool MassmoreBME280::isConnected() {
  if (_wire == NULL) {
    return false;
  }
  _wire->beginTransmission(_address);
  return (_wire->endTransmission() == 0);
}

bool MassmoreBME280::readCalibration() {
  uint8_t tp[MASSMORE_BME280_CALIB_TP_LEN];
  uint8_t h[MASSMORE_BME280_CALIB_H_LEN];

  if (!readRegs(MASSMORE_BME280_REG_CALIB_TP, tp, MASSMORE_BME280_CALIB_TP_LEN)) {
    return false;
  }
  if (!readRegs(MASSMORE_BME280_REG_CALIB_H, h, MASSMORE_BME280_CALIB_H_LEN)) {
    return false;
  }

  /* บล็อกแรก 0x88-0xA1 : dig_T1..T3, dig_P1..P9, ที่ว่างหนึ่งไบต์, dig_H1 */
  _calib.dig_T1 = readU16LE(tp, 0);
  _calib.dig_T2 = readS16LE(tp, 2);
  _calib.dig_T3 = readS16LE(tp, 4);
  _calib.dig_P1 = readU16LE(tp, 6);
  _calib.dig_P2 = readS16LE(tp, 8);
  _calib.dig_P3 = readS16LE(tp, 10);
  _calib.dig_P4 = readS16LE(tp, 12);
  _calib.dig_P5 = readS16LE(tp, 14);
  _calib.dig_P6 = readS16LE(tp, 16);
  _calib.dig_P7 = readS16LE(tp, 18);
  _calib.dig_P8 = readS16LE(tp, 20);
  _calib.dig_P9 = readS16LE(tp, 22);
  /* tp[24] คือ 0xA0 ไบต์สงวน ไม่ใช้ */
  _calib.dig_H1 = tp[25];

  /* บล็อกที่สอง 0xE1-0xE7 : dig_H2..H6
     สังเกตว่า dig_H4 และ dig_H5 เป็นค่า 12 บิตที่ใช้ไบต์ 0xE5 ร่วมกันคนละครึ่ง
     ครึ่งล่างของ 0xE5 เป็นของ H4 ครึ่งบนเป็นของ H5 (ดาต้าชีตตาราง 16) */
  _calib.dig_H2 = readS16LE(h, 0);
  _calib.dig_H3 = h[2];
  _calib.dig_H4 = (int16_t)(((int16_t)(int8_t)h[3] << 4) | (int16_t)(h[4] & 0x0F));
  _calib.dig_H5 = (int16_t)(((int16_t)(int8_t)h[5] << 4) | (int16_t)(h[4] >> 4));
  _calib.dig_H6 = (int8_t)h[6];

  /* ค่าชดเชยที่เป็นศูนย์หรือ 0xFFFF ทั้งก้อนแปลว่าอ่านไม่ติด หรือชิปไม่ใช่ของจริง */
  if (_calib.dig_T1 == 0 || _calib.dig_T1 == 0xFFFF || _calib.dig_P1 == 0 ||
      _calib.dig_P1 == 0xFFFF) {
    _lastError = MASSMORE_BME280_ERR_CALIB;
    return false;
  }

  return true;
}

/* ========================================================================= */
/* สูตรชดเชยตามดาต้าชีต                                                      */
/* ========================================================================= */

/* หมายเหตุสำหรับคนที่อ่านโค้ดนี้เพื่อเรียนรู้
   ตัวเลขมหาศาลที่เห็น (เช่น 1048576, 3125, 419430400) มาจากดาต้าชีตทั้งหมด
   ห้ามปัดเศษหรือย่อ เพราะเป็นการคำนวณแบบจุดตรึงที่ Bosch คำนวณ error ไว้แล้ว */

int32_t MassmoreBME280::compensateTemperature(int32_t adc_T) {
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)_calib.dig_T1 << 1))) * ((int32_t)_calib.dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)_calib.dig_T1)) *
            ((adc_T >> 4) - ((int32_t)_calib.dig_T1))) >>
           12) *
          ((int32_t)_calib.dig_T3)) >>
         14;

  _tFine = var1 + var2;
  T = (_tFine * 5 + 128) >> 8;
  return T; /* หน่วย 0.01 องศาเซลเซียส */
}

uint32_t MassmoreBME280::compensatePressure(int32_t adc_P) {
  int64_t var1, var2, p;

  var1 = ((int64_t)_tFine) - 128000;
  var2 = var1 * var1 * (int64_t)_calib.dig_P6;
  var2 = var2 + ((var1 * (int64_t)_calib.dig_P5) << 17);
  var2 = var2 + (((int64_t)_calib.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)_calib.dig_P3) >> 8) +
         ((var1 * (int64_t)_calib.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)_calib.dig_P1) >> 33;

  if (var1 == 0) {
    return 0; /* กันหารด้วยศูนย์ตามที่ดาต้าชีตกำหนดไว้เอง */
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)_calib.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)_calib.dig_P7) << 4);

  return (uint32_t)p; /* Q24.8 หน่วยปาสคาล */
}

uint32_t MassmoreBME280::compensateHumidity(int32_t adc_H) {
  int32_t v_x1_u32r;

  v_x1_u32r = (_tFine - ((int32_t)76800));
  v_x1_u32r =
      (((((adc_H << 14) - (((int32_t)_calib.dig_H4) << 20) -
          (((int32_t)_calib.dig_H5) * v_x1_u32r)) +
         ((int32_t)16384)) >>
        15) *
       (((((((v_x1_u32r * ((int32_t)_calib.dig_H6)) >> 10) *
            (((v_x1_u32r * ((int32_t)_calib.dig_H3)) >> 11) + ((int32_t)32768))) >>
           10) +
          ((int32_t)2097152)) *
             ((int32_t)_calib.dig_H2) +
         8192) >>
        14));
  v_x1_u32r =
      (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)_calib.dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

  return (uint32_t)(v_x1_u32r >> 12); /* Q22.10 หน่วย %RH */
}

/* ========================================================================= */
/* การอ่านค่า                                                                */
/* ========================================================================= */

bool MassmoreBME280::readRawADC(massmore_bme280_raw_t &out) {
  if (!_begun) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }

  uint8_t buffer[MASSMORE_BME280_DATA_LEN];
  if (!readRegs(MASSMORE_BME280_REG_DATA, buffer, MASSMORE_BME280_DATA_LEN)) {
    return false;
  }

  /* ความดันและอุณหภูมิเป็นค่า 20 บิต เก็บใน 3 ไบต์ (msb, lsb, xlsb บิตบนสุด 4 บิต)
     ความชื้นเป็นค่า 16 บิตเต็ม 2 ไบต์ */
  out.adc_P = (int32_t)(((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) |
                        ((uint32_t)buffer[2] >> 4));
  out.adc_T = (int32_t)(((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) |
                        ((uint32_t)buffer[5] >> 4));
  out.adc_H = (int32_t)(((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7]);

  if (out.adc_T != MASSMORE_BME280_RAW_SKIPPED_20BIT) {
    compensateTemperature(out.adc_T);
  }
  out.t_fine = _tFine;

  _lastError = MASSMORE_BME280_OK;
  return true;
}

bool MassmoreBME280::read(massmore_bme280_reading_t &out) {
  out.temperature = NAN;
  out.pressure = NAN;
  out.humidity = NAN;
  out.altitude = NAN;
  out.timestamp = 0;
  out.valid = false;

  if (!_begun) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }

  /* ในโหมด forced ชิปหลับอยู่ ต้องสะกิดให้วัดก่อนหนึ่งครั้ง */
  if (_mode == MASSMORE_BME280_MODE_FORCED) {
    if (!takeForcedMeasurement()) {
      return false;
    }
  }

  massmore_bme280_raw_t raw;
  if (!readRawADC(raw)) {
    return false;
  }

  if (raw.adc_T == MASSMORE_BME280_RAW_SKIPPED_20BIT) {
    /* ปิดช่องอุณหภูมิไว้ ทำให้ไม่มี t_fine จึงคำนวณความดันและความชื้นต่อไม่ได้
       ตามหลักการของชิปตัวนี้ (ดาต้าชีตหัวข้อ 4.2.2) */
    _lastError = MASSMORE_BME280_ERR_WRONG_MODE;
    return false;
  }

  int32_t t = compensateTemperature(raw.adc_T);
  out.temperature = ((float)t / 100.0f) + _offsetT;

  if (raw.adc_P != MASSMORE_BME280_RAW_SKIPPED_20BIT) {
    uint32_t p = compensatePressure(raw.adc_P);
    out.pressure = (((float)p / 256.0f) / 100.0f) + _offsetP; /* Pa -> hPa */
    out.altitude = 44330.0f * (1.0f - powf(out.pressure / _seaLevelhPa, 0.1903f));
  }

  if (_hasHumidity && raw.adc_H != MASSMORE_BME280_RAW_SKIPPED_16BIT) {
    uint32_t h = compensateHumidity(raw.adc_H);
    out.humidity = ((float)h / 1024.0f) + _offsetH;
    if (out.humidity > 100.0f) {
      out.humidity = 100.0f;
    } else if (out.humidity < 0.0f) {
      out.humidity = 0.0f;
    }
  }

  out.timestamp = millis();
  out.valid = true;
  _lastError = MASSMORE_BME280_OK;
  return true;
}

float MassmoreBME280::readTemperature() {
  massmore_bme280_reading_t r;
  if (!read(r)) {
    return NAN;
  }
  return r.temperature;
}

float MassmoreBME280::readPressure() {
  massmore_bme280_reading_t r;
  if (!read(r)) {
    return NAN;
  }
  return r.pressure;
}

float MassmoreBME280::readPressurePa() {
  float hpa = readPressure();
  if (isnan(hpa)) {
    return NAN;
  }
  return hpa * 100.0f;
}

float MassmoreBME280::readHumidity() {
  if (!_hasHumidity) {
    _lastError = MASSMORE_BME280_ERR_NO_HUMIDITY;
    return NAN;
  }
  massmore_bme280_reading_t r;
  if (!read(r)) {
    return NAN;
  }
  return r.humidity;
}

float MassmoreBME280::readAltitude(float seaLevelhPa) {
  float pressure = readPressure();
  if (isnan(pressure)) {
    return NAN;
  }
  return 44330.0f * (1.0f - powf(pressure / seaLevelhPa, 0.1903f));
}

/* ========================================================================= */
/* การตั้งค่า                                                                */
/* ========================================================================= */

uint8_t MassmoreBME280::samplingToBits(massmore_bme280_sampling_t sampling) {
  switch (sampling) {
    case MASSMORE_BME280_SAMPLING_NONE:
      return MASSMORE_BME280_OSRS_SKIPPED;
    case MASSMORE_BME280_SAMPLING_X1:
      return MASSMORE_BME280_OSRS_X1;
    case MASSMORE_BME280_SAMPLING_X2:
      return MASSMORE_BME280_OSRS_X2;
    case MASSMORE_BME280_SAMPLING_X4:
      return MASSMORE_BME280_OSRS_X4;
    case MASSMORE_BME280_SAMPLING_X8:
      return MASSMORE_BME280_OSRS_X8;
    case MASSMORE_BME280_SAMPLING_X16:
    default:
      return MASSMORE_BME280_OSRS_X16;
  }
}

uint8_t MassmoreBME280::modeToBits(massmore_bme280_mode_t mode) {
  switch (mode) {
    case MASSMORE_BME280_MODE_SLEEP:
      return MASSMORE_BME280_MODE_SLEEP_BITS;
    case MASSMORE_BME280_MODE_FORCED:
      return MASSMORE_BME280_MODE_FORCED_BITS;
    case MASSMORE_BME280_MODE_NORMAL:
    default:
      return MASSMORE_BME280_MODE_NORMAL_BITS;
  }
}

uint16_t MassmoreBME280::samplingFactor(massmore_bme280_sampling_t sampling) {
  switch (sampling) {
    case MASSMORE_BME280_SAMPLING_NONE:
      return 0;
    case MASSMORE_BME280_SAMPLING_X1:
      return 1;
    case MASSMORE_BME280_SAMPLING_X2:
      return 2;
    case MASSMORE_BME280_SAMPLING_X4:
      return 4;
    case MASSMORE_BME280_SAMPLING_X8:
      return 8;
    case MASSMORE_BME280_SAMPLING_X16:
    default:
      return 16;
  }
}

bool MassmoreBME280::applySettings() {
  /* ลำดับนี้สำคัญมาก ห้ามสลับ (ดาต้าชีตหัวข้อ 5.4.3 และ 5.4.6)
       1. เข้าโหมด sleep ก่อน ไม่งั้นการเขียน config จะถูกชิปเมิน
       2. เขียน config (standby + filter)
       3. เขียน ctrl_hum
       4. เขียน ctrl_meas  <- การเขียนตัวนี้เท่านั้นที่ทำให้ ctrl_hum มีผลจริง */
  uint8_t ctrlMeasSleep = (uint8_t)((samplingToBits(_osrsT) << 5) |
                                    (samplingToBits(_osrsP) << 2) |
                                    MASSMORE_BME280_MODE_SLEEP_BITS);
  if (!writeReg(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeasSleep)) {
    return false;
  }

  uint8_t config = (uint8_t)(((uint8_t)_standby << 5) | ((uint8_t)_filter << 2));
  if (!writeReg(MASSMORE_BME280_REG_CONFIG, config)) {
    return false;
  }

  if (!writeReg(MASSMORE_BME280_REG_CTRL_HUM, samplingToBits(_osrsH))) {
    return false;
  }

  uint8_t ctrlMeas = (uint8_t)((samplingToBits(_osrsT) << 5) |
                               (samplingToBits(_osrsP) << 2) | modeToBits(_mode));
  if (!writeReg(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas)) {
    return false;
  }

  _lastError = MASSMORE_BME280_OK;
  return true;
}

bool MassmoreBME280::setSampling(massmore_bme280_mode_t mode,
                                 massmore_bme280_sampling_t tempSampling,
                                 massmore_bme280_sampling_t pressSampling,
                                 massmore_bme280_sampling_t humSampling,
                                 massmore_bme280_filter_t filter,
                                 massmore_bme280_standby_t standby) {
  if (!_begun) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }
  _mode = mode;
  _osrsT = tempSampling;
  _osrsP = pressSampling;
  _osrsH = humSampling;
  _filter = filter;
  _standby = standby;
  return applySettings();
}

bool MassmoreBME280::useWeatherStationPreset() {
  /* ดาต้าชีตตาราง 7 : วัดนาน ๆ ครั้ง กินไฟต่ำสุด */
  return setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_SAMPLING_X1, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);
}

bool MassmoreBME280::useHumiditySensingPreset() {
  /* ดาต้าชีตตาราง 8 : เน้นความชื้น ปิดช่องความดันเพื่อประหยัดไฟ */
  return setSampling(MASSMORE_BME280_MODE_FORCED, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_SAMPLING_NONE, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_FILTER_OFF, MASSMORE_BME280_STANDBY_1000_MS);
}

bool MassmoreBME280::useIndoorNavigationPreset() {
  /* ดาต้าชีตตาราง 9 : noise ต่ำสุด เหมาะกับวัดความสูงในอาคาร */
  return setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X2,
                     MASSMORE_BME280_SAMPLING_X16, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_FILTER_16, MASSMORE_BME280_STANDBY_0_5_MS);
}

bool MassmoreBME280::useGamingPreset() {
  /* ดาต้าชีตตาราง 10 : ตอบสนองไว ไม่ต้องใช้ความชื้น */
  return setSampling(MASSMORE_BME280_MODE_NORMAL, MASSMORE_BME280_SAMPLING_X1,
                     MASSMORE_BME280_SAMPLING_X4, MASSMORE_BME280_SAMPLING_NONE,
                     MASSMORE_BME280_FILTER_16, MASSMORE_BME280_STANDBY_0_5_MS);
}

bool MassmoreBME280::setMode(massmore_bme280_mode_t mode) {
  if (!_begun) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }
  _mode = mode;
  uint8_t ctrlMeas = (uint8_t)((samplingToBits(_osrsT) << 5) |
                               (samplingToBits(_osrsP) << 2) | modeToBits(_mode));
  return writeReg(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas);
}

bool MassmoreBME280::setTemperatureOversampling(massmore_bme280_sampling_t s) {
  _osrsT = s;
  return applySettings();
}

bool MassmoreBME280::setPressureOversampling(massmore_bme280_sampling_t s) {
  _osrsP = s;
  return applySettings();
}

bool MassmoreBME280::setHumidityOversampling(massmore_bme280_sampling_t s) {
  _osrsH = s;
  return applySettings();
}

bool MassmoreBME280::setFilter(massmore_bme280_filter_t filter) {
  _filter = filter;
  return applySettings();
}

bool MassmoreBME280::setStandbyTime(massmore_bme280_standby_t standby) {
  _standby = standby;
  return applySettings();
}

/* ========================================================================= */
/* โหมด forced                                                               */
/* ========================================================================= */

bool MassmoreBME280::startForcedMeasurement() {
  if (!_begun) {
    _lastError = MASSMORE_BME280_ERR_NOT_BEGUN;
    return false;
  }
  /* ctrl_hum ต้องถูกเขียนก่อน ctrl_meas เสมอ ไม่งั้นค่า osrs_h จะไม่ถูกใช้
     ในรอบวัดที่กำลังจะเกิดขึ้น (ข้อผิดพลาดยอดฮิตของ BME280) */
  if (!writeReg(MASSMORE_BME280_REG_CTRL_HUM, samplingToBits(_osrsH))) {
    return false;
  }
  uint8_t ctrlMeas = (uint8_t)((samplingToBits(_osrsT) << 5) |
                               (samplingToBits(_osrsP) << 2) |
                               MASSMORE_BME280_MODE_FORCED_BITS);
  if (!writeReg(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeas)) {
    return false;
  }
  _forcedStartMs = millis();
  _forcedPending = true;
  return true;
}

bool MassmoreBME280::isMeasurementReady() {
  if (!_forcedPending) {
    return true;
  }
  /* รอให้ครบเวลาตามดาต้าชีตก่อน แล้วค่อยเช็คบิต measuring
     เพราะช่วงแรกสุดหลังสั่งวัด บิตนี้อาจยังไม่ทันขึ้น */
  if ((millis() - _forcedStartMs) < (uint32_t)measurementTimeMaxMs()) {
    return false;
  }
  if (isMeasuring()) {
    return false;
  }
  _forcedPending = false;
  return true;
}

bool MassmoreBME280::takeForcedMeasurement() {
  if (!startForcedMeasurement()) {
    return false;
  }
  return waitForMeasurement(MASSMORE_BME280_TIMEOUT_DEFAULT_MS);
}

bool MassmoreBME280::waitForMeasurement(uint32_t timeoutMs) {
  uint32_t start = millis();
  delay(measurementTimeMaxMs());
  while (isMeasuring()) {
    if ((millis() - start) > timeoutMs) {
      _forcedPending = false;
      _lastError = MASSMORE_BME280_ERR_TIMEOUT;
      return false;
    }
    delay(1);
  }
  _forcedPending = false;
  return true;
}

bool MassmoreBME280::isMeasuring() {
  uint8_t status = 0;
  if (!readReg8(MASSMORE_BME280_REG_STATUS, status)) {
    return false;
  }
  return (status & MASSMORE_BME280_STATUS_MEASURING) != 0;
}

bool MassmoreBME280::isUpdatingNVM() {
  uint8_t status = 0;
  if (!readReg8(MASSMORE_BME280_REG_STATUS, status)) {
    return false;
  }
  return (status & MASSMORE_BME280_STATUS_IM_UPDATE) != 0;
}

bool MassmoreBME280::readStatus(uint8_t &status) {
  return readReg8(MASSMORE_BME280_REG_STATUS, status);
}

bool MassmoreBME280::reset() {
  if (!writeReg(MASSMORE_BME280_REG_RESET, MASSMORE_BME280_RESET_MAGIC)) {
    return false;
  }
  delay(10);
  uint32_t start = millis();
  while (isUpdatingNVM()) {
    if ((millis() - start) > 100) {
      _lastError = MASSMORE_BME280_ERR_TIMEOUT;
      return false;
    }
    delay(1);
  }
  if (!readCalibration()) {
    return false;
  }
  /* เขียนค่าที่ผู้ใช้ตั้งไว้กลับให้เอง ผู้ใช้จะได้ไม่ต้องจำว่าต้องตั้งใหม่ */
  if (_begun) {
    return applySettings();
  }
  return true;
}

uint16_t MassmoreBME280::measurementTimeMs() const {
  /* คำนวณเป็นหน่วยหนึ่งในสิบมิลลิวินาทีเพื่อเลี่ยงเลขทศนิยม
     t = 1 + 2*osrs_t + (2*osrs_p + 0.5) + (2*osrs_h + 0.5) */
  uint32_t tenths = 10;
  uint16_t ot = samplingFactor(_osrsT);
  uint16_t op = samplingFactor(_osrsP);
  uint16_t oh = samplingFactor(_osrsH);

  if (ot > 0) {
    tenths += 20UL * ot;
  }
  if (op > 0) {
    tenths += 20UL * op + 5UL;
  }
  if (oh > 0) {
    tenths += 20UL * oh + 5UL;
  }
  return (uint16_t)((tenths + 9) / 10);
}

uint16_t MassmoreBME280::measurementTimeMaxMs() const {
  /* หน่วยหนึ่งในร้อยมิลลิวินาที
     t = 1.25 + 2.3*osrs_t + (2.3*osrs_p + 0.575) + (2.3*osrs_h + 0.575) */
  uint32_t hundredths = 125;
  uint16_t ot = samplingFactor(_osrsT);
  uint16_t op = samplingFactor(_osrsP);
  uint16_t oh = samplingFactor(_osrsH);

  if (ot > 0) {
    hundredths += 230UL * ot;
  }
  if (op > 0) {
    hundredths += 230UL * op + 58UL;
  }
  if (oh > 0) {
    hundredths += 230UL * oh + 58UL;
  }
  return (uint16_t)((hundredths + 99) / 100);
}

/* ========================================================================= */
/* รีจิสเตอร์ทั่วไป                                                          */
/* ========================================================================= */

bool MassmoreBME280::readRegister(uint8_t reg, uint8_t &value) {
  return readReg8(reg, value);
}

bool MassmoreBME280::writeRegister(uint8_t reg, uint8_t value) {
  return writeReg(reg, value);
}

/* ========================================================================= */
/* ตรวจตัวตนของชิป                                                           */
/* ========================================================================= */

uint8_t MassmoreBME280::getChipID() {
  uint8_t id = 0;
  if (!readReg8(MASSMORE_BME280_REG_CHIP_ID, id)) {
    return 0;
  }
  _chipId = id;
  return id;
}

massmore_bme280_chip_t MassmoreBME280::getChipType() {
  uint8_t id = getChipID();
  switch (id) {
    case MASSMORE_BME280_CHIP_ID_BME280:
      return MASSMORE_BME280_CHIP_BME280;
    case MASSMORE_BME280_CHIP_ID_BMP280:
    case MASSMORE_BME280_CHIP_ID_BMP280_S1:
    case MASSMORE_BME280_CHIP_ID_BMP280_S2:
      return MASSMORE_BME280_CHIP_BMP280;
    case MASSMORE_BME280_CHIP_ID_BME680:
      return MASSMORE_BME280_CHIP_BME680;
    case 0x00:
    case 0xFF:
      return MASSMORE_BME280_CHIP_NO_RESPONSE;
    default:
      return MASSMORE_BME280_CHIP_UNKNOWN;
  }
}

massmore_bme280_genuine_t MassmoreBME280::verifyChip(massmore_bme280_identity_t *out) {
  massmore_bme280_identity_t id;
  id.chipIdOk = false;
  id.calibTempOk = false;
  id.calibPressOk = false;
  id.calibHumOk = false;
  id.calibUniqueOk = false;
  id.resetOk = false;
  id.ctrlHumLatchOk = false;
  id.registerEchoOk = false;
  id.measuringBitOk = false;
  id.humidityLiveOk = false;
  id.passCount = 0;
  id.verdict = MASSMORE_BME280_GENUINE_UNKNOWN;

  /* --- ข้อ 1 : รหัสประจำรุ่น --- */
  uint8_t chipId = getChipID();
  id.chipIdOk = (chipId == MASSMORE_BME280_CHIP_ID_BME280);

  /* ถ้าเจอ BMP280 ให้ตัดจบทันที ไม่ต้องทดสอบต่อ เพราะพฤติกรรมต่างกันโดยสิ้นเชิง */
  if (chipId == MASSMORE_BME280_CHIP_ID_BMP280 ||
      chipId == MASSMORE_BME280_CHIP_ID_BMP280_S1 ||
      chipId == MASSMORE_BME280_CHIP_ID_BMP280_S2 || chipId == 0x00 || chipId == 0xFF) {
    id.verdict = MASSMORE_BME280_GENUINE_NO;
    if (out != NULL) {
      *out = id;
    }
    return id.verdict;
  }

  /* --- ข้อ 2-5 : ตรวจค่าชดเชยที่โรงงานเบิร์นไว้ ---
     ของปลอมที่ทำเป็นตารางค่าตายตัวจะตกข้อเหล่านี้ เพราะช่วงค่าที่ Bosch ใช้จริง
     แคบกว่าที่คนนอกเดา และค่าความชื้นชุด H ต้องมีครบด้วย */
  readCalibration();

  id.calibTempOk = (_calib.dig_T1 >= 20000 && _calib.dig_T1 <= 36000 &&
                    _calib.dig_T2 >= 22000 && _calib.dig_T2 <= 30000 &&
                    _calib.dig_T3 >= -6000 && _calib.dig_T3 <= 6000);

  id.calibPressOk = (_calib.dig_P1 >= 28000 && _calib.dig_P1 <= 48000 &&
                     _calib.dig_P2 >= -20000 && _calib.dig_P2 <= 0 &&
                     _calib.dig_P9 != 0 && _calib.dig_P7 != 0);

  id.calibHumOk = (_calib.dig_H1 != 0 && _calib.dig_H1 != 0xFF &&
                   _calib.dig_H2 >= 200 && _calib.dig_H2 <= 500 &&
                   _calib.dig_H6 != 0);

  /* ค่าชดเชยของชิปจริงไม่มีทางซ้ำกันหมด ถ้าซ้ำแปลว่าเป็นตารางที่ใครสักคนใส่ไว้ */
  id.calibUniqueOk = !(_calib.dig_T1 == _calib.dig_P1 && _calib.dig_T2 == _calib.dig_P2) &&
                     (_calib.dig_T2 != _calib.dig_T3) && (_calib.dig_P4 != _calib.dig_P5);

  /* --- ข้อ 6 : soft reset ต้องล้างรีจิสเตอร์กลับเป็นค่าโรงงานจริง --- */
  uint8_t ctrlMeasAfterReset = 0xAA;
  uint8_t configAfterReset = 0xAA;
  uint8_t ctrlHumAfterReset = 0xAA;
  if (writeReg(MASSMORE_BME280_REG_CTRL_MEAS, 0xB7) &&
      writeReg(MASSMORE_BME280_REG_RESET, MASSMORE_BME280_RESET_MAGIC)) {
    delay(10);
    readReg8(MASSMORE_BME280_REG_CTRL_MEAS, ctrlMeasAfterReset);
    readReg8(MASSMORE_BME280_REG_CONFIG, configAfterReset);
    readReg8(MASSMORE_BME280_REG_CTRL_HUM, ctrlHumAfterReset);
  }
  id.resetOk = (ctrlMeasAfterReset == 0x00 && configAfterReset == 0x00 &&
                ctrlHumAfterReset == 0x00);

  /* --- ข้อ 8 : เขียน config แล้วอ่านกลับต้องได้ทุกบิต --- */
  const uint8_t configProbe =
      (uint8_t)((MASSMORE_BME280_STANDBY_500_BITS << 5) | (MASSMORE_BME280_FILTER_16_BITS << 2));
  uint8_t configEcho = 0;
  if (writeReg(MASSMORE_BME280_REG_CONFIG, configProbe) &&
      readReg8(MASSMORE_BME280_REG_CONFIG, configEcho)) {
    id.registerEchoOk = (configEcho == configProbe);
  }
  writeReg(MASSMORE_BME280_REG_CONFIG, 0x00);

  /* --- ข้อ 7 : ช่องความชื้นต้องเปิดปิดตาม osrs_h ได้จริง ---
     ปิด osrs_h แล้ววัดหนึ่งครั้ง ค่าดิบต้องเป็น 0x8000 (skipped)
     เปิด osrs_h x1 แล้ววัดอีกครั้ง ค่าดิบต้องไม่ใช่ 0x8000
     BMP280 ที่ถูกสกรีนเป็น BME280 จะให้ 0x8000 ทั้งสองรอบ */
  uint8_t buffer[MASSMORE_BME280_DATA_LEN];
  int32_t rawHumOff = 0;
  int32_t rawHumOn = 0;

  writeReg(MASSMORE_BME280_REG_CTRL_HUM, MASSMORE_BME280_OSRS_SKIPPED);
  writeReg(MASSMORE_BME280_REG_CTRL_MEAS,
           (uint8_t)((MASSMORE_BME280_OSRS_X1 << 5) | (MASSMORE_BME280_OSRS_X1 << 2) |
                     MASSMORE_BME280_MODE_FORCED_BITS));
  delay(20);
  if (readRegs(MASSMORE_BME280_REG_DATA, buffer, MASSMORE_BME280_DATA_LEN)) {
    rawHumOff = (int32_t)(((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7]);
  }

  writeReg(MASSMORE_BME280_REG_CTRL_HUM, MASSMORE_BME280_OSRS_X1);
  writeReg(MASSMORE_BME280_REG_CTRL_MEAS,
           (uint8_t)((MASSMORE_BME280_OSRS_X1 << 5) | (MASSMORE_BME280_OSRS_X1 << 2) |
                     MASSMORE_BME280_MODE_FORCED_BITS));
  delay(20);
  if (readRegs(MASSMORE_BME280_REG_DATA, buffer, MASSMORE_BME280_DATA_LEN)) {
    rawHumOn = (int32_t)(((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7]);
  }

  id.ctrlHumLatchOk = (rawHumOff == MASSMORE_BME280_RAW_SKIPPED_16BIT) &&
                      (rawHumOn != MASSMORE_BME280_RAW_SKIPPED_16BIT) && (rawHumOn != 0);

  /* --- ข้อ 10 : ค่าความชื้นที่คำนวณออกมาต้องอยู่ในโลกความจริง --- */
  int32_t rawT = (int32_t)(((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) |
                           ((uint32_t)buffer[5] >> 4));
  if (rawT != MASSMORE_BME280_RAW_SKIPPED_20BIT &&
      rawHumOn != MASSMORE_BME280_RAW_SKIPPED_16BIT) {
    compensateTemperature(rawT);
    float rh = (float)compensateHumidity(rawHumOn) / 1024.0f;
    id.humidityLiveOk = (rh > 0.5f && rh < 99.5f);
  }

  /* --- ข้อ 9 : บิต measuring ต้องขึ้นจริงระหว่างวัด --- */
  writeReg(MASSMORE_BME280_REG_CTRL_HUM, MASSMORE_BME280_OSRS_X16);
  writeReg(MASSMORE_BME280_REG_CTRL_MEAS,
           (uint8_t)((MASSMORE_BME280_OSRS_X16 << 5) | (MASSMORE_BME280_OSRS_X16 << 2) |
                     MASSMORE_BME280_MODE_FORCED_BITS));
  uint8_t statusDuring = 0;
  readReg8(MASSMORE_BME280_REG_STATUS, statusDuring);
  delay(150);
  uint8_t statusAfter = 0xFF;
  readReg8(MASSMORE_BME280_REG_STATUS, statusAfter);
  id.measuringBitOk = ((statusDuring & MASSMORE_BME280_STATUS_MEASURING) != 0) &&
                      ((statusAfter & MASSMORE_BME280_STATUS_MEASURING) == 0);

  /* --- นับคะแนนและสรุปผล --- */
  const bool checks[10] = {id.chipIdOk,       id.calibTempOk,    id.calibPressOk,
                           id.calibHumOk,     id.calibUniqueOk,  id.resetOk,
                           id.ctrlHumLatchOk, id.registerEchoOk, id.measuringBitOk,
                           id.humidityLiveOk};
  for (uint8_t i = 0; i < 10; i++) {
    if (checks[i]) {
      id.passCount++;
    }
  }

  if (!id.chipIdOk || !id.ctrlHumLatchOk || id.passCount < 8) {
    id.verdict = MASSMORE_BME280_GENUINE_NO;
  } else if (id.passCount == 10) {
    id.verdict = MASSMORE_BME280_GENUINE_YES;
  } else {
    id.verdict = MASSMORE_BME280_GENUINE_SUSPECT;
  }

  /* คืนค่าที่ผู้ใช้ตั้งไว้ให้เหมือนเดิม เพราะการตรวจข้างบนไปยุ่งกับรีจิสเตอร์เยอะ */
  readCalibration();
  if (_begun) {
    applySettings();
  }

  if (out != NULL) {
    *out = id;
  }
  return id.verdict;
}

bool MassmoreBME280::isGenuine() {
  return verifyChip(NULL) == MASSMORE_BME280_GENUINE_YES;
}

/* ========================================================================= */
/* ค่าที่คำนวณต่อ                                                            */
/* ========================================================================= */

float MassmoreBME280::seaLevelForAltitude(float altitudeMeters, float pressurehPa) {
  return pressurehPa / powf(1.0f - (altitudeMeters / 44330.0f), 5.255f);
}

float MassmoreBME280::saturationVaporPressure(float temperatureC) {
  /* สูตร Magnus แบบที่ใช้กันทั่วไปในงานอุตุนิยมวิทยา หน่วยผลลัพธ์ hPa */
  return 6.112f * expf((17.62f * temperatureC) / (243.12f + temperatureC));
}

float MassmoreBME280::dewPoint(float temperatureC, float humidityRH) {
  if (isnan(temperatureC) || isnan(humidityRH) || humidityRH <= 0.0f) {
    return NAN;
  }
  const float a = 17.62f;
  const float b = 243.12f;
  float gamma = logf(humidityRH / 100.0f) + (a * temperatureC) / (b + temperatureC);
  return (b * gamma) / (a - gamma);
}

float MassmoreBME280::absoluteHumidity(float temperatureC, float humidityRH) {
  if (isnan(temperatureC) || isnan(humidityRH)) {
    return NAN;
  }
  /* ก./ลบ.ม. = 216.7 * (RH/100 * es(T)) / (273.15 + T) */
  float es = saturationVaporPressure(temperatureC);
  return 216.7f * ((humidityRH / 100.0f) * es) / (273.15f + temperatureC);
}

/* ========================================================================= */
/* ข้อความ                                                                   */
/* ========================================================================= */

const char *MassmoreBME280::errorToString(massmore_bme280_error_t error) {
  switch (error) {
    case MASSMORE_BME280_OK:
      return "สำเร็จ";
    case MASSMORE_BME280_ERR_NOT_BEGUN:
      return "ยังไม่ได้เรียก begin()";
    case MASSMORE_BME280_ERR_NO_DEVICE:
      return "ไม่มีอุปกรณ์ตอบที่ address นี้ ตรวจสาย VCC GND SDA SCL";
    case MASSMORE_BME280_ERR_I2C_WRITE:
      return "เขียนลงบัส I2C ไม่สำเร็จ";
    case MASSMORE_BME280_ERR_I2C_READ:
      return "อ่านจากบัส I2C ได้ไบต์ไม่ครบ";
    case MASSMORE_BME280_ERR_WRONG_CHIP:
      return "รหัสชิปไม่ใช่ 0x60 อาจเป็น BMP280 หรือของเลียนแบบ";
    case MASSMORE_BME280_ERR_TIMEOUT:
      return "รอผลวัดเกินเวลาที่กำหนด";
    case MASSMORE_BME280_ERR_NOT_READY:
      return "ยังวัดไม่เสร็จ";
    case MASSMORE_BME280_ERR_WRONG_MODE:
      return "โหมดหรือค่า oversampling ไม่เหมาะกับสิ่งที่สั่ง";
    case MASSMORE_BME280_ERR_BAD_ARG:
      return "พารามิเตอร์ไม่ถูกต้อง";
    case MASSMORE_BME280_ERR_CALIB:
      return "ค่าชดเชยจากโรงงานผิดปกติ";
    case MASSMORE_BME280_ERR_NO_HUMIDITY:
      return "ชิปตัวนี้ไม่มีเซ็นเซอร์ความชื้น (BMP280)";
    default:
      return "ข้อผิดพลาดที่ไม่รู้จัก";
  }
}

const char *MassmoreBME280::chipToString(massmore_bme280_chip_t chip) {
  switch (chip) {
    case MASSMORE_BME280_CHIP_BME280:
      return "BME280";
    case MASSMORE_BME280_CHIP_BMP280:
      return "BMP280";
    case MASSMORE_BME280_CHIP_BME680:
      return "BME680";
    case MASSMORE_BME280_CHIP_NO_RESPONSE:
      return "ไม่มีการตอบสนอง";
    case MASSMORE_BME280_CHIP_UNKNOWN:
    default:
      return "ไม่รู้จัก";
  }
}

const char *MassmoreBME280::genuineToString(massmore_bme280_genuine_t genuine) {
  switch (genuine) {
    case MASSMORE_BME280_GENUINE_YES:
      return "ของแท้";
    case MASSMORE_BME280_GENUINE_SUSPECT:
      return "น่าสงสัย";
    case MASSMORE_BME280_GENUINE_NO:
      return "ไม่ผ่าน";
    case MASSMORE_BME280_GENUINE_UNKNOWN:
    default:
      return "ยังไม่ได้ตรวจ";
  }
}

const char *MassmoreBME280::samplingToString(massmore_bme280_sampling_t sampling) {
  switch (sampling) {
    case MASSMORE_BME280_SAMPLING_NONE:
      return "ปิด";
    case MASSMORE_BME280_SAMPLING_X1:
      return "x1";
    case MASSMORE_BME280_SAMPLING_X2:
      return "x2";
    case MASSMORE_BME280_SAMPLING_X4:
      return "x4";
    case MASSMORE_BME280_SAMPLING_X8:
      return "x8";
    case MASSMORE_BME280_SAMPLING_X16:
    default:
      return "x16";
  }
}

const char *MassmoreBME280::filterToString(massmore_bme280_filter_t filter) {
  switch (filter) {
    case MASSMORE_BME280_FILTER_OFF:
      return "ปิด";
    case MASSMORE_BME280_FILTER_2:
      return "2";
    case MASSMORE_BME280_FILTER_4:
      return "4";
    case MASSMORE_BME280_FILTER_8:
      return "8";
    case MASSMORE_BME280_FILTER_16:
    default:
      return "16";
  }
}

const char *MassmoreBME280::standbyToString(massmore_bme280_standby_t standby) {
  switch (standby) {
    case MASSMORE_BME280_STANDBY_0_5_MS:
      return "0.5 ms";
    case MASSMORE_BME280_STANDBY_62_5_MS:
      return "62.5 ms";
    case MASSMORE_BME280_STANDBY_125_MS:
      return "125 ms";
    case MASSMORE_BME280_STANDBY_250_MS:
      return "250 ms";
    case MASSMORE_BME280_STANDBY_500_MS:
      return "500 ms";
    case MASSMORE_BME280_STANDBY_1000_MS:
      return "1000 ms";
    case MASSMORE_BME280_STANDBY_10_MS:
      return "10 ms";
    case MASSMORE_BME280_STANDBY_20_MS:
    default:
      return "20 ms";
  }
}
