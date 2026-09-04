/*!
 * @file Massmore_BME280.h
 * @brief ไลบรารี Arduino / PlatformIO สำหรับเซ็นเซอร์สภาพแวดล้อม
 *        Bosch BME280 (อุณหภูมิ + ความชื้นสัมพัทธ์ + ความดันบรรยากาศ)
 *
 * บอร์ด Massmore BME280 / BMP280 Environment Sensor  SKU-1023
 * https://www.massmore.shop
 *
 * จุดเด่นของไลบรารีตัวนี้
 *   - เขียนขึ้นจากดาต้าชีต Bosch BST-BME280-DS002 โดยตรง
 *     ใช้สูตรชดเชยแบบจำนวนเต็ม (int32 / int64) ตามภาคผนวก 4.2.3 ของดาต้าชีต
 *     ไม่พึ่งไลบรารีอื่นนอกจาก Wire
 *   - ไม่ใช้ heap เลย (ไม่มี new / malloc / String ในส่วนแกน)
 *   - ครอบคลุมทุกฟังก์ชันของชิป: oversampling x1..x16 แยกรายช่อง,
 *     โหมด sleep / forced / normal, IIR filter, standby time,
 *     อ่านค่าดิบ ADC และค่าชดเชยทั้ง 32 ตัว, soft reset
 *   - มีโหมด non-blocking สำหรับงานที่ห้ามค้าง
 *   - มี verifyChip() ตรวจ 10 ข้อว่าเป็นชิป Bosch BME280 ของแท้
 *     แยก BMP280 (chip id 0x58 - ไม่มีความชื้น) ออกจาก BME280 (0x60) ได้ชัดเจน
 *
 * ตัวอย่างสั้นที่สุด
 * @code
 *   #include <Massmore_BME280.h>
 *   MassmoreBME280 bme;
 *
 *   void setup() {
 *     Serial.begin(115200);
 *     Wire.begin(21, 22);          // Qwiic ของบอร์ด Massmore ESP32
 *     bme.begin();                 // ปริยาย 0x76
 *   }
 *   void loop() {
 *     Serial.println(bme.readTemperature());
 *     delay(1000);
 *   }
 * @endcode
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#ifndef MASSMORE_BME280_H
#define MASSMORE_BME280_H

#include "Massmore_BME280_Registers.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#else
/* ใช้ตอนคอมไพล์ host test บนเครื่อง PC ไฟล์ mock อยู่ในโฟลเดอร์ test/ */
#include "massmore_bme280_host_shim.h"
#endif

/*! เวอร์ชันของไลบรารี */
#define MASSMORE_BME280_VERSION_MAJOR 1
#define MASSMORE_BME280_VERSION_MINOR 0
#define MASSMORE_BME280_VERSION_PATCH 0
#define MASSMORE_BME280_VERSION_STRING "1.0.0"

/*! ความถี่ I2C ที่แนะนำสำหรับบอร์ด Massmore (สาย Qwiic ยาวไม่เกิน 30 ซม.) */
#define MASSMORE_BME280_I2C_FREQ_DEFAULT 100000UL

/*! เวลารอสูงสุด (ms) ตอนรอผลวัดในโหมด forced */
#define MASSMORE_BME280_TIMEOUT_DEFAULT_MS 200

/* ========================================================================= */
/* ชนิดข้อมูล                                                                */
/* ========================================================================= */

/*!
 * @brief จำนวนครั้งที่ชิปวัดซ้ำแล้วเฉลี่ยภายในหนึ่งรอบ (oversampling)
 *
 * ยิ่งสูง noise ยิ่งต่ำ แต่ใช้เวลาและพลังงานมากขึ้นเป็นเชิงเส้น
 * ตั้งค่า NONE เพื่อปิดช่องนั้นทิ้งไปเลย (ประหยัดไฟ ค่าดิบจะเป็น 0x80000)
 */
typedef enum {
  MASSMORE_BME280_SAMPLING_NONE = 0, /*!< ปิดช่องนี้ ไม่วัด */
  MASSMORE_BME280_SAMPLING_X1,       /*!< วัด 1 ครั้ง */
  MASSMORE_BME280_SAMPLING_X2,       /*!< วัด 2 ครั้งแล้วเฉลี่ย */
  MASSMORE_BME280_SAMPLING_X4,       /*!< วัด 4 ครั้งแล้วเฉลี่ย */
  MASSMORE_BME280_SAMPLING_X8,       /*!< วัด 8 ครั้งแล้วเฉลี่ย */
  MASSMORE_BME280_SAMPLING_X16       /*!< วัด 16 ครั้งแล้วเฉลี่ย noise ต่ำสุด */
} massmore_bme280_sampling_t;

