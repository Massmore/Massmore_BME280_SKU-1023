/*!
 * @file massmore_bme280_host_shim.cpp
 * @brief ชิป BME280 จำลอง + ตัวแทน Wire สำหรับรันชุดทดสอบบนเครื่อง PC
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include "massmore_bme280_host_shim.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* นาฬิกาจำลอง                                                          */
/* ------------------------------------------------------------------ */

static uint32_t g_millis = 0;

uint32_t millis(void) { return g_millis; }

void delay(uint32_t ms) { g_millis += ms; }

void hostAdvanceMillis(uint32_t ms) { g_millis += ms; }

/* ------------------------------------------------------------------ */
/* GPIO จำลอง                                                           */
/* ------------------------------------------------------------------ */

static uint8_t g_pinState[64];

void pinMode(uint8_t pin, uint8_t mode) {
  (void)pin;
  (void)mode;
}

uint8_t hostGetPinState(uint8_t pin) { return (pin < 64) ? g_pinState[pin] : 0; }

/* ------------------------------------------------------------------ */
/* ชิปจำลอง                                                             */
/* ------------------------------------------------------------------ */

HostFakeBme280 g_fakeBme;

/* ค่าชดเชยชุดตัวอย่างจากดาต้าชีต BST-BME280-DS002 หัวข้อ 4.2.3
   (ชุด T และ P เป็นตัวเลขที่ดาต้าชีตใช้อธิบายสูตร ชุด H เป็นค่าที่พบจริงในชิป) */
static const uint16_t CAL_T1 = 27504;
static const int16_t CAL_T2 = 26435;
static const int16_t CAL_T3 = -1000;
static const uint16_t CAL_P1 = 36477;
static const int16_t CAL_P2 = -10685;
static const int16_t CAL_P3 = 3024;
static const int16_t CAL_P4 = 2855;
static const int16_t CAL_P5 = 140;
static const int16_t CAL_P6 = -7;
static const int16_t CAL_P7 = 15500;
static const int16_t CAL_P8 = -14600;
static const int16_t CAL_P9 = 6000;
static const uint8_t CAL_H1 = 75;
static const int16_t CAL_H2 = 366;
static const uint8_t CAL_H3 = 0;
static const int16_t CAL_H4 = 301;
static const int16_t CAL_H5 = 50;
static const int8_t CAL_H6 = 30;

static void putU16(uint8_t reg, uint16_t value) {
  g_fakeBme.regs[reg] = (uint8_t)(value & 0xFF);
  g_fakeBme.regs[reg + 1] = (uint8_t)(value >> 8);
}

static uint16_t osrsFactor(uint8_t bits) {
  switch (bits) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 2;
    case 3:
      return 4;
    case 4:
      return 8;
    default:
      return 16;
  }
}

