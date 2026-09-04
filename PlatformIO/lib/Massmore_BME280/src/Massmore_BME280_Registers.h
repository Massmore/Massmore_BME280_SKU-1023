/*!
 * @file Massmore_BME280_Registers.h
 * @brief แผนที่รีจิสเตอร์และค่าคงที่ทั้งหมดของชิป Bosch BME280
 *
 * อ้างอิงจากดาต้าชีต Bosch Sensortec BST-BME280-DS002
 *   - ตาราง 18 "Memory map"            (หัวข้อ 5.3)
 *   - ตาราง 16 "Compensation parameter storage"
 *   - หัวข้อ 4.2.3 "IIR filter"
 *   - หัวข้อ 5.4.x  รายละเอียดของแต่ละรีจิสเตอร์
 *   - หัวข้อ 9.1    เวลาที่ใช้ในการวัด
 *
 * ไฟล์นี้มีแต่ #define ล้วน ๆ ไม่มีโค้ดทำงาน แยกออกมาเพื่อให้คนที่อยากคุยกับ
 * ชิปตรง ๆ หยิบไปใช้ได้โดยไม่ต้องดึงทั้งไลบรารี
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#ifndef MASSMORE_BME280_REGISTERS_H
#define MASSMORE_BME280_REGISTERS_H

#include <stdint.h>

/* =========================================================================
   ที่อยู่บนบัส I2C
   =========================================================================
   บอร์ด Massmore BME280 (SKU-1023) ต่อขา SDO ลง GND ไว้จากโรงงาน จึงได้
   0x76 ถ้าบัดกรีจัมเปอร์ ADDR ด้านหลังบอร์ด (ADDR -> 0x77) จะย้ายไป 0x77
   ทำให้ต่อเซ็นเซอร์สองตัวบนบัสเดียวกันได้ */

#define MASSMORE_BME280_I2C_ADDR_A 0x76 /*!< SDO ลง GND (ค่าปริยายของบอร์ด) */
#define MASSMORE_BME280_I2C_ADDR_B 0x77 /*!< SDO ขึ้น VDDIO (บัดกรีจัมเปอร์ ADDR) */

/* =========================================================================
   รีจิสเตอร์
   ========================================================================= */

/* ค่าชดเชยชุดอุณหภูมิและความดัน 0x88 - 0xA1 (26 ไบต์ติดกัน) */
#define MASSMORE_BME280_REG_CALIB_TP 0x88 /*!< dig_T1 ... dig_P9 + dig_H1 */
#define MASSMORE_BME280_CALIB_TP_LEN 26   /*!< 0x88 ถึง 0xA1 */

#define MASSMORE_BME280_REG_DIG_T1 0x88 /*!< uint16 little endian */
#define MASSMORE_BME280_REG_DIG_T2 0x8A /*!< int16 */
#define MASSMORE_BME280_REG_DIG_T3 0x8C /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P1 0x8E /*!< uint16 */
#define MASSMORE_BME280_REG_DIG_P2 0x90 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P3 0x92 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P4 0x94 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P5 0x96 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P6 0x98 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P7 0x9A /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P8 0x9C /*!< int16 */
#define MASSMORE_BME280_REG_DIG_P9 0x9E /*!< int16 */
#define MASSMORE_BME280_REG_DIG_H1 0xA1 /*!< uint8 */

/*! รหัสประจำรุ่นของชิป อ่านได้ตลอดเวลาแม้อยู่ในโหมด sleep */
#define MASSMORE_BME280_REG_CHIP_ID 0xD0

/*! เขียน 0xB6 ลงรีจิสเตอร์นี้เพื่อรีเซ็ตชิป (power-on-reset) */
#define MASSMORE_BME280_REG_RESET 0xE0
#define MASSMORE_BME280_RESET_MAGIC 0xB6

/* ค่าชดเชยชุดความชื้น 0xE1 - 0xE7 (7 ไบต์ติดกัน) */
#define MASSMORE_BME280_REG_CALIB_H 0xE1 /*!< dig_H2 ... dig_H6 */
#define MASSMORE_BME280_CALIB_H_LEN 7    /*!< 0xE1 ถึง 0xE7 */

#define MASSMORE_BME280_REG_DIG_H2 0xE1 /*!< int16 */
#define MASSMORE_BME280_REG_DIG_H3 0xE3 /*!< uint8 */
#define MASSMORE_BME280_REG_DIG_H4 0xE4 /*!< int16 12 บิต แบ่งกับ 0xE5 */
#define MASSMORE_BME280_REG_DIG_H5 0xE5 /*!< int16 12 บิต แบ่งกับ 0xE6 */
#define MASSMORE_BME280_REG_DIG_H6 0xE7 /*!< int8 */

/*! ctrl_hum : ตั้ง oversampling ของความชื้น (บิต 2:0)
    ข้อควรระวังจากดาต้าชีตหัวข้อ 5.4.3 - ค่าที่เขียนลงรีจิสเตอร์นี้จะยังไม่มีผล
    จนกว่าจะมีการเขียน ctrl_meas (0xF4) ตามหลัง ไลบรารีนี้จัดลำดับให้แล้ว */
#define MASSMORE_BME280_REG_CTRL_HUM 0xF2

/*! status : บิต 3 = measuring, บิต 0 = im_update (กำลังคัดลอกค่าชดเชยจาก NVM) */
#define MASSMORE_BME280_REG_STATUS 0xF3
#define MASSMORE_BME280_STATUS_MEASURING 0x08
#define MASSMORE_BME280_STATUS_IM_UPDATE 0x01

/*! ctrl_meas : osrs_t (7:5), osrs_p (4:2), mode (1:0) */
#define MASSMORE_BME280_REG_CTRL_MEAS 0xF4

