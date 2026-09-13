# เฟิร์มแวร์สำเร็จรูป — Massmore BME280 Factory Test (SKU-1023)

<p align="center">
  <img src="../Document/images/01_massmore_bme280_cover.png" alt="Massmore BME280" width="420">
</p>

ไฟล์ในโฟลเดอร์นี้คือ **ชุดทดสอบโรงงาน** (ตัวอย่าง `07_Factory_Test`) ที่คอมไพล์ไว้แล้ว
แฟลชลงบอร์ด ESP32 ได้ทันทีโดยไม่ต้องติดตั้ง Arduino IDE หรือ PlatformIO

เหมาะกับ

- ตรวจบอร์ดทีละหลายแผ่นในสายการผลิต
- ตรวจว่าบอร์ดที่เพิ่งได้รับใช้งานได้ครบทุกฟังก์ชันจริง
- แยก **BME280 (0x60)** ออกจาก **BMP280 (0x58)** ให้ชัดเจน
- ตรวจว่าชิปเป็น Bosch ของแท้หรือไม่ ด้วยการทดสอบพฤติกรรม 10 ข้อ

---

## ไฟล์ในโฟลเดอร์นี้

ไบนารีทั้งหมดอยู่ใน [`bin/`](bin/) (เวอร์ชัน **1.1.0** = ไลบรารี 1.1.0, address ปริยาย 0x77 และสแกนหา 0x76 ด้วย)

| ไฟล์ใน `bin/` | บอร์ด | offset | คำอธิบาย |
|---|---|---|---|
| `Massmore_BME280_FactoryTest_v1.1.0_esp32dev_merged.bin` | ESP32 classic | `0x0` | **ไฟล์เดียวจบ** แนะนำ |
| `Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1_merged.bin` | ESP32-S3 | `0x0` | **ไฟล์เดียวจบ** แนะนำ |
| `Massmore_BME280_FactoryTest_v1.1.0_<board>.bin` | ทั้งสอง | `0x10000` | เฉพาะแอปพลิเคชัน |
| `parts/<board>/bootloader.bin` | ทั้งสอง | ESP32 `0x1000` · S3 `0x0` | บูตโหลดเดอร์ |
| `parts/<board>/partitions.bin` | ทั้งสอง | `0x8000` | ตารางพาร์ทิชัน |
| `parts/<board>/boot_app0.bin` | ทั้งสอง | `0xe000` | ตัวเลือกพาร์ทิชัน OTA |
| `Massmore_BME280_FactoryTest_v1.0.0_esp32dev*.bin` | ESP32 classic | – | รุ่นเก่า (ไลบรารี 1.0.0) เก็บไว้อ้างอิง |
| `manifest.json` | – | – | ข้อมูลการบิลด์ทั้งสองบอร์ด ใช้กับ ESP Web Tools ได้ |
| `SHA256SUMS.txt` | – | – | ค่าแฮชไว้ตรวจว่าไฟล์ไม่เสียหาย |
| `flash_mac.command` / `flash_linux.sh` | – | – | สคริปต์แฟลช (รับชื่อบอร์ดเป็นพารามิเตอร์แรก) |

> **สถานะ v1.1.0** : บิลด์ผ่านแล้วบน PlatformIO แต่ **ยังรอผล MASSMORE HARDWARE TEST บนบอร์ดจริง**
> `// TODO: [MASSMORE_INPUT_REQUIRED: v1.1.0 hardware test result on ESP32-S3 / ESP32 / Arduino Nano]`
> สำหรับ Arduino Nano ไม่มีไบนารีสำเร็จรูป (ต้องอัปโหลดผ่าน IDE เพราะ bootloader/ฟิวส์ต่างกันตามล็อต) ใช้ `07_Factory_Test` จาก IDE ได้เลย

**ข้อมูลการบิลด์**

