// 통합 장치 제어 라이브러리
// 기존 wiringXXX.c 파일들의 함수를 재사용하고, 장치 간 상호작용을 추가

#include <stdio.h>
#include <wiringPi.h>
#include "../include/device_manage.h"

// 기존 장치 제어 헤더 포함
#include "../include/wiringLED.h"
#include "../include/wiringBuzzer.h"
#include "../include/wiring7Seg.h"
#include "../include/wiringCDS.h"

// wiringPi 초기화를 한 번만 수행하기 위한 전역 플래그
static int wiringpi_global_init = 0;

// ===== 전체 장치 초기화 =====
int device_init_all(void) {
    if (!wiringpi_global_init) {
        // wiringPi는 한 번만 초기화 (기존 파일들의 init 함수들이 중복 호출하지 않도록)
        if (wiringPiSetup() == -1) {
            fprintf(stderr, "wiringPi 전역 초기화 실패\n");
            return -1;
        }
        wiringpi_global_init = 1;
    }
    return 0;
}

// ===== LED 제어 (기존 함수 재사용) =====
// wiringLED.c의 함수들을 그대로 사용

// ===== 부저 제어 (기존 함수 재사용) =====
// wiringBuzzer.c의 함수들을 그대로 사용

// ===== 7세그먼트 제어 (기존 함수 재사용) =====
// wiring7Seg.c의 함수들을 그대로 사용 (segment_countdown은 부저 연동 포함)

// ===== CDS 센서 제어 (기존 함수 재사용) =====
// wiringCDS.c의 함수들을 그대로 사용
