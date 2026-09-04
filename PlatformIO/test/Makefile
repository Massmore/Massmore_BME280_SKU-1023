# ============================================================================
#  ชุดทดสอบไลบรารี Massmore_BME280 ที่รันบนเครื่อง PC
#
#  วิธีใช้
#    cd PlatformIO/test
#    make            คอมไพล์แล้วรันทันที
#    make clean      ลบไฟล์ที่คอมไพล์ไว้
#
#  ต้องมีแค่ g++ ไม่ต้องมีบอร์ด ไม่ต้องมี PlatformIO
# ============================================================================

CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O1 -g -I. -I../lib/Massmore_BME280/src

SOURCES = test_massmore_bme280.cpp \
          massmore_bme280_host_shim.cpp \
          ../lib/Massmore_BME280/src/Massmore_BME280.cpp

TARGET = testrun

.PHONY: all run clean

all: run

$(TARGET): $(SOURCES) massmore_bme280_host_shim.h ../lib/Massmore_BME280/src/Massmore_BME280.h ../lib/Massmore_BME280/src/Massmore_BME280_Registers.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) -lm

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