/*!
 * @brief โหมดการทำงานของชิป (ctrl_meas บิต 1:0)
 */
typedef enum {
  MASSMORE_BME280_MODE_SLEEP = 0, /*!< หลับ กินไฟ 0.1 uA อ่านค่าเดิมค้างไว้ได้ */
  MASSMORE_BME280_MODE_FORCED,    /*!< วัดหนึ่งครั้งแล้วกลับไป sleep เอง */
  MASSMORE_BME280_MODE_NORMAL     /*!< วัดวนต่อเนื่องโดยพักตามเวลา standby */
} massmore_bme280_mode_t;

/*!
 * @brief ค่าสัมประสิทธิ์ของฟิลเตอร์ IIR ในตัวชิป (config บิต 4:2)
 *
 * ฟิลเตอร์นี้ทำงานกับอุณหภูมิและความดันเท่านั้น ความชื้นไม่ผ่านฟิลเตอร์
 * สูตรตามดาต้าชีตหัวข้อ 3.4.4 : y[n] = (y[n-1]*(K-1) + x[n]) / K
 * ใช้กดสัญญาณรบกวนระยะสั้น เช่น ลมจากประตูที่เปิดปิด หรือแรงกดบนตัวบอร์ด
 */
typedef enum {
  MASSMORE_BME280_FILTER_OFF = 0, /*!< ปิดฟิลเตอร์ ตอบสนองไวที่สุด */
  MASSMORE_BME280_FILTER_2,       /*!< นิ่งขึ้นเล็กน้อย */
  MASSMORE_BME280_FILTER_4,
  MASSMORE_BME280_FILTER_8,
  MASSMORE_BME280_FILTER_16 /*!< นิ่งที่สุด เหมาะกับสถานีวัดอากาศ */
} massmore_bme280_filter_t;

/*!
 * @brief เวลาพักระหว่างรอบวัดในโหมด normal (config บิต 7:5)
 *
 * ยิ่งพักนาน ยิ่งกินไฟน้อย แต่ค่าที่อ่านได้ก็เก่าลงตามไปด้วย
 */
typedef enum {
  MASSMORE_BME280_STANDBY_0_5_MS = 0, /*!< 0.5 ms - เร็วสุด กินไฟมากสุด */
  MASSMORE_BME280_STANDBY_62_5_MS,
  MASSMORE_BME280_STANDBY_125_MS,
  MASSMORE_BME280_STANDBY_250_MS,
  MASSMORE_BME280_STANDBY_500_MS,
  MASSMORE_BME280_STANDBY_1000_MS, /*!< 1 วินาที */
  MASSMORE_BME280_STANDBY_10_MS,   /*!< 10 ms (ค่านี้อยู่หลัง 1000 ms ในตาราง) */
  MASSMORE_BME280_STANDBY_20_MS    /*!< 20 ms */
} massmore_bme280_standby_t;

/*!
 * @brief รหัสผลลัพธ์ของทุกฟังก์ชันที่คุยกับชิป
 *
 * ฟังก์ชันส่วนใหญ่คืน bool เพื่อให้เขียนง่าย แล้วเก็บรหัสละเอียดไว้ที่
 * lastError() ให้ไปดูตอนเกิดปัญหา
 */
typedef enum {
  MASSMORE_BME280_OK = 0,         /*!< สำเร็จ */
  MASSMORE_BME280_ERR_NOT_BEGUN,  /*!< ยังไม่ได้เรียก begin() */
  MASSMORE_BME280_ERR_NO_DEVICE,  /*!< ไม่มีอุปกรณ์ตอบที่ address นี้ */
  MASSMORE_BME280_ERR_I2C_WRITE,  /*!< เขียนลงบัสไม่สำเร็จ */
  MASSMORE_BME280_ERR_I2C_READ,   /*!< อ่านได้ไบต์ไม่ครบ */
  MASSMORE_BME280_ERR_WRONG_CHIP, /*!< chip id ไม่ใช่ 0x60 (อาจเป็น BMP280) */
  MASSMORE_BME280_ERR_TIMEOUT,    /*!< รอผลวัดเกินเวลาที่ตั้งไว้ */
  MASSMORE_BME280_ERR_NOT_READY,  /*!< ยังวัดไม่เสร็จ (โหมดไม่บล็อก) */
  MASSMORE_BME280_ERR_WRONG_MODE, /*!< เรียกผิดโหมด เช่น รอผล forced ตอนอยู่ normal */
  MASSMORE_BME280_ERR_BAD_ARG,    /*!< พารามิเตอร์ไม่ถูกต้อง */
  MASSMORE_BME280_ERR_CALIB,      /*!< ค่าชดเชยที่อ่านมาไม่สมเหตุสมผล */
  MASSMORE_BME280_ERR_NO_HUMIDITY /*!< ชิปตัวนี้ไม่มีเซ็นเซอร์ความชื้น (BMP280) */
} massmore_bme280_error_t;

