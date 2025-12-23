#include <stdio.h>
#include <wiringPi.h>

#include "wiring7Seg.h"
#include "wiringBuzzer.h"

// 핀 번호 정의 (wiringPi 번호 기준)
#define SEGMENT_PIN_A  15
#define SEGMENT_PIN_B  16
#define SEGMENT_PIN_C  1
#define SEGMENT_PIN_D  4


static int segment_initialized = 0;

static int segment_pins[] = {
    SEGMENT_PIN_D, SEGMENT_PIN_C, SEGMENT_PIN_B,
    SEGMENT_PIN_A
};

// 0~9에 대한 세그먼트 패턴 (d,c,b,a)
// 1: ON, 0: OFF
static int segment_numbers[10][4] = {
    {0,0,0,0}, // 0
    {0,0,0,1}, // 1
    {0,0,1,0}, // 2
    {0,0,1,1}, // 3
    {0,1,0,0}, // 4
    {0,1,0,1}, // 5
    {0,1,1,0}, // 6
    {0,1,1,1}, // 7
    {1,0,0,0}, // 8
    {1,0,0,1} // 9
};

int segment_init(void)
{
    if (!segment_initialized) {
        if (wiringPiSetup() == -1) {
            fprintf(stderr, "wiringPi 초기화 실패 (7SEG)\n");
            return -1;
        }
        for (int i = 0; i < 4; ++i) {
            pinMode(segment_pins[i], OUTPUT);
            digitalWrite(segment_pins[i], LOW);
        }
        segment_initialized = 1;
    }
    return 0;
}

static void segment_clear(void)
{
    for (int i = 0; i < 4; ++i) {
        digitalWrite(segment_pins[i], LOW);
    }
}

int segment_display(int number)
{
    if (segment_init() < 0) return -1;
    if (number < 0 || number > 9) {
        fprintf(stderr, "세그먼트 표시 범위 초과: %d\n", number);
        return -1;
    }

    for (int i = 0; i < 4; ++i) {
        digitalWrite(segment_pins[i],
                     segment_numbers[number][i] ? HIGH : LOW);
    }
    return 0;
}

int segment_countdown(int start)
{
    if (segment_init() < 0) return -1;

    if (start < 0) start = 0;
    if (start > 9) start = 9;

    for (int n = start; n >= 0; --n) {
        segment_display(n);
        delay(1000); // 1초 대기
    }

    // // 0이 되었을 때 부저 울림
    // buzzer_on();
    // delay(500);
    // buzzer_off();

    // 표시 정리
    segment_clear();
    return 0;
}