| หัวข้อ | ค่า |
|---|---|
| ตัวอย่างที่ใช้ | `examples/07_Factory_Test` ไลบรารี Massmore_BME280 1.1.0 |
| บอร์ด | `esp32dev` (ESP32-WROOM-32, แฟลช 4 MB) · `esp32-s3-devkitc-1` (แฟลช 8 MB, USB-CDC on boot) |
| Arduino ESP32 core | 3.3.11 (ฐาน ESP-IDF v5.x) ผ่าน pioarduino platform-espressif32 55.03.311 |
| Serial Monitor | **115200** baud |
| ขา I²C (ขาปริยายของ variant) | ESP32: SDA **GPIO 21** · SCL **GPIO 22** — ESP32-S3: SDA **GPIO 8** · SCL **GPIO 9** |
| บรรทัดสุดท้ายที่ต้องเห็น | `[PASS] SENSOR QA PASSED - READY TO SHIP` |

---

## วิธีที่ 1 — ใช้สคริปต์สำเร็จรูป (ง่ายที่สุด)

### macOS

1. เสียบบอร์ด ESP32 เข้าคอมพิวเตอร์ด้วยสาย USB ที่ **ส่งข้อมูลได้** (ไม่ใช่สายชาร์จอย่างเดียว)
2. ดับเบิลคลิก `flash_mac.command`
   ถ้า macOS ไม่ยอมเปิด ให้ **คลิกขวา → Open → Open**

หรือรันในเทอร์มินัล

```bash
cd bin
chmod +x flash_mac.command
./flash_mac.command                       # ESP32 classic (esp32dev)
./flash_mac.command esp32-s3-devkitc-1    # ESP32-S3
```

### Linux

```bash
cd bin
chmod +x flash_linux.sh
./flash_linux.sh                                # esp32dev ให้สคริปต์หาพอร์ตเอง
./flash_linux.sh esp32-s3-devkitc-1             # ESP32-S3
./flash_linux.sh esp32dev /dev/ttyUSB0          # ระบุพอร์ตเอง
```

ถ้าเจอ `Permission denied` ให้เพิ่มตัวเองเข้ากลุ่ม `dialout` แล้ว logout/login ใหม่

```bash
sudo usermod -a -G dialout $USER
```

สคริปต์ทั้งสองต้องมี `esptool` ติดตั้งไว้ก่อน

```bash
pip3 install esptool
```

---

## วิธีที่ 2 — พิมพ์คำสั่ง esptool เอง

### ไฟล์เดียวจบ (แนะนำ)

```bash
# ESP32 classic
esptool.py --chip esp32 --port <พอร์ตของคุณ> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32dev_merged.bin

# ESP32-S3
esptool.py --chip esp32s3 --port <พอร์ตของคุณ> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1_merged.bin
```

### แยกไฟล์ (เมื่ออยากเก็บพาร์ทิชันเดิมไว้)

```bash
# ESP32 classic (bootloader ที่ 0x1000)
esptool.py --chip esp32 --port <พอร์ตของคุณ> --baud 512000 \
  write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000  bin/parts/esp32dev/bootloader.bin \
  0x8000  bin/parts/esp32dev/partitions.bin \
  0xe000  bin/parts/esp32dev/boot_app0.bin \
  0x10000 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32dev.bin

# ESP32-S3 (bootloader ที่ 0x0)
esptool.py --chip esp32s3 --port <พอร์ตของคุณ> --baud 512000 \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0     bin/parts/esp32-s3-devkitc-1/bootloader.bin \
  0x8000  bin/parts/esp32-s3-devkitc-1/partitions.bin \
  0xe000  bin/parts/esp32-s3-devkitc-1/boot_app0.bin \
  0x10000 bin/Massmore_BME280_FactoryTest_v1.1.0_esp32-s3-devkitc-1.bin
```

**หาพอร์ตยังไง**

