// 통합 장치 제어 라이브러리 헤더
// 모든 장치 제어 함수와 장치 간 상호작용 로직 포함

#ifndef DEVICE_MANAGE_H
#define DEVICE_MANAGE_H

// ===== 초기화 =====
// 모든 장치 초기화 (한 번만 호출)
int device_init_all(void);

// ===== LED 제어 =====
int led_init(void);
int led_on(void);
int led_off(void);
int led_set_brightness(int level);  // 1: 최저, 2: 중간, 3: 최대

// ===== 부저 제어 =====
int buzzer_init(void);
int buzzer_on(void);
int buzzer_off(void);

// ===== 7세그먼트 제어 =====
int segment_init(void);
int segment_display(int number);   // 0~9
int segment_countdown(int start);  // start -> 0, 0이 되면 부저 자동 울림

// ===== CDS 센서 제어 =====
int sensor_init(void);
int sensor_get_value(int *value);

#endif // DEVICE_MANAGE_H

