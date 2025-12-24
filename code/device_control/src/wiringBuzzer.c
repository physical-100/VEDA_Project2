#include <stdio.h>
#include <wiringPi.h>
#include <softTone.h>
#include <unistd.h>

#include "../include/wiringBuzzer.h"

#define BUZZER_PIN 29

// 음계별 주파수 정의
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_G4  392
#define NOTE_C5  523
#define NOTE_A5  880
#define NOTE_E5  659

static int buzzer_initialized = 0;

int buzzer_init(void) {
    if (!buzzer_initialized) {
        if (softToneCreate(BUZZER_PIN) != 0) {
            fprintf(stderr, "부저 softToneCreate 실패\n");
            return -1;
        }
        buzzer_initialized = 1;
    }
    return 0;
}

// 1. Warning: 낮고 짧은 경고음 (삐- 삐- 삐-)
int buzzer_warning(void) {
    if (buzzer_init() < 0) return -1;
    
    for(int i = 0; i < 2; i++) {
        softToneWrite(BUZZER_PIN, 440); // 라(A4) 음
        usleep(200000);                // 0.2초 소리
        softToneWrite(BUZZER_PIN, 0);
        usleep(100000);                // 0.1초 쉼
    }
    return 0;
}

// 2. Emergency: 높은 음의 비상 사이렌 (삐용- 삐용-)
int buzzer_emergency(void) {
    if (buzzer_init() < 0) return -1;
    
    for(int i = 0; i < 3; i++) {
        softToneWrite(BUZZER_PIN, 880); // 높은 라
        usleep(300000);
        softToneWrite(BUZZER_PIN, 523); // 도
        usleep(300000);
    }
    softToneWrite(BUZZER_PIN, 0);
    return 0;
}

// 3. Success: 경쾌한 딩동댕 (도-미-솔-도)
int buzzer_success(void) {
    if (buzzer_init() < 0) return -1;
    
    int melody[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
    int duration[] = {150000, 150000, 150000, 300000};
    
    for(int i = 0; i < 4; i++) {
        softToneWrite(BUZZER_PIN, melody[i]);
        usleep(duration[i]);
    }
    softToneWrite(BUZZER_PIN, 0);
    return 0;
}

// 기존 기본 함수들
int buzzer_on(void) {
    if (buzzer_init() < 0) return -1;
    softToneWrite(BUZZER_PIN, 440);
    return 0;
}

int buzzer_off(void) {
    if (buzzer_init() < 0) return -1;
    softToneWrite(BUZZER_PIN, 0);
    return 0;
}