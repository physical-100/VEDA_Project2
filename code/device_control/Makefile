# device_control 하위 Makefile
# 각 장치별 wiringXXX.c를 공유 라이브러리(.so)로 빌드한다.

CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC

SRC_DIR = .
OUT_DIR = ../../exec/lib

LED_SRC    = $(SRC_DIR)/wiringLED.c
BUZZER_SRC = $(SRC_DIR)/wiringBuzzer.c
SEG7_SRC   = $(SRC_DIR)/wiring7Seg.c
CDS_SRC    = $(SRC_DIR)/wiringCDS.c

LED_LIB    = $(OUT_DIR)/libwiringLED.so
BUZZER_LIB = $(OUT_DIR)/libwiringBuzzer.so
SEG7_LIB   = $(OUT_DIR)/libwiring7Seg.so
CDS_LIB    = $(OUT_DIR)/libwiringCDS.so

all: libs

libs: $(LED_LIB) $(BUZZER_LIB) $(SEG7_LIB) $(CDS_LIB)
	@echo "[device_control] 모든 장치 라이브러리 빌드 완료"

$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

$(LED_LIB): $(LED_SRC) | $(OUT_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< -lwiringPi
	@echo "[device_control] LED 라이브러리 빌드 완료: $@"

$(BUZZER_LIB): $(BUZZER_SRC) | $(OUT_DIR)
	# softTone 이 별도 라이브러리인 경우와 wiringPi 안에 포함된 경우를 모두 고려
	$(CC) $(CFLAGS) -shared -o $@ $< -lwiringPi -lsoftTone || $(CC) $(CFLAGS) -shared -o $@ $< -lwiringPi
	@echo "[device_control] BUZZER 라이브러리 빌드 완료: $@"

$(SEG7_LIB): $(SEG7_SRC) | $(OUT_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< -lwiringPi
	@echo "[device_control] 7SEG 라이브러리 빌드 완료: $@"

$(CDS_LIB): $(CDS_SRC) | $(OUT_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< -lwiringPi
	@echo "[device_control] CDS 라이브러리 빌드 완료: $@"

clean:
	rm -f $(LED_LIB) $(BUZZER_LIB) $(SEG7_LIB) $(CDS_LIB)
	@echo "[device_control] 라이브러리 정리 완료"

.PHONY: all libs clean

