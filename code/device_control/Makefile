# device_control 하위 Makefile
# 통합 장치 제어 라이브러리(device_manage.so)를 빌드한다.
# 기존 wiringXXX.c 파일들을 모두 포함하여 빌드

CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC
INCLUDE_DIR = include
SRC_DIR = src
OUT_DIR = ../../exec/lib

# 모든 소스 파일 포함 (기존 파일들 + device_manage.c)
DEVICE_SOURCES = \
	$(SRC_DIR)/wiringLED.c \
	$(SRC_DIR)/wiringBuzzer.c \
	$(SRC_DIR)/wiring7Seg.c \
	$(SRC_DIR)/wiringCDS.c \
	$(SRC_DIR)/device_manage.c

DEVICE_MANAGE_LIB = $(OUT_DIR)/libdevice_manage.so

all: libs

libs: $(DEVICE_MANAGE_LIB)
	@echo "[device_control] 통합 장치 라이브러리 빌드 완료"

$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

$(DEVICE_MANAGE_LIB): $(DEVICE_SOURCES) | $(OUT_DIR)
	# softTone 이 별도 라이브러리인 경우와 wiringPi 안에 포함된 경우를 모두 고려
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -shared -o $@ $(DEVICE_SOURCES) -lwiringPi -lsoftTone || $(CC) $(CFLAGS) -I$(INCLUDE_DIR) -shared -o $@ $(DEVICE_SOURCES) -lwiringPi
	@echo "[device_control] 통합 장치 라이브러리 빌드 완료: $@"

clean:
	rm -f $(DEVICE_MANAGE_LIB)
	@echo "[device_control] 라이브러리 정리 완료"

.PHONY: all libs clean

