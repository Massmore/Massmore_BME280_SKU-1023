#!/usr/bin/env bash
# ============================================================================
#  สร้างไฟล์ Massmore_BME280.zip ใหม่จากโฟลเดอร์ Massmore_BME280/
#  Massmore BME280 (SKU-1023)
#
#  ใช้ตอนไหน
#    ทุกครั้งที่แก้ซอร์สหรือตัวอย่างในโฟลเดอร์ Massmore_BME280/ แล้วอยากให้
#    ไฟล์ zip ที่แจกในรีโปตรงกับโค้ดล่าสุด
#
#  วิธีใช้
#    chmod +x make_zip.sh
#    ./make_zip.sh
#
#  สคริปต์จะตัดไฟล์ขยะออกให้เอง (.DS_Store, *.o, *~, โฟลเดอร์ที่ขึ้นต้นด้วยจุด)
#  เพราะไฟล์พวกนี้ทำให้ Arduino IDE ขึ้นเตือนตอน import
# ============================================================================
set -e
cd "$(dirname "$0")"

LIB="Massmore_BME280"
OUT="$LIB.zip"

if [ ! -f "$LIB/library.properties" ]; then
  echo "ไม่พบ $LIB/library.properties  รันสคริปต์นี้ในโฟลเดอร์ ArduinoIDE เท่านั้น"
  exit 1
fi

# ลบของเก่าทิ้งก่อน (ถ้าลบไม่ได้ก็ไม่เป็นไร python เขียนทับให้เอง)
rm -f "$OUT" 2>/dev/null || true

python3 - "$LIB" "$OUT" <<'PY'
import os
import sys
import zipfile

lib, out = sys.argv[1], sys.argv[2]
skip_names = {".DS_Store", "Thumbs.db", "desktop.ini", "__MACOSX"}
skip_ext = (".o", ".swp", "~")
count = 0

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for dirpath, dirnames, filenames in os.walk(lib):
        dirnames[:] = sorted(
            d for d in dirnames if d not in skip_names and not d.startswith(".")
        )
        for name in sorted(filenames):
            if name in skip_names or name.startswith(".") or name.endswith(skip_ext):
                continue
            z.write(os.path.join(dirpath, name))
            count += 1

print("ใส่ไฟล์ลงซิปแล้ว %d ไฟล์" % count)
PY

echo "สร้าง $OUT เสร็จแล้ว  ($(du -h "$OUT" | cut -f1))"
echo "นำไปติดตั้งที่ Arduino IDE -> Sketch -> Include Library -> Add .ZIP Library..."
