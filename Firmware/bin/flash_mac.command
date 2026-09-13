#!/usr/bin/env bash
# ============================================================================
#  แฟลชเฟิร์มแวร์ Factory Test v1.1.0 ลงบอร์ด ESP32 / ESP32-S3  (macOS)
#  Massmore BME280 (SKU-1023)
#
#  วิธีใช้
#    ดับเบิลคลิก = esp32dev            หรือในเทอร์มินัล:
#    ./flash_mac.command                    (ESP32 classic)
#    ./flash_mac.command esp32-s3-devkitc-1 (ESP32-S3)
#    ./flash_mac.command esp32dev /dev/cu.usbserial-XXXX   (ระบุพอร์ตเอง)
#    ถ้า macOS ไม่ยอมให้เปิด ให้คลิกขวา -> Open -> Open
# ============================================================================
set -e
cd "$(dirname "$0")"

BOARD="${1:-esp32dev}"
PORT="${2:-}"
BAUD=512000
VERSION=1.1.0

case "$BOARD" in
  esp32dev)            CHIP=esp32 ;;
  esp32-s3-devkitc-1)  CHIP=esp32s3 ;;
  *) echo "บอร์ดที่รองรับ : esp32dev | esp32-s3-devkitc-1"; exit 1 ;;
esac
BIN="Massmore_BME280_FactoryTest_v${VERSION}_${BOARD}_merged.bin"

echo "=========================================================="
echo "  Massmore BME280 (SKU-1023) - แฟลชเฟิร์มแวร์ Factory Test"
echo "  บอร์ด : $BOARD   ไฟล์ : $BIN"
echo "=========================================================="

if command -v esptool.py >/dev/null 2>&1; then ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then ESPTOOL="esptool"
elif python3 -c "import esptool" >/dev/null 2>&1; then ESPTOOL="python3 -m esptool"
else
  echo "ไม่พบ esptool บนเครื่องนี้  ติดตั้งด้วย :  pip3 install esptool"
  read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"; exit 1
fi

if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "ไม่พบพอร์ต USB ของบอร์ด"
  echo "  1. ใช้สาย USB ที่ส่งข้อมูลได้ ไม่ใช่สายชาร์จอย่างเดียว"
  echo "  2. ชิป CH340 อาจต้องติดตั้งไดรเวอร์ก่อน"
  echo "  3. ESP32-S3 : ถ้าไม่ขึ้นพอร์ต ให้กดค้าง BOOT แล้วกด RESET เพื่อเข้าโหมดดาวน์โหลด"
  read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"; exit 1
fi

echo "พอร์ตที่พบ : $PORT"
echo

# merged bin เขียนที่ 0x0 และมี header ระบุ flash mode/size อยู่แล้ว จึงใช้ keep
$ESPTOOL --chip $CHIP --port "$PORT" --baud $BAUD \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 "$BIN"

echo
echo "=========================================================="
echo "  แฟลชเสร็จแล้ว"
echo "  เปิด Serial Monitor ที่ 115200 baud เพื่อดูผลการทดสอบ"
echo "  บรรทัดสุดท้ายต้องเป็น  [PASS] SENSOR QA PASSED - READY TO SHIP"
echo "  พิมพ์ r แล้วกด Enter เพื่อทดสอบซ้ำ"
echo "=========================================================="
read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"