/*!
 * @brief รุ่นของชิปที่ตรวจพบจากรีจิสเตอร์ 0xD0
 */
typedef enum {
  MASSMORE_BME280_CHIP_UNKNOWN = 0, /*!< รหัสที่อ่านได้ไม่ตรงกับรุ่นใดที่รู้จัก */
  MASSMORE_BME280_CHIP_BME280,      /*!< 0x60 มีครบทั้งสามค่า */
  MASSMORE_BME280_CHIP_BMP280,      /*!< 0x58 / 0x56 / 0x57 ไม่มีความชื้น */
  MASSMORE_BME280_CHIP_BME680,      /*!< 0x61 คนละตระกูล ใช้ไลบรารีนี้ไม่ได้ */
  MASSMORE_BME280_CHIP_NO_RESPONSE  /*!< ไม่มีอะไรตอบบนบัสเลย */
} massmore_bme280_chip_t;

/*!
 * @brief ผลการตรวจว่าเป็นชิป Bosch ของแท้หรือไม่
 */
typedef enum {
  MASSMORE_BME280_GENUINE_UNKNOWN = 0, /*!< ยังไม่ได้ตรวจ */
  MASSMORE_BME280_GENUINE_YES,         /*!< ผ่านครบทุกข้อ เป็น BME280 ของแท้ */
  MASSMORE_BME280_GENUINE_SUSPECT,     /*!< ผ่านเกือบหมด แต่มีบางข้อผิดปกติ */
  MASSMORE_BME280_GENUINE_NO           /*!< ไม่ผ่าน ไม่ใช่ BME280 หรือชิปเสีย */
} massmore_bme280_genuine_t;

/*!
 * @brief ค่าที่อ่านได้หนึ่งชุด จากการวัดรอบเดียวกัน
 */
typedef struct {
  float temperature; /*!< องศาเซลเซียส (รวมค่าชดเชยที่ผู้ใช้ตั้งไว้แล้ว) */
  float pressure;    /*!< เฮกโตปาสคาล hPa (= มิลลิบาร์) */
  float humidity;    /*!< เปอร์เซ็นต์ความชื้นสัมพัทธ์ %RH (NAN ถ้าเป็น BMP280) */
  float altitude;    /*!< เมตร คำนวณจาก seaLevelhPa ที่ตั้งไว้ */
  uint32_t timestamp; /*!< ค่า millis() ตอนที่อ่านสำเร็จ */
  bool valid;         /*!< true เมื่อข้อมูลชุดนี้ใช้ได้ */
} massmore_bme280_reading_t;

/*!
 * @brief ค่าดิบจาก ADC ก่อนผ่านสูตรชดเชย
 *
 * ใช้ตอนอยากทำสูตรชดเชยเอง หรือตอนตรวจสอบว่าชิปวัดจริงหรือไม่
 * ค่า 0x80000 (อุณหภูมิ/ความดัน) และ 0x8000 (ความชื้น) แปลว่าช่องนั้นถูกปิดอยู่
 */
typedef struct {
  int32_t adc_T;  /*!< 20 บิต */
  int32_t adc_P;  /*!< 20 บิต */
  int32_t adc_H;  /*!< 16 บิต */
  int32_t t_fine; /*!< ค่ากลางจากสูตรอุณหภูมิ ใช้ต่อในสูตรความดันและความชื้น */
} massmore_bme280_raw_t;

/*!
 * @brief ค่าชดเชยทั้ง 32 ตัวที่โรงงานเบิร์นไว้ใน NVM ของชิปแต่ละตัว
 *
 * ค่าชุดนี้ไม่ซ้ำกันเลยระหว่างชิปสองตัว จึงใช้เป็น "ลายนิ้วมือ" ของชิปได้
 * (ดาต้าชีตตาราง 16 หัวข้อ 4.2.2)
 */
typedef struct {
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
} massmore_bme280_calib_t;

/*!
 * @brief ผลการตรวจตัวตนของชิปแบบละเอียด 10 ข้อ
 */
