#include <stdio.h>
#include <wiringPi.h>

#include "wiringCDS.h"

// 핀 번호 정의 (wiringPi 번호 기준)
#define SENSOR_PIN 14   // GPIO11 ())

static int sensor_initialized = 0;

int sensor_init(void)
{
    if (!sensor_initialized) {
        if (wiringPiSetup() == -1) {
            fprintf(stderr, "wiringPi 초기화 실패 (CDS)\n");
            return -1;
        }
        pinMode(SENSOR_PIN, INPUT);
        sensor_initialized = 1;
    }
    return 0;
}

int sensor_get_value(int *value)
{
    if (value == NULL) return -1;
    if (sensor_init() < 0) return -1;

    // 간단한 디지털 입력 예시 (아날로그 사용 시 ADC 연동 필요)
    *value = digitalRead(SENSOR_PIN);
    return 0;
}


