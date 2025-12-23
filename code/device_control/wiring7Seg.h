// 7세그먼트 제어용 헤더

#ifndef WIRING_7SEG_H
#define WIRING_7SEG_H

int segment_init(void);
int segment_display(int number);   // 0~9
int segment_countdown(int start);  // start -> 0

#endif // WIRING_7SEG_H


