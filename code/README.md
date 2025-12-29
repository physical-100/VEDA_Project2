# TCP 원격 장치 제어 프로그램 - 코드 구조

## 프로젝트 구조

```
code/
├── client/              # 클라이언트 소스 코드
│   └── client.c        # 클라이언트 메인 소스
├── server/             # 서버 소스 코드
│   └── server.c        # 서버 메인 소스
└── device_control/     # 장치 제어 통합 라이브러리
    ├── include/        # 헤더 파일
    │   ├── device_manage.h  # 통합 장치 제어 헤더
    │   ├── wiringLED.h     # LED 제어 헤더
    │   ├── wiringBuzzer.h  # 부저 제어 헤더
    │   ├── wiring7Seg.h    # 7세그먼트 제어 헤더
    │   └── wiringCDS.h     # 조도 센서 제어 헤더
    ├── src/            # 소스 파일
    │   ├── device_manage.c  # 통합 장치 제어 구현
    │   ├── wiringLED.c      # LED 제어 구현
    │   ├── wiringBuzzer.c   # 부저 제어 구현
    │   ├── wiring7Seg.c     # 7세그먼트 제어 구현
    │   └── wiringCDS.c      # 조도 센서 제어 구현
    └── Makefile        # 장치 라이브러리 빌드 스크립트
```

## 빌드 방법

### 전체 빌드
```bash
# 프로젝트 루트에서
make
```

### 개별 빌드
```bash
# 장치 라이브러리만 빌드
make libs

# 클라이언트만 빌드
make client

# 서버만 빌드
make server
```

### 정리
```bash
make clean
```

## 장치 제어 통합 라이브러리 (`libdevice_manage.so`)

### 빌드 결과
- 위치: `exec/lib/libdevice_manage.so`
- 모든 장치 제어 함수를 하나의 동적 라이브러리로 통합

### 1. LED 제어 (`wiringLED`)
**파일**: `src/wiringLED.c`, `include/wiringLED.h`

**제공 함수**:
- `int led_init(void)` - LED 초기화
- `int led_on(void)` - LED 켜기
- `int led_off(void)` - LED 끄기
- `int led_set_brightness(int level)` - 밝기 설정 (1: 최저, 2: 중간, 3: 최대)

**특징**:
- ACTIVE LOW 회로 대응
- PWM을 이용한 밝기 제어

### 2. 부저 제어 (`wiringBuzzer`)
**파일**: `src/wiringBuzzer.c`, `include/wiringBuzzer.h`

**제공 함수**:
- `int buzzer_init(void)` - 부저 초기화
- `int buzzer_on(void)` - 부저 켜기
- `int buzzer_off(void)` - 부저 끄기
- `int buzzer_warning(void)` - 경고음 (0.2초)
- `int buzzer_emergency(void)` - 비상음 (0.2초)
- `int buzzer_success(void)` - 성공음
- `int buzzer_fail(void)` - 실패음

**특징**:
- `softTone`을 이용한 주파수 제어
- 퀴즈 기능에서 다양한 소리 패턴 사용

### 3. 7세그먼트 제어 (`wiring7Seg`)
**파일**: `src/wiring7Seg.c`, `include/wiring7Seg.h`

**제공 함수**:
- `int segment_init(void)` - 7세그먼트 초기화
- `int segment_display(int number)` - 숫자 표시 (0~9)
- `int segment_off(void)` - 7세그먼트 끄기

**특징**:
- 4자리 7세그먼트 디스플레이 지원
- 카운트다운 기능은 서버에서 스레드로 구현

### 4. 조도 센서 제어 (`wiringCDS`)
**파일**: `src/wiringCDS.c`, `include/wiringCDS.h`

**제공 함수**:
- `int sensor_init(void)` - 센서 초기화
- `int sensor_get_value(int *value)` - 센서 값 읽기 (0: 어둠, 1: 밝음)

**특징**:
- 디지털 입력 기반
- 서버에서 스레드로 지속 모니터링

### 5. 통합 관리 (`device_manage`)
**파일**: `src/device_manage.c`, `include/device_manage.h`

**제공 함수**:
- `int device_init_all(void)` - 전체 장치 초기화