/*! config : t_sb (7:5), filter (4:2), spi3w_en (0)
    เขียนได้ผลจริงเฉพาะตอนอยู่โหมด sleep เท่านั้น (ดาต้าชีตหัวข้อ 5.4.6) */
#define MASSMORE_BME280_REG_CONFIG 0xF5

/* ข้อมูลดิบ 0xF7 - 0xFE อ่านรวดเดียว 8 ไบต์เพื่อให้ได้ค่าจากรอบวัดเดียวกัน
   (ดาต้าชีตหัวข้อ 4 "shadowing" - ค่าจะถูกล็อกไว้ตลอดช่วงที่อ่านต่อเนื่อง) */
#define MASSMORE_BME280_REG_DATA 0xF7
#define MASSMORE_BME280_DATA_LEN 8

#define MASSMORE_BME280_REG_PRESS_MSB 0xF7
#define MASSMORE_BME280_REG_PRESS_LSB 0xF8
#define MASSMORE_BME280_REG_PRESS_XLSB 0xF9
#define MASSMORE_BME280_REG_TEMP_MSB 0xFA
#define MASSMORE_BME280_REG_TEMP_LSB 0xFB
#define MASSMORE_BME280_REG_TEMP_XLSB 0xFC
#define MASSMORE_BME280_REG_HUM_MSB 0xFD
#define MASSMORE_BME280_REG_HUM_LSB 0xFE

/* =========================================================================
   รหัสประจำรุ่นของชิป (รีจิสเตอร์ 0xD0)
   =========================================================================
   ค่านี้คือด่านแรกที่ใช้แยกของแท้ออกจากของปลอมหรือของที่ติดฉลากผิดรุ่น */

#define MASSMORE_BME280_CHIP_ID_BME280 0x60 /*!< BME280 ของแท้จาก Bosch */
#define MASSMORE_BME280_CHIP_ID_BMP280 0x58 /*!< BMP280 - ไม่มีเซ็นเซอร์ความชื้น */
#define MASSMORE_BME280_CHIP_ID_BMP280_S1 0x56 /*!< BMP280 ตัวอย่างวิศวกรรม */
#define MASSMORE_BME280_CHIP_ID_BMP280_S2 0x57 /*!< BMP280 ตัวอย่างวิศวกรรม */
#define MASSMORE_BME280_CHIP_ID_BME680 0x61 /*!< BME680 - คนละรุ่น ใช้ไลบรารีนี้ไม่ได้ */

/* =========================================================================
   ค่าบิตของแต่ละฟิลด์
   ========================================================================= */

/* oversampling (ใช้ได้ทั้ง osrs_t, osrs_p, osrs_h) */
#define MASSMORE_BME280_OSRS_SKIPPED 0x00 /*!< ปิดการวัดช่องนี้ ผลลัพธ์เป็น 0x80000 */
#define MASSMORE_BME280_OSRS_X1 0x01
#define MASSMORE_BME280_OSRS_X2 0x02
#define MASSMORE_BME280_OSRS_X4 0x03
#define MASSMORE_BME280_OSRS_X8 0x04
#define MASSMORE_BME280_OSRS_X16 0x05

/* mode (ctrl_meas บิต 1:0) */
#define MASSMORE_BME280_MODE_SLEEP_BITS 0x00
#define MASSMORE_BME280_MODE_FORCED_BITS 0x01 /*!< 0x02 ก็คือ forced เหมือนกัน */
#define MASSMORE_BME280_MODE_NORMAL_BITS 0x03

/* filter (config บิต 4:2) */
#define MASSMORE_BME280_FILTER_OFF_BITS 0x00
#define MASSMORE_BME280_FILTER_2_BITS 0x01
#define MASSMORE_BME280_FILTER_4_BITS 0x02
#define MASSMORE_BME280_FILTER_8_BITS 0x03
#define MASSMORE_BME280_FILTER_16_BITS 0x04

/* t_sb - เวลาพักระหว่างรอบวัดในโหมด normal (config บิต 7:5) */
#define MASSMORE_BME280_STANDBY_0_5_BITS 0x00
#define MASSMORE_BME280_STANDBY_62_5_BITS 0x01
#define MASSMORE_BME280_STANDBY_125_BITS 0x02
#define MASSMORE_BME280_STANDBY_250_BITS 0x03
#define MASSMORE_BME280_STANDBY_500_BITS 0x04
#define MASSMORE_BME280_STANDBY_1000_BITS 0x05
#define MASSMORE_BME280_STANDBY_10_BITS 0x06
#define MASSMORE_BME280_STANDBY_20_BITS 0x07

/* =========================================================================
   ค่าอื่น ๆ ที่ใช้บ่อย
   ========================================================================= */

/*! ค่าดิบที่ชิปคืนมาเมื่อช่องนั้นถูกปิด (skipped) - อุณหภูมิและความดัน 20 บิต */
#define MASSMORE_BME280_RAW_SKIPPED_20BIT 0x80000L
/*! ค่าดิบที่ชิปคืนมาเมื่อปิดช่องความชื้น - ความชื้น 16 บิต */
#define MASSMORE_BME280_RAW_SKIPPED_16BIT 0x8000L

/*! ความดันที่ระดับน้ำทะเลตามมาตรฐาน ICAO ใช้เป็นค่าตั้งต้นในการคำนวณความสูง */
#define MASSMORE_BME280_SEALEVEL_HPA_DEFAULT 1013.25f

/*! เวลาสูงสุดที่ชิปใช้คัดลอกค่าชดเชยจาก NVM หลังรีเซ็ต (ดาต้าชีตตาราง 1) */
#define MASSMORE_BME280_STARTUP_MS 2

#endif /* MASSMORE_BME280_REGISTERS_H */