/*! เขียนค่าดิบลงรีจิสเตอร์ข้อมูล ตามช่องที่เปิดอยู่ */
static void fakeProduceData(void) {
  uint8_t ctrlMeas = g_fakeBme.regs[0xF4];
  uint8_t osrsT = (uint8_t)((ctrlMeas >> 5) & 0x07);
  uint8_t osrsP = (uint8_t)((ctrlMeas >> 2) & 0x07);
  uint8_t osrsH = (uint8_t)(g_fakeBme.latchedOsrsH & 0x07);

  int32_t adcT = (osrsT == 0) ? 0x80000L : g_fakeBme.adcT;
  int32_t adcP = (osrsP == 0) ? 0x80000L : g_fakeBme.adcP;
  int32_t adcH = (osrsH == 0) ? 0x8000L : g_fakeBme.adcH;

  g_fakeBme.regs[0xF7] = (uint8_t)((adcP >> 12) & 0xFF);
  g_fakeBme.regs[0xF8] = (uint8_t)((adcP >> 4) & 0xFF);
  g_fakeBme.regs[0xF9] = (uint8_t)((adcP << 4) & 0xF0);
  g_fakeBme.regs[0xFA] = (uint8_t)((adcT >> 12) & 0xFF);
  g_fakeBme.regs[0xFB] = (uint8_t)((adcT >> 4) & 0xFF);
  g_fakeBme.regs[0xFC] = (uint8_t)((adcT << 4) & 0xF0);
  g_fakeBme.regs[0xFD] = (uint8_t)((adcH >> 8) & 0xFF);
  g_fakeBme.regs[0xFE] = (uint8_t)(adcH & 0xFF);

  /* เวลาที่ใช้วัดตามสูตรกรณีแย่ที่สุดของดาต้าชีต หน่วยหนึ่งในร้อยมิลลิวินาที */
  uint32_t hundredths = 125;
  if (osrsFactor(osrsT) > 0) {
    hundredths += 230UL * osrsFactor(osrsT);
  }
  if (osrsFactor(osrsP) > 0) {
    hundredths += 230UL * osrsFactor(osrsP) + 58UL;
  }
  if (osrsFactor(osrsH) > 0) {
    hundredths += 230UL * osrsFactor(osrsH) + 58UL;
  }
  g_fakeBme.measuringUntilMs = millis() + ((hundredths + 99) / 100);
}

void hostResetFakeBme280(void) {
  memset(&g_fakeBme, 0, sizeof(g_fakeBme));
  g_fakeBme.present = true;
  g_fakeBme.address = 0x77; /* บอร์ด SKU-1023 ดึง SDO ขึ้น VDDIO */
  g_fakeBme.spiCsPin = 10;
  g_fakeBme.spiExpectControl = true;
  g_fakeBme.adcT = HOST_ADC_T_EXAMPLE;
  g_fakeBme.adcP = HOST_ADC_P_EXAMPLE;
  g_fakeBme.adcH = HOST_ADC_H_EXAMPLE;

  g_fakeBme.regs[0xD0] = 0x60; /* BME280 */

  putU16(0x88, CAL_T1);
  putU16(0x8A, (uint16_t)CAL_T2);
  putU16(0x8C, (uint16_t)CAL_T3);
  putU16(0x8E, CAL_P1);
  putU16(0x90, (uint16_t)CAL_P2);
  putU16(0x92, (uint16_t)CAL_P3);
  putU16(0x94, (uint16_t)CAL_P4);
  putU16(0x96, (uint16_t)CAL_P5);
  putU16(0x98, (uint16_t)CAL_P6);
  putU16(0x9A, (uint16_t)CAL_P7);
  putU16(0x9C, (uint16_t)CAL_P8);
  putU16(0x9E, (uint16_t)CAL_P9);
  g_fakeBme.regs[0xA0] = 0x00;
  g_fakeBme.regs[0xA1] = CAL_H1;

  putU16(0xE1, (uint16_t)CAL_H2);
  g_fakeBme.regs[0xE3] = CAL_H3;
  /* dig_H4 และ dig_H5 เป็นค่า 12 บิตที่ใช้ไบต์ 0xE5 ร่วมกันคนละครึ่ง */
  g_fakeBme.regs[0xE4] = (uint8_t)((CAL_H4 >> 4) & 0xFF);
  g_fakeBme.regs[0xE5] = (uint8_t)(((CAL_H5 & 0x0F) << 4) | (CAL_H4 & 0x0F));
  g_fakeBme.regs[0xE6] = (uint8_t)((CAL_H5 >> 4) & 0xFF);
  g_fakeBme.regs[0xE7] = (uint8_t)CAL_H6;

  /* ค่าเริ่มต้นหลังเปิดเครื่องตามดาต้าชีต : ทุกอย่างเป็นศูนย์ */
  g_fakeBme.regs[0xF2] = 0x00;
  g_fakeBme.regs[0xF3] = 0x00;
  g_fakeBme.regs[0xF4] = 0x00;
  g_fakeBme.regs[0xF5] = 0x00;
  g_fakeBme.latchedOsrsH = 0;
  fakeProduceData();
  g_fakeBme.measuringUntilMs = 0;
}