**역할**:
- `wiringPiSetupSys()`를 1회 호출하여 전체 장치 공통 초기화
- 모든 장치 함수 시그니처를 한 헤더에 모아 서버에서 한 번에 `dlsym` 가능하도록 제공

## 서버 구조 (`server.c`)

### 주요 기능
1. **데몬 프로세스**
   - `daemon(0, 0)` 함수 사용
   - 실행 파일 기준 절대 경로 계산
   - PID 파일 및 로그 파일 관리

2. **동적 라이브러리 로딩**
   - `dlopen("exec/lib/libdevice_manage.so", RTLD_LAZY)`
   - `dlsym`으로 각 장치 제어 함수 심볼 로드
   - 서버 종료 시 `dlclose`로 정리

3. **멀티 스레드**
   - 클라이언트별 처리 스레드
   - CDS 센서 모니터링 스레드
   - 7세그먼트 카운트다운 스레드
   - 퀴즈 처리 스레드

4. **클라이언트 명령 처리**
   - `handle_command()` 함수에서 문자열 명령을 장치 제어 함수로 매핑
   - 명령 예시:
     - `"LED_ON"` → `led_on()`
     - `"LED_OFF"` → `led_off()`
     - `"LED_BRIGHTNESS N"` → `led_set_brightness(N)`
     - `"BUZZER_ON"` → `buzzer_on()`
     - `"BUZZER_OFF"` → `buzzer_off()`
     - `"SEGMENT_DISPLAY N"` → `segment_display(N)`
     - `"SEGMENT_COUNTDOWN N"` → 카운트다운 스레드 시작
     - `"SEGMENT_STOP"` → 카운트다운 스레드 중지
     - `"SENSOR_ON"` → CDS 센서 모니터링 스레드 시작
     - `"SENSOR_OFF"` → CDS 센서 모니터링 스레드 중지
     - `"QUIZ_START"` → 퀴즈 스레드 시작
     - `"QUIZ_ANSWER N"` → 퀴즈 답변 처리

5. **브로드캐스트 기능**
   - CDS 센서 값 변경 시 모든 클라이언트로 브로드캐스트
   - 퀴즈 결과를 모든 클라이언트로 브로드캐스트

## 클라이언트 구조 (`client.c`)

### 주요 기능
1. **TCP 클라이언트**
   - 서버에 연결
   - 명령 전송 및 응답 수신

2. **멀티 스레드**
   - 메인 스레드: 메뉴 표시 및 명령 입력
   - 서버 메시지 수신 스레드: 비동기 메시지 처리
   - 서버 재연결 스레드: 연결 끊김 시 자동 재연결

3. **시그널 처리**
   - SIGINT(Ctrl+C)만 종료 처리
   - 다른 시그널 무시

4. **서버 재연결**
   - 연결 끊김 감지 시 자동으로 재연결 스레드 시작
   - 3초마다 재연결 시도
   - 재연결 성공 시 자동으로 메뉴 복귀

5. **입력 검증**
   - 숫자가 아닌 입력 시 경고 메시지
   - 유효하지 않은 메뉴 번호 입력 시 경고 메시지

6. **메뉴 UI**
   - ANSI 컬러 지원
   - 실시간 상태 업데이트
   - 퀴즈 모드 전환

## 빌드 의존성

### 서버
- `wiringPi`: GPIO 제어
- `pthread`: 멀티 스레드
- `dl`: 동적 라이브러리 로딩

### 클라이언트
- `pthread`: 멀티 스레드

### 장치 라이브러리
- `wiringPi`: GPIO 제어
- `pthread`: 멀티 스레드 (필요 시)

## 실행 파일 위치

빌드 후 실행 파일은 다음 위치에 생성됩니다:
- 클라이언트: `exec/client`
- 서버: `exec/server`
- 장치 라이브러리: `exec/lib/libdevice_manage.so`

## 로그 파일

서버 데몬 로그는 다음 위치에 저장됩니다:
- `exec/misc/device_server.log`

## PID 파일

서버 데몬 PID 파일은 다음 위치에 저장됩니다:
- `exec/device_server.pid`
