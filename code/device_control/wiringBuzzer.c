#include <stdio.h>
#include <wiringPi.h>
#include <softTone.h>

#include "wiringBuzzer.h"

// 핀 번호 정의 (wiringPi 번호 기준)
#define BUZZER_PIN 29   // 예: GPIO21

static int buzzer_initialized = 0;

int buzzer_init(void)
{
    if (!buzzer_initialized) {
        // wiringPiSetup()은 device_init_all()에서 호출됨
        if (softToneCreate(BUZZER_PIN) != 0) {
            fprintf(stderr, "부저 softToneCreate 실패\n");
            return -1;
        }
        buzzer_initialized = 1;
    }
    return 0;
}

int buzzer_on(void)
{
    if (buzzer_init() < 0) return -1;
    // 간단한 톤 (예: 440Hz)
    softToneWrite(BUZZER_PIN, 440);
    return 0;
}

int buzzer_off(void)
{
    if (buzzer_init() < 0) return -1;
    softToneWrite(BUZZER_PIN, 0);
    return 0;
}