void hostMakeFakeBmp280(void) {
  hostResetFakeBme280();
  g_fakeBme.regs[0xD0] = 0x58; /* BMP280 */
  /* BMP280 ไม่มีค่าชดเชยความชื้นและไม่มีช่องความชื้น */
  g_fakeBme.regs[0xA1] = 0x00;
  for (uint8_t reg = 0xE1; reg <= 0xE7; reg++) {
    g_fakeBme.regs[reg] = 0x00;
  }
  g_fakeBme.adcH = 0x8000L;
}

void hostRemoveFakeDevice(void) {
  hostResetFakeBme280();
  g_fakeBme.present = false;
}

/*! จำลองการเขียนหนึ่งรีจิสเตอร์ */
static void fakeWrite(uint8_t reg, uint8_t value) {
  g_fakeBme.writeCount++;
  if (g_fakeBme.writeLogLength < (uint8_t)sizeof(g_fakeBme.writeLog)) {
    g_fakeBme.writeLog[g_fakeBme.writeLogLength++] = reg;
  }

  if (reg == 0xE0) {
    if (value == 0xB6) {
      /* soft reset : ล้างเฉพาะรีจิสเตอร์ตั้งค่า ค่าชดเชยยังอยู่ */
      g_fakeBme.regs[0xF2] = 0x00;
      g_fakeBme.regs[0xF4] = 0x00;
      g_fakeBme.regs[0xF5] = 0x00;
      g_fakeBme.latchedOsrsH = 0;
      g_fakeBme.measuringUntilMs = 0;
      fakeProduceData();
      g_fakeBme.measuringUntilMs = 0;
    }
    return;
  }

  /* รีจิสเตอร์ที่เขียนได้มีแค่ 4 ตัวนี้ */
  if (reg != 0xF2 && reg != 0xF4 && reg != 0xF5) {
    return;
  }

  g_fakeBme.regs[reg] = value;

  if (reg == 0xF4) {
    /* กลไกสำคัญของ BME280 : ctrl_hum จะถูก latch ก็ต่อเมื่อเขียน ctrl_meas */
    g_fakeBme.latchedOsrsH = (uint8_t)(g_fakeBme.regs[0xF2] & 0x07);
    uint8_t mode = (uint8_t)(value & 0x03);
    if (mode != 0x00) {
      fakeProduceData();
      if (mode == 0x01 || mode == 0x02) {
        /* forced : วัดครั้งเดียวแล้วกลับไป sleep */
        g_fakeBme.regs[0xF4] = (uint8_t)(value & 0xFC);
      }
    }
  }
}

/*! จำลองการอ่านหนึ่งรีจิสเตอร์ */
static uint8_t fakeRead(uint8_t reg) {
  if (reg == 0xF3) {
    uint8_t status = 0;
    if (millis() < g_fakeBme.measuringUntilMs) {
      status |= 0x08; /* measuring */
    }
    return status;
  }
  return g_fakeBme.regs[reg];
}

/* ------------------------------------------------------------------ */
/* ตัวแทน TwoWire                                                       */
/* ------------------------------------------------------------------ */

TwoWire Wire;