typedef struct {
  bool chipIdOk;        /*!< 1. รหัสชิปเป็น 0x60 */
  bool calibTempOk;     /*!< 2. ค่าชดเชยอุณหภูมิอยู่ในช่วงที่ Bosch ใช้จริง */
  bool calibPressOk;    /*!< 3. ค่าชดเชยความดันอยู่ในช่วงที่ Bosch ใช้จริง */
  bool calibHumOk;      /*!< 4. มีค่าชดเชยความชื้น (BMP280 ที่ติดฉลากผิดจะตกข้อนี้) */
  bool calibUniqueOk;   /*!< 5. ค่าชดเชยไม่ใช่ค่าซ้ำ ๆ แบบตารางปลอม */
  bool resetOk;         /*!< 6. soft reset แล้วรีจิสเตอร์กลับเป็นค่าเริ่มต้นจริง */
  bool ctrlHumLatchOk;  /*!< 7. ctrl_hum มีผลก็ต่อเมื่อเขียน ctrl_meas ตาม (ลักษณะเฉพาะของ BME280) */
  bool registerEchoOk;  /*!< 8. เขียน config แล้วอ่านกลับได้ค่าเดิมทุกบิต */
  bool measuringBitOk;  /*!< 9. บิต measuring ขึ้นจริงระหว่างวัด แล้วลงเองเมื่อเสร็จ */
  bool humidityLiveOk;  /*!< 10. ช่องความชื้นให้ค่าดิบจริง ไม่ใช่ค่า skipped */
  uint8_t passCount;    /*!< จำนวนข้อที่ผ่าน จาก 10 */
  massmore_bme280_genuine_t verdict; /*!< สรุปผล */
} massmore_bme280_identity_t;

/* ========================================================================= */
/* คลาสหลัก                                                                  */
/* ========================================================================= */

/*!
 * @brief ตัวขับเซ็นเซอร์ Bosch BME280 ผ่านบัส I2C
 */
class MassmoreBME280 {
 public:
  MassmoreBME280();

  /* ------------------------------------------------------------------ */
  /* กลุ่มพื้นฐาน - ใช้แค่ห้าฟังก์ชันนี้ก็ได้ค่าครบแล้ว                     */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief เริ่มต้นใช้งานเซ็นเซอร์
   *
   * ลำดับที่ทำให้: ตรวจว่ามีอุปกรณ์ตอบ -> อ่าน chip id -> soft reset ->
   * รอ NVM copy เสร็จ -> อ่านค่าชดเชยทั้ง 32 ตัว -> ตั้งค่าเริ่มต้นแบบ
   * "ใช้ทั่วไป" (oversampling x1 ทุกช่อง, normal mode, filter off, standby 125 ms)
   *
   * ต้องเรียก Wire.begin(SDA, SCL) เองก่อน หรือปล่อยให้ฟังก์ชันนี้เรียกให้
   * ด้วยค่าปริยายของบอร์ดก็ได้
   *
   * @param address ที่อยู่บนบัส 0x76 (ปริยาย) หรือ 0x77
   * @param wire    ตัวชี้ไปยังบัสที่ใช้ ปริยายคือ &Wire
   * @return true เมื่อพบชิป BME280 ของแท้และอ่านค่าชดเชยได้ครบ
   */
  bool begin(uint8_t address = MASSMORE_BME280_I2C_ADDR_A, TwoWire *wire = &Wire);

  /*!
   * @brief เริ่มต้นโดยไล่หาเซ็นเซอร์เองทั้ง 0x76 และ 0x77
   * @param wire ตัวชี้ไปยังบัสที่ใช้
   * @return true เมื่อเจอที่ address ใด address หนึ่ง
   */
  bool beginAuto(TwoWire *wire = &Wire);

  /*!
   * @brief อ่านอุณหภูมิ หน่วยองศาเซลเซียส
   * @return ค่าอุณหภูมิ หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readTemperature();

  /*!
   * @brief อ่านความดันบรรยากาศ หน่วยเฮกโตปาสคาล (hPa = mbar)
   * @return ค่าความดัน หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readPressure();

  /*!
   * @brief อ่านความดันบรรยากาศ หน่วยปาสคาล (Pa)
   * @return ค่าความดัน หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readPressurePa();

  /*!
   * @brief อ่านความชื้นสัมพัทธ์ หน่วยเปอร์เซ็นต์
   * @return ค่าความชื้น หรือ NAN เมื่ออ่านไม่สำเร็จ / ชิปเป็น BMP280
   */
  float readHumidity();

