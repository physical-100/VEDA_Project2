// LED 제어용 헤더

#ifndef WIRING_LED_H
#define WIRING_LED_H

// LED 초기화
int led_init(void);

// LED 켜기/끄기
int led_on(void);
int led_off(void);

// 밝기 설정 (1: 최저, 2: 중간, 3: 최대)
int led_set_brightness(int level);

#endif // WIRING_LED_H