TwoWire::TwoWire()
    : beginCount(0),
      writeCount(0),
      readCount(0),
      _address(0),
      _txLength(0),
      _rxLength(0),
      _rxIndex(0),
      _pointer(0) {
  memset(_txBuffer, 0, sizeof(_txBuffer));
  memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

void TwoWire::begin() { beginCount++; }

void TwoWire::begin(int sda, int scl) {
  (void)sda;
  (void)scl;
  beginCount++;
}

void TwoWire::setClock(uint32_t frequency) { (void)frequency; }

void TwoWire::beginTransmission(uint8_t address) {
  _address = address;
  _txLength = 0;
}

size_t TwoWire::write(uint8_t value) {
  if (_txLength < (uint8_t)sizeof(_txBuffer)) {
    _txBuffer[_txLength++] = value;
  }
  return 1;
}

uint8_t TwoWire::endTransmission(void) { return endTransmission(true); }

uint8_t TwoWire::endTransmission(bool sendStop) {
  (void)sendStop;
  writeCount++;

  if (!g_fakeBme.present || _address != g_fakeBme.address) {
    return 2; /* NACK ที่ address */
  }
  if (_txLength == 0) {
    return 0; /* แค่เช็คว่ามีตัวตน */
  }

  _pointer = _txBuffer[0];
  for (uint8_t i = 1; i < _txLength; i++) {
    fakeWrite((uint8_t)(_pointer + (i - 1)), _txBuffer[i]);
  }
  _txLength = 0;
  return 0;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length) {
  readCount++;
  _rxIndex = 0;
  _rxLength = 0;

  if (!g_fakeBme.present || address != g_fakeBme.address) {
    return 0;
  }
  if (length > (uint8_t)sizeof(_rxBuffer)) {
    length = (uint8_t)sizeof(_rxBuffer);
  }
  for (uint8_t i = 0; i < length; i++) {
    _rxBuffer[i] = fakeRead((uint8_t)(_pointer + i));
  }
  _rxLength = length;
  return length;
}

int TwoWire::available(void) { return (int)(_rxLength - _rxIndex); }

int TwoWire::read(void) {
  if (_rxIndex >= _rxLength) {
    return -1;
  }
  return (int)_rxBuffer[_rxIndex++];
}

/* ------------------------------------------------------------------ */
/* ตัวแทน SPIClass + ขา CS                                              */
/* ------------------------------------------------------------------ */

SPIClass SPI;

SPIClass::SPIClass()
    : beginCount(0), transactionCount(0), lastClock(0), lastMode(0xFF), inTransaction(false) {}

void SPIClass::begin() { beginCount++; }

void SPIClass::beginTransaction(SPISettings settings) {
  transactionCount++;
  lastClock = settings.clock;
  lastMode = settings.dataMode;
  inTransaction = true;
}

void SPIClass::endTransaction() { inTransaction = false; }

void digitalWrite(uint8_t pin, uint8_t value) {
  if (pin < 64) {
    g_pinState[pin] = value;
  }
  /* ทุกครั้งที่ CS เปลี่ยน ให้ชิปจำลองกลับไปรอ control byte ใหม่ (ดาต้าชีตหัวข้อ 6.3) */
  if (pin == g_fakeBme.spiCsPin) {
    g_fakeBme.spiExpectControl = true;
  }
}

uint8_t SPIClass::transfer(uint8_t data) {
  /* ชิปไม่ถูกเลือก หรือไม่มีชิป : MISO ลอย -> อ่านได้ 0xFF */
  if (!g_fakeBme.present || hostGetPinState(g_fakeBme.spiCsPin) != LOW) {
    return 0xFF;
  }
  if (g_fakeBme.spiExpectControl) {
    g_fakeBme.spiIsRead = (data & 0x80) != 0;
    /* รีจิสเตอร์ของ BME280 ทุกตัวอยู่ที่ 0x80 ขึ้นไป บิต 7 ของ control byte จึงเป็น
       R/W ล้วน ๆ และชิปเติมบิต 7 ของ address กลับให้เอง (เขียน 0x72 -> 0xF2) */
    g_fakeBme.spiAddress = (uint8_t)((data & 0x7F) | 0x80);
    g_fakeBme.spiExpectControl = false;
    return 0x00;
  }
  if (g_fakeBme.spiIsRead) {
    return fakeRead(g_fakeBme.spiAddress++);
  }
  fakeWrite(g_fakeBme.spiAddress, data);
  /* เขียนหลายคู่ในเฟรมเดียวได้ : คู่ถัดไปเริ่มด้วย control byte ใหม่ */
  g_fakeBme.spiExpectControl = true;
  return 0x00;
}