  /*!
   * @brief คำนวณความสูงจากความดันที่วัดได้
   *
   * ใช้สูตรบรรยากาศมาตรฐานสากล (ดาต้าชีตหัวข้อ 3.9)
   *   h = 44330 * (1 - (p / p0) ^ (1/5.255))
   *
   * ความแม่นยำขึ้นกับ seaLevelhPa ที่ใส่เข้าไปเป็นหลัก ถ้าใช้ค่ามาตรฐาน
   * 1013.25 จะคลาดเคลื่อนได้หลายสิบเมตรตามสภาพอากาศของวันนั้น
   *
   * @param seaLevelhPa ความดันที่ระดับน้ำทะเลของพื้นที่ ณ เวลานั้น
   * @return ความสูงเป็นเมตร หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readAltitude(float seaLevelhPa = MASSMORE_BME280_SEALEVEL_HPA_DEFAULT);

  /*!
   * @brief อ่านทุกค่าในครั้งเดียว (แนะนำให้ใช้ตัวนี้)
   *
   * อ่านรีจิสเตอร์ 0xF7-0xFE รวดเดียว 8 ไบต์ จึงได้ค่าทั้งสามจากรอบวัดเดียวกันแน่นอน
   * และประหยัดเวลาบัสกว่าการเรียก readTemperature/readPressure/readHumidity แยกกันสามครั้ง
   *
   * @param out โครงสร้างรับผล
   * @return true เมื่อสำเร็จ
   */
  bool read(massmore_bme280_reading_t &out);

  /* ------------------------------------------------------------------ */
  /* กลุ่มตั้งค่าขั้นสูง                                                   */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief ตั้งค่าการวัดทั้งชุดในครั้งเดียว
   *
   * จัดลำดับการเขียนให้ถูกต้องตามดาต้าชีตให้แล้ว คือ
   * เข้า sleep -> เขียน config -> เขียน ctrl_hum -> เขียน ctrl_meas (ตัวนี้ทำให้ ctrl_hum มีผล)
   *
   * @param mode      โหมดการทำงาน
   * @param tempSampling  oversampling ของอุณหภูมิ
   * @param pressSampling oversampling ของความดัน
   * @param humSampling   oversampling ของความชื้น
   * @param filter    ค่าฟิลเตอร์ IIR
   * @param standby   เวลาพักระหว่างรอบ (มีผลเฉพาะโหมด normal)
   * @return true เมื่อเขียนสำเร็จทุกรีจิสเตอร์
   */
  bool setSampling(massmore_bme280_mode_t mode = MASSMORE_BME280_MODE_NORMAL,
                   massmore_bme280_sampling_t tempSampling = MASSMORE_BME280_SAMPLING_X1,
                   massmore_bme280_sampling_t pressSampling = MASSMORE_BME280_SAMPLING_X1,
                   massmore_bme280_sampling_t humSampling = MASSMORE_BME280_SAMPLING_X1,
                   massmore_bme280_filter_t filter = MASSMORE_BME280_FILTER_OFF,
                   massmore_bme280_standby_t standby = MASSMORE_BME280_STANDBY_125_MS);

  /*!
   * @brief ตั้งค่าสำเร็จรูปตามที่ Bosch แนะนำ - สถานีวัดอากาศ
   *
   * วัดนาน ๆ ครั้ง (1 นาที) ใช้ forced mode, oversampling x1 ทุกช่อง, ปิดฟิลเตอร์
   * กินไฟเฉลี่ยประมาณ 0.16 uA (ดาต้าชีตตาราง 7)
   */
  bool useWeatherStationPreset();

  /*!
   * @brief ตั้งค่าสำเร็จรูปตามที่ Bosch แนะนำ - วัดความชื้นในบ้าน
   *
   * forced mode, ความชื้น x1, อุณหภูมิ x1, ปิดช่องความดัน, ปิดฟิลเตอร์
   */
  bool useHumiditySensingPreset();

  /*!
   * @brief ตั้งค่าสำเร็จรูปตามที่ Bosch แนะนำ - ตรวจวัดในอาคารต่อเนื่อง
   *
   * normal mode, ความดัน x16, อุณหภูมิ x2, ความชื้น x1, filter 16, standby 0.5 ms
   * เป็นชุดที่ให้ noise ต่ำที่สุด เหมาะกับงานวัดความสูงละเอียด
   */
  bool useIndoorNavigationPreset();

  /*!
   * @brief ตั้งค่าสำเร็จรูป - เกมมิ่ง / วัดความสูงตอบสนองไว
   *
   * normal mode, ความดัน x4, อุณหภูมิ x1, ปิดความชื้น, filter 16, standby 0.5 ms
   */
  bool useGamingPreset();

  bool setMode(massmore_bme280_mode_t mode);          /*!< เปลี่ยนโหมดอย่างเดียว */
  bool setTemperatureOversampling(massmore_bme280_sampling_t s);
  bool setPressureOversampling(massmore_bme280_sampling_t s);
  bool setHumidityOversampling(massmore_bme280_sampling_t s);
  bool setFilter(massmore_bme280_filter_t filter);    /*!< เปลี่ยนฟิลเตอร์อย่างเดียว */
  bool setStandbyTime(massmore_bme280_standby_t standby);

