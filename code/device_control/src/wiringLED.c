#include <stdio.h>
#include <wiringPi.h>

#include "../include/wiringLED.h"

// 핀 번호 정의 (wiringPi 번호 기준, 실제 회로에 맞게 수정 가능)
#define LED_PIN 26   // 예: GPIO12

static int led_initialized = 0;

int led_init(void)
{
    if (!led_initialized) {
        // wiringPiSetup()은 device_init_all()에서 호출됨
        pinMode(LED_PIN, PWM_OUTPUT); // PWM을 사용하여 밝기 조절
        pwmSetMode(PWM_MODE_MS);
        pwmSetRange(1024);
        pwmSetClock(32);
        led_initialized = 1;
    }
    return 0;
}

int led_on(void)
{
    if (led_init() < 0) return -1;
    // ACTIVE LOW: 0(LOW)일 때 LED ON
    pwmWrite(LED_PIN, 0);
    return 0;
}

int led_off(void)
{
    if (led_init() < 0) return -1;
    // ACTIVE LOW: 높은 PWM 값일수록 OFF에 가까움
    pwmWrite(LED_PIN, 1023);
    return 0;
}

int led_set_brightness(int level)
{
    if (led_init() < 0) return -1;

    // ACTIVE LOW 기준 밝기 맵핑 (0에 가까울수록 더 밝음)
    int value = 0;
    switch (level) {
        case 1: value = 768; break;   // 최저 (어두움)
        case 2: value = 512; break;   // 중간
        case 3: value = 0;   break;   // 최대 (가장 밝게)
        default:
            fprintf(stderr, "잘못된 밝기 레벨: %d\n", level);
            return -1;
    }
    pwmWrite(LED_PIN, value);
    return 0;
}