| ระบบปฏิบัติการ | คำสั่ง | ตัวอย่างชื่อพอร์ต |
|---|---|---|
| macOS | `ls /dev/cu.*` | `/dev/cu.usbserial-0001` · `/dev/cu.wchusbserial1420` |
| Linux | `ls /dev/ttyUSB* /dev/ttyACM*` | `/dev/ttyUSB0` |
| Windows | Device Manager → Ports (COM & LPT) | `COM5` |

> `--baud 512000` คือค่าที่นิ่งที่สุดบน macOS กับบอร์ด Massmore
> ถ้าเจอ `Timed out waiting for packet header` ให้ลดเป็น `460800` หรือ `115200`

---

## วิธีที่ 3 — ผ่านเว็บเบราว์เซอร์

`manifest.json` ในโฟลเดอร์นี้เตรียมไว้ให้ใช้กับ **ESP Web Tools** ได้เลย
เปิดจากเบราว์เซอร์ที่รองรับ Web Serial (Chrome หรือ Edge) แล้วแฟลชได้โดยไม่ต้องลงโปรแกรมอะไรเลย

---

## ตรวจสอบว่าไฟล์ไม่เสียหาย

```bash
cd bin
shasum -a 256 -c SHA256SUMS.txt     # macOS
sha256sum -c SHA256SUMS.txt         # Linux
```

---

## หลังแฟลชเสร็จ ต้องทำอะไรต่อ

1. ต่อบอร์ด **Massmore BME280** เข้ากับ ESP32
   เสียบสาย Qwiic เส้นเดียว หรือต่อสายเปล่า `VIN → 3V3/5V` · `GND → GND` · `SDI → SDA` · `SCK → SCL`
   (ESP32 classic: SDA GPIO 21 / SCL GPIO 22 — ESP32-S3: SDA GPIO 8 / SCL GPIO 9)

   <p align="center">
     <img src="../Document/images/03_massmore_bme280_wiring_esp32.png" alt="wiring" width="480">
   </p>

2. เปิด **Serial Monitor ที่ 115200 baud**
3. กดปุ่ม **EN / RESET** บนบอร์ด ESP32 การทดสอบจะเริ่มเองทันที
4. พิมพ์ `r` แล้วกด Enter เพื่อทดสอบซ้ำ (ใช้ตอนตรวจบอร์ดทีละหลายแผ่น)

---

## ตัวอย่างผลที่ควรเห็น

```
==========================================================
  Massmore BME280 (SKU-1023) - Factory Test
  Environment Sensor  |  Bosch BME280
  Library v1.1.0
==========================================================

--- GATE 1 : สแกนบัส I2C ---
      พบอุปกรณ์ที่ 0x77
[ OK ] 01 I2C_SCAN       พบเซ็นเซอร์ที่ 0x77 (อุปกรณ์บนบัสทั้งหมด 1 ตัว)
#RESULT,1,I2C_SCAN,PASS,พบเซ็นเซอร์ที่ 0x77 (อุปกรณ์บนบัสทั้งหมด 1 ตัว)

--- GATE 2 : ตรวจรหัสประจำรุ่น ---
[ OK ] 02 CHIP_ID        รหัส 0x60 = Bosch BME280 ถูกต้อง
[ OK ] 03 BEGIN          เริ่มต้นไลบรารีและอ่านค่าชดเชยได้ครบ

--- RUN TEST ---
[ OK ] 04 SOFT_RESET     รีเซ็ตแล้วรีจิสเตอร์ตั้งค่าทั้งสามกลับเป็น 0x00
[ OK ] 05 CALIB_READ     T1=28455 T2=26824 P1=37124 P2=-10629 H1=75 H2=364
...
[ OK ] 22 GENUINE        ผ่าน 10 จาก 10 ข้อ

[ สรุป ]
  ผ่าน 22   เตือน 0   ไม่ผ่าน 0

==========================================================
#DEVICE,0x77,BME280,0x60,GENUINE,10
#VERDICT,PASS,22,0,0

[PASS] SENSOR QA PASSED - READY TO SHIP
```

---

## บรรทัดที่ให้โปรแกรมฝั่งเว็บอ่าน