  massmore_bme280_mode_t getMode() const { return _mode; }
  massmore_bme280_sampling_t getTemperatureOversampling() const { return _osrsT; }
  massmore_bme280_sampling_t getPressureOversampling() const { return _osrsP; }
  massmore_bme280_sampling_t getHumidityOversampling() const { return _osrsH; }
  massmore_bme280_filter_t getFilter() const { return _filter; }
  massmore_bme280_standby_t getStandbyTime() const { return _standby; }

  /*!
   * @brief สั่งวัดหนึ่งครั้งในโหมด forced แล้วรอจนเสร็จ (บล็อก)
   *
   * ใช้กับงานประหยัดไฟ ชิปจะวัดครั้งเดียวแล้วกลับไป sleep เอง
   * @return true เมื่อวัดเสร็จภายในเวลาที่กำหนด
   */
  bool takeForcedMeasurement();

  /*!
   * @brief สั่งวัดหนึ่งครั้งแล้วคืนค่าทันที ไม่รอ (ไม่บล็อก)
   *
   * คู่กับ isMeasurementReady() ใช้ในลูปที่ห้ามค้าง
   * @return true เมื่อสั่งวัดสำเร็จ
   */
  bool startForcedMeasurement();

  /*!
   * @brief ถามว่าผลวัดพร้อมหรือยัง (ไม่บล็อก)
   *
   * เช็คทั้งเวลาที่ควรใช้ตามดาต้าชีตและบิต measuring ในรีจิสเตอร์ status
   * @return true เมื่ออ่านผลได้แล้ว
   */
  bool isMeasurementReady();

  /*!
   * @brief กำลังวัดอยู่หรือไม่ (บิต 3 ของรีจิสเตอร์ status)
   */
  bool isMeasuring();

  /*!
   * @brief กำลังคัดลอกค่าชดเชยจาก NVM อยู่หรือไม่ (บิต 0 ของ status)
   */
  bool isUpdatingNVM();

  /*!
   * @brief รีเซ็ตชิปแบบซอฟต์ (เขียน 0xB6 ลง 0xE0) เทียบเท่าการถอดไฟใหม่
   *
   * หลังรีเซ็ตค่าที่ตั้งไว้ทั้งหมดจะหายกลับเป็นค่าโรงงาน ไลบรารีจะเขียนค่าที่
   * ผู้ใช้ตั้งไว้ล่าสุดกลับให้เองอัตโนมัติ
   */
  bool reset();

  /*!
   * @brief เวลาที่ชิปใช้ต่อหนึ่งรอบวัดตามค่าที่ตั้งไว้ปัจจุบัน (ค่าทั่วไป)
   *
   * สูตรจากดาต้าชีตหัวข้อ 9.1
   *   t = 1 + (2 * osrs_t) + (2 * osrs_p + 0.5) + (2 * osrs_h + 0.5)  [ms]
   * @return เวลาเป็นมิลลิวินาที (ปัดขึ้น)
   */
  uint16_t measurementTimeMs() const;

  /*!
   * @brief เวลาที่ชิปใช้ต่อหนึ่งรอบวัด กรณีแย่ที่สุดตามดาต้าชีต
   *
   *   t = 1.25 + (2.3 * osrs_t) + (2.3 * osrs_p + 0.575) + (2.3 * osrs_h + 0.575)
   */
  uint16_t measurementTimeMaxMs() const;

  /* ------------------------------------------------------------------ */
  /* กลุ่มข้อมูลดิบและค่าชดเชย                                             */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief อ่านค่าดิบจาก ADC ทั้งสามช่อง พร้อม t_fine
   * @param out โครงสร้างรับผล
   * @return true เมื่อสำเร็จ
   */
  bool readRawADC(massmore_bme280_raw_t &out);

  /*!
   * @brief คัดลอกค่าชดเชยทั้ง 32 ตัวที่อ่านไว้ตอน begin() ออกมา
   * @param out โครงสร้างรับผล
   */
  void getCalibration(massmore_bme280_calib_t &out) const { out = _calib; }

  /*!
   * @brief อ่านค่าชดเชยจากชิปใหม่อีกครั้ง
   *
   * ปกติไม่ต้องเรียกเอง begin() และ reset() เรียกให้แล้ว
   */
  bool readCalibration();

  /*!
   * @brief อ่านรีจิสเตอร์ใดก็ได้ (ไว้ตรวจสอบหรือทดลอง)
   */
  bool readRegister(uint8_t reg, uint8_t &value);

  /*!
   * @brief เขียนรีจิสเตอร์ใดก็ได้ (ระวังการใช้)
   */
  bool writeRegister(uint8_t reg, uint8_t value);

  /*!
   * @brief อ่านรีจิสเตอร์ status (0xF3) ทั้งไบต์
   */
  bool readStatus(uint8_t &status);

