#!/usr/bin/env bash
# ============================================================================
#  แฟลชเฟิร์มแวร์ Factory Test ลงบอร์ด ESP32  (Linux)
#  Massmore BME280 (SKU-1023)
#
#  วิธีใช้
#    chmod +x flash_linux.sh
#    ./flash_linux.sh              ให้สคริปต์หาพอร์ตเอง
#    ./flash_linux.sh /dev/ttyUSB0 ระบุพอร์ตเอง
#
#  ถ้าเจอ Permission denied ให้เพิ่มตัวเองเข้ากลุ่ม dialout แล้ว logout/login
#    sudo usermod -a -G dialout $USER
# ============================================================================
set -e
cd "$(dirname "$0")"

BIN="Massmore_BME280_FactoryTest_v1.0.0_esp32dev_merged.bin"
BAUD=512000

echo "=========================================================="
echo "  Massmore BME280 (SKU-1023) - แฟลชเฟิร์มแวร์ Factory Test"
echo "=========================================================="

if command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL="esptool"
elif python3 -c "import esptool" >/dev/null 2>&1; then
  ESPTOOL="python3 -m esptool"
else
  echo "ไม่พบ esptool  ติดตั้งด้วย :  pip3 install esptool"
  exit 1
fi

PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "ไม่พบพอร์ต USB ของบอร์ด ระบุเองได้ เช่น ./flash_linux.sh /dev/ttyUSB0"
  exit 1
fi

echo "พอร์ตที่พบ : $PORT"
echo "ไฟล์       : $BIN"
echo

$ESPTOOL --chip esp32 --port "$PORT" --baud $BAUD \
  write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 "$BIN"

echo
echo "แฟลชเสร็จแล้ว เปิด Serial Monitor ที่ 115200 baud เพื่อดูผลการทดสอบ"
