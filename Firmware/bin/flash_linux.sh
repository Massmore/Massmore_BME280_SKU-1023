#!/usr/bin/env bash
# ============================================================================
#  แฟลชเฟิร์มแวร์ Factory Test v1.1.0 ลงบอร์ด ESP32 / ESP32-S3  (Linux)
#  Massmore BME280 (SKU-1023)
#
#  ./flash_linux.sh                                  (esp32dev, หาพอร์ตเอง)
#  ./flash_linux.sh esp32-s3-devkitc-1               (ESP32-S3)
#  ./flash_linux.sh esp32dev /dev/ttyUSB0            (ระบุพอร์ตเอง)
#  ต้องมี esptool :  pip3 install esptool   และอยู่ในกลุ่ม dialout
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
else echo "ไม่พบ esptool  ติดตั้งด้วย :  pip3 install esptool"; exit 1; fi

if [ -z "$PORT" ]; then
  PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "ไม่พบพอร์ต USB ของบอร์ด ระบุเองได้ เช่น ./flash_linux.sh esp32dev /dev/ttyUSB0"; exit 1
fi

echo "พอร์ตที่พบ : $PORT"
echo

$ESPTOOL --chip $CHIP --port "$PORT" --baud $BAUD \
  write_flash -z --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 "$BIN"

echo
echo "แฟลชเสร็จแล้ว เปิด Serial Monitor ที่ 115200 baud"
echo "บรรทัดสุดท้ายต้องเป็น  [PASS] SENSOR QA PASSED - READY TO SHIP"