  /* ------------------------------------------------------------------ */
  /* กลุ่มตรวจตัวตนของชิป                                                  */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief อ่านรหัสประจำรุ่นจากรีจิสเตอร์ 0xD0
   * @return ค่าที่อ่านได้ (0x60 = BME280) หรือ 0x00 เมื่ออ่านไม่สำเร็จ
   */
  uint8_t getChipID();

  /*!
   * @brief แปลรหัสชิปเป็นรุ่นที่รู้จัก
   */
  massmore_bme280_chip_t getChipType();

  /*!
   * @brief ชิปตัวนี้มีเซ็นเซอร์ความชื้นหรือไม่ (BME280 มี, BMP280 ไม่มี)
   */
  bool hasHumidity() const { return _hasHumidity; }

  /*!
   * @brief ตรวจตัวตนของชิปแบบละเอียด 10 ข้อ
   *
   * ทำงานจริงกับชิป (มีการรีเซ็ตและสั่งวัด) จึงควรเรียกตอนเริ่มระบบ
   * ไม่ใช่ในลูปที่กำลังเก็บข้อมูลอยู่ ใช้เวลาประมาณ 300 ms
   *
   * เกณฑ์ที่ใช้ไม่ได้ดูแค่ chip id เพราะของปลอมคัดลอกค่านั้นได้ง่าย
   * แต่ดูพฤติกรรมเฉพาะตัวของซิลิคอน Bosch ด้วย เช่น กลไก latch ของ ctrl_hum
   * และช่วงค่าของสัมประสิทธิ์ชดเชยที่โรงงานใช้จริง
   *
   * @param out โครงสร้างรับผลรายข้อ (ใส่ NULL ได้ถ้าต้องการแค่ผลสรุป)
   * @return ผลสรุป
   */
  massmore_bme280_genuine_t verifyChip(massmore_bme280_identity_t *out = NULL);

  /*!
   * @brief ทางลัดของ verifyChip() คืนแค่ true / false
   */
  bool isGenuine();

  /* ------------------------------------------------------------------ */
  /* กลุ่มค่าที่คำนวณต่อ                                                   */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief คำนวณความดันที่ระดับน้ำทะเล เมื่อรู้ความสูงของจุดที่ติดตั้ง
   *
   * ใช้ปรับเทียบก่อนนำ readAltitude() ไปใช้จริง
   * @param altitudeMeters ความสูงจริงของจุดติดตั้ง (เมตร)
   * @param pressurehPa    ความดันที่วัดได้ ณ จุดนั้น
   * @return ความดันที่ระดับน้ำทะเล (hPa)
   */
  static float seaLevelForAltitude(float altitudeMeters, float pressurehPa);

  /*!
   * @brief จุดน้ำค้าง (Magnus-Tetens) หน่วยองศาเซลเซียส
   */
  static float dewPoint(float temperatureC, float humidityRH);

  /*!
   * @brief ความชื้นสัมบูรณ์ หน่วยกรัมต่อลูกบาศก์เมตร
   */
  static float absoluteHumidity(float temperatureC, float humidityRH);

  /*!
   * @brief ความดันไออิ่มตัว หน่วย hPa
   */
  static float saturationVaporPressure(float temperatureC);

  /* ------------------------------------------------------------------ */
  /* กลุ่มค่าชดเชยที่ผู้ใช้ตั้งเอง                                          */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief ตั้งค่าชดเชยอุณหภูมิ
   *
   * ใช้เมื่อบอร์ดติดตั้งใกล้แหล่งความร้อน เช่น ตัว ESP32 เอง ค่าที่ตั้งจะถูก
   * บวกเข้ากับอุณหภูมิที่อ่านได้ (ปกติใส่ค่าลบ เช่น -1.5)
   */
  void setTemperatureOffset(float offsetC) { _offsetT = offsetC; }
  void setPressureOffset(float offsethPa) { _offsetP = offsethPa; }
  void setHumidityOffset(float offsetRH) { _offsetH = offsetRH; }
  float getTemperatureOffset() const { return _offsetT; }
  float getPressureOffset() const { return _offsetP; }
  float getHumidityOffset() const { return _offsetH; }

  /*!
   * @brief ตั้งความดันที่ระดับน้ำทะเลที่ใช้ใน read() เพื่อคำนวณความสูง
   */
  void setSeaLevelPressure(float hPa) { _seaLevelhPa = hPa; }
  float getSeaLevelPressure() const { return _seaLevelhPa; }

  /* ------------------------------------------------------------------ */
  /* กลุ่มสถานะและข้อผิดพลาด                                               */
  /* ------------------------------------------------------------------ */

