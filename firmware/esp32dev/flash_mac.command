#!/usr/bin/env bash
# ============================================================================
#  แฟลชเฟิร์มแวร์ Factory Test ลงบอร์ด ESP32  (macOS)
#  Massmore BME280 (SKU-1023)
#
#  วิธีใช้
#    1. เสียบบอร์ด ESP32 เข้ากับคอมพิวเตอร์ด้วยสาย USB ที่ส่งข้อมูลได้
#    2. ดับเบิลคลิกไฟล์นี้
#    ถ้า macOS ไม่ยอมให้เปิด ให้คลิกขวา -> Open -> Open
#    หรือรันในเทอร์มินัล:  chmod +x flash_mac.command && ./flash_mac.command
# ============================================================================
set -e
cd "$(dirname "$0")"

BIN="Massmore_BME280_FactoryTest_v1.0.0_esp32dev_merged.bin"
BAUD=512000

echo "=========================================================="
echo "  Massmore BME280 (SKU-1023) - แฟลชเฟิร์มแวร์ Factory Test"
echo "=========================================================="

# หา esptool
if command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL="esptool"
elif python3 -c "import esptool" >/dev/null 2>&1; then
  ESPTOOL="python3 -m esptool"
else
  echo "ไม่พบ esptool บนเครื่องนี้"
  echo "ติดตั้งด้วยคำสั่ง :  pip3 install esptool"
  read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"
  exit 1
fi

# หาพอร์ต USB ของบอร์ด (CH340 / CP2102 / บอร์ดที่มี USB ในตัว)
PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
if [ -z "$PORT" ]; then
  echo "ไม่พบพอร์ต USB ของบอร์ด"
  echo "  1. ตรวจว่าสาย USB เป็นสายที่ส่งข้อมูลได้ ไม่ใช่สายชาร์จอย่างเดียว"
  echo "  2. ถ้าเป็นชิป CH340 อาจต้องติดตั้งไดรเวอร์ก่อน"
  read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"
  exit 1
fi

echo "พอร์ตที่พบ : $PORT"
echo "ไฟล์       : $BIN"
echo

$ESPTOOL --chip esp32 --port "$PORT" --baud $BAUD \
  write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 "$BIN"

echo
echo "=========================================================="
echo "  แฟลชเสร็จแล้ว"
echo "  เปิด Serial Monitor ที่ 115200 baud เพื่อดูผลการทดสอบ"
echo "  พิมพ์ r แล้วกด Enter เพื่อทดสอบซ้ำ"
echo "=========================================================="
read -n 1 -s -r -p "กด Enter เพื่อปิดหน้าต่าง"
