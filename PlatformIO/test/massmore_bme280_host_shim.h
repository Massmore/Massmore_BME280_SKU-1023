/*!
 * @file massmore_bme280_host_shim.h
 * @brief ตัวแทน Arduino และ Wire สำหรับรันชุดทดสอบบนเครื่อง PC
 *
 * ไฟล์นี้ไม่ได้ถูกคอมไพล์ตอนใช้งานจริงบนบอร์ด แต่ทำให้เราคอมไพล์
 * Massmore_BME280.cpp ด้วย g++ ธรรมดาแล้วทดสอบสูตรชดเชยและลำดับการ
 * เขียนรีจิสเตอร์ได้โดยไม่ต้องมีฮาร์ดแวร์
 *
 * ข้างในมีชิป BME280 จำลองที่ทำตามดาต้าชีตพอสมควร คือ
 *   - รหัสชิปที่ 0xD0
 *   - ค่าชดเชยชุดตัวอย่างจากดาต้าชีตที่ 0x88-0xA1 และ 0xE1-0xE7
 *   - soft reset ที่ 0xE0 ล้าง ctrl_meas / ctrl_hum / config
 *   - ctrl_hum มีผลก็ต่อเมื่อเขียน ctrl_meas ตามหลัง
 *   - บิต measuring ในรีจิสเตอร์ status ขึ้นลงตามเวลาจำลอง
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#ifndef MASSMORE_BME280_HOST_SHIM_H
#define MASSMORE_BME280_HOST_SHIM_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* ตัวแทนฟังก์ชันของ Arduino                                            */
/* ------------------------------------------------------------------ */

uint32_t millis(void);
void delay(uint32_t ms);

/*! เดินนาฬิกาจำลองไปข้างหน้าโดยไม่ต้องรอจริง ใช้ในชุดทดสอบ */
void hostAdvanceMillis(uint32_t ms);

/* Arduino มีมาโครนี้ ไลบรารีบางส่วนใช้ */
#ifndef F
#define F(x) (x)
#endif

/* ------------------------------------------------------------------ */
/* ตัวแทนคลาส TwoWire                                                  */
/* ------------------------------------------------------------------ */

class TwoWire {
 public:
  TwoWire();

  void begin();
  void begin(int sda, int scl);
  void setClock(uint32_t frequency);

  void beginTransmission(uint8_t address);
  size_t write(uint8_t value);
  uint8_t endTransmission(void);
  uint8_t endTransmission(bool sendStop);
  uint8_t requestFrom(uint8_t address, uint8_t length);
  int available(void);
  int read(void);

  /* ตัวนับไว้ให้ชุดทดสอบตรวจว่าเรียกบัสกี่ครั้ง */
  uint32_t beginCount;
  uint32_t writeCount;
  uint32_t readCount;

 private:
  uint8_t _address;
  uint8_t _txBuffer[8];
  uint8_t _txLength;
  uint8_t _rxBuffer[32];
  uint8_t _rxLength;
  uint8_t _rxIndex;
  uint8_t _pointer;
};

extern TwoWire Wire;

/* ------------------------------------------------------------------ */
/* ชิปจำลอง                                                             */
/* ------------------------------------------------------------------ */

/*! ค่าดิบชุดตัวอย่างจากดาต้าชีต ใช้ตรวจสูตรชดเชยว่าตรงกับที่ Bosch ระบุ */
#define HOST_ADC_T_EXAMPLE 519888L
#define HOST_ADC_P_EXAMPLE 415148L
#define HOST_ADC_H_EXAMPLE 25000L

struct HostFakeBme280 {
  bool present;      /*!< มีอุปกรณ์ตอบบนบัสหรือไม่ */
  uint8_t address;   /*!< ที่อยู่ที่ชิปจำลองตอบ */
  uint8_t regs[256]; /*!< รีจิสเตอร์ทั้งหมด */

  /* ค่าที่จะเขียนลงรีจิสเตอร์ข้อมูลตอนวัดเสร็จ */
  int32_t adcT;
  int32_t adcP;
  int32_t adcH;

  /* osrs_h ที่ถูก latch ไว้จริง (คัดลอกจาก ctrl_hum ตอนเขียน ctrl_meas) */
  uint8_t latchedOsrsH;

  uint32_t measuringUntilMs; /*!< บิต measuring จะลงเมื่อถึงเวลานี้ */
  uint32_t writeCount;       /*!< จำนวนครั้งที่ถูกเขียนรีจิสเตอร์ */
  uint8_t writeLog[64];      /*!< ลำดับรีจิสเตอร์ที่ถูกเขียน ใช้ตรวจลำดับ */
  uint8_t writeLogLength;
};

extern HostFakeBme280 g_fakeBme;

/*! ตั้งชิปจำลองกลับเป็นค่าเริ่มต้นของ BME280 ของแท้ */
void hostResetFakeBme280(void);

/*! เปลี่ยนชิปจำลองให้กลายเป็น BMP280 (ไม่มีความชื้น รหัสชิป 0x58) */
void hostMakeFakeBmp280(void);

/*! ตัดชิปออกจากบัส เพื่อทดสอบเส้นทางที่ผิดพลาด */
void hostRemoveFakeDevice(void);

#endif /* MASSMORE_BME280_HOST_SHIM_H */