  massmore_bme280_error_t lastError() const { return _lastError; }
  uint8_t getAddress() const { return _address; }
  bool isConnected();

  /*! แปลงรหัสข้อผิดพลาดเป็นข้อความภาษาไทย */
  static const char *errorToString(massmore_bme280_error_t error);
  /*! แปลงรุ่นชิปเป็นข้อความ */
  static const char *chipToString(massmore_bme280_chip_t chip);
  /*! แปลงผลตรวจของแท้เป็นข้อความ */
  static const char *genuineToString(massmore_bme280_genuine_t genuine);
  /*! แปลง oversampling เป็นข้อความ เช่น "x16" */
  static const char *samplingToString(massmore_bme280_sampling_t sampling);
  /*! แปลงค่าฟิลเตอร์เป็นข้อความ เช่น "16" */
  static const char *filterToString(massmore_bme280_filter_t filter);
  /*! แปลงเวลา standby เป็นข้อความ เช่น "125 ms" */
  static const char *standbyToString(massmore_bme280_standby_t standby);
  /*! เวอร์ชันของไลบรารีเป็นข้อความ */
  static const char *getLibraryVersion() { return MASSMORE_BME280_VERSION_STRING; }

  /* ------------------------------------------------------------------ */
  /* สูตรชดเชย - เปิดเป็น public เพื่อให้ทดสอบและเรียนรู้ได้                 */
  /* ------------------------------------------------------------------ */

  /*!
   * @brief สูตรชดเชยอุณหภูมิแบบจำนวนเต็มตามดาต้าชีต
   * @param adc_T ค่าดิบ 20 บิต
   * @return อุณหภูมิหน่วย 0.01 องศาเซลเซียส (5123 = 51.23 องศา)
   *         และตั้งค่า t_fine ภายในให้ใช้ต่อ
   */
  int32_t compensateTemperature(int32_t adc_T);

  /*!
   * @brief สูตรชดเชยความดันแบบ 64 บิตตามดาต้าชีต (ต้องเรียกอุณหภูมิก่อน)
   * @param adc_P ค่าดิบ 20 บิต
   * @return ความดันในรูป Q24.8 หน่วยปาสคาล (24674867 = 24674867/256 = 96386.2 Pa)
   */
  uint32_t compensatePressure(int32_t adc_P);

  /*!
   * @brief สูตรชดเชยความชื้นแบบจำนวนเต็มตามดาต้าชีต (ต้องเรียกอุณหภูมิก่อน)
   * @param adc_H ค่าดิบ 16 บิต
   * @return ความชื้นในรูป Q22.10 หน่วย %RH (47445 = 47445/1024 = 46.333 %RH)
   */
  uint32_t compensateHumidity(int32_t adc_H);

  /*! ตั้งค่าชดเชยเอง ใช้ในชุดทดสอบบนเครื่อง PC เท่านั้น */
  void setCalibrationForTest(const massmore_bme280_calib_t &calib) { _calib = calib; }
  /*! อ่านค่า t_fine ล่าสุด */
  int32_t getTFine() const { return _tFine; }

 private:
  /* --- การสื่อสารระดับล่าง --- */
  bool writeReg(uint8_t reg, uint8_t value);
  bool readRegs(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool readReg8(uint8_t reg, uint8_t &value);
  uint16_t readU16LE(const uint8_t *buffer, uint8_t offset);
  int16_t readS16LE(const uint8_t *buffer, uint8_t offset);

  /* --- ตัวช่วยภายใน --- */
  bool applySettings(); /*!< เขียน config / ctrl_hum / ctrl_meas ตามลำดับที่ถูกต้อง */
  static uint8_t samplingToBits(massmore_bme280_sampling_t sampling);
  static uint8_t modeToBits(massmore_bme280_mode_t mode);
  static uint16_t samplingFactor(massmore_bme280_sampling_t sampling);
  bool waitForMeasurement(uint32_t timeoutMs);

  TwoWire *_wire;
  uint8_t _address;
  bool _begun;
  bool _hasHumidity;
  uint8_t _chipId;

  massmore_bme280_calib_t _calib;
  int32_t _tFine;

  massmore_bme280_mode_t _mode;
  massmore_bme280_sampling_t _osrsT;
  massmore_bme280_sampling_t _osrsP;
  massmore_bme280_sampling_t _osrsH;
  massmore_bme280_filter_t _filter;
  massmore_bme280_standby_t _standby;

  float _offsetT;
  float _offsetP;
  float _offsetH;
  float _seaLevelhPa;

  uint32_t _forcedStartMs;
  bool _forcedPending;

  massmore_bme280_error_t _lastError;
};

#endif /* MASSMORE_BME280_H */