ทุกบรรทัดที่ขึ้นต้นด้วย `#` ออกแบบมาให้ parse ได้ง่าย ไม่ต้องแกะข้อความภาษาไทย

```
#RESULT,<ลำดับ>,<ชื่อหัวข้อ>,<PASS|FAIL|WARN>,<รายละเอียด>
#DEVICE,<addr>,<chip>,<chip_id>,<GENUINE|SUSPECT|FAKE|UNKNOWN>,<ผ่านกี่ข้อจาก10>
#VERDICT,<PASS|FAIL>,<ผ่าน>,<ไม่ผ่าน>,<เตือน>
```

| ฟิลด์ | ค่าที่เป็นไปได้ |
|---|---|
| `<ชื่อหัวข้อ>` | `I2C_SCAN` `CHIP_ID` `BEGIN` `SOFT_RESET` `CALIB_READ` `CALIB_RANGE` `CALIB_HUM` `REG_ECHO` `STATUS_REG` `SLEEP_MODE` `FORCED_MODE` `NORMAL_MODE` `HUM_CHANNEL` `MEAS_TIMING` `NOISE` `IIR_FILTER` `READ_VALUES` `TEMP_RANGE` `HUM_RANGE` `PRES_RANGE` `DEWPOINT` `ALTITUDE` `STABILITY` `BUS_400K` `PRESETS` `GENUINE` |
| `<chip>` | `BME280` `BMP280` `BME680` `ไม่รู้จัก` `ไม่มีการตอบสนอง` |
| `WARN` | หัวข้อที่ไม่ผ่านแต่ **ไม่ถือว่าบอร์ดเสีย** เช่นบัส 400 kHz ไม่นิ่งเพราะสายยาว |

ตัวเลข `#VERDICT` เป็น `PASS` ก็ต่อเมื่อ **ไม่มีหัวข้อไหนเป็น FAIL เลย** และผ่านทั้งสองด่านคัดกรอง

---

## แก้ปัญหา

| อาการ | วิธีแก้ |
|---|---|
| `ไม่พบพอร์ต USB ของบอร์ด` | สาย USB อาจเป็นสายชาร์จอย่างเดียว · ถ้าเป็นชิป CH340 ต้องลงไดรเวอร์ก่อน |
| `Failed to connect to ESP32` | กดปุ่ม **BOOT** ค้างไว้ตอนเริ่มแฟลช แล้วปล่อยเมื่อขึ้น `Connecting...` |
| `Timed out waiting for packet header` | ลด `--baud` เป็น `460800` หรือ `115200` |
| แฟลชผ่านแต่ Serial ไม่ขึ้นอะไร | ตรวจว่าตั้ง Serial Monitor ที่ **115200** และกดปุ่ม EN/RESET หนึ่งครั้ง |
| `I2C_SCAN` ไม่ผ่าน | ตรวจ `VIN` `GND` `SDA→21` `SCL→22` และสาย Qwiic ทั้งสองหัว |
| `CHIP_ID` ขึ้น 0x58 | บอร์ดนี้เป็น **BMP280** ซึ่งวัดความชื้นไม่ได้ ตรวจช่องติ๊กบนซิลค์สกรีน |
| `NOISE` ไม่ผ่าน | บอร์ดอาจโดนลมหรือแรงสั่นสะเทือน ลองทดสอบซ้ำในที่นิ่ง ๆ |

---

## บิลด์ใหม่เอง

```bash
cd ../PlatformIO
cp examples/07_Factory_Test/main.cpp src/main.cpp
pio run -e esp32dev                  # หรือ -e esp32-s3-devkitc-1
```

ไฟล์ผลลัพธ์จะอยู่ที่

```
PlatformIO/.pio/build/esp32dev/firmware.bin          <- แอปพลิเคชัน
PlatformIO/.pio/build/esp32dev/firmware.factory.bin  <- ไฟล์เดียวจบ (merged)
```

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
