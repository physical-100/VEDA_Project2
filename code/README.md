# TCP 원격 장치 제어 프로그램

## 프로젝트 구조

```
├── code/
│ ├── client/ # 클라이언트 소스 코드
│ │ └── client.c
│ ├── server/ # 서버 소스 코드
│ │ └── server.c
│ └── device_control/ # 장치 제어 통합 라이브러리 소스 + 하위 Makefile
│ ├── include/ # 장치 제어용 헤더 모음
│ │ ├── device_manage.h # 통합 장치 제어 헤더
│ │ ├── wiringLED.h # LED 제어
│ │ ├── wiringBuzzer.h # 부저 제어
│ │ ├── wiring7Seg.h # 7세그먼트 제어
│ │ └── wiringCDS.h # 조도(CDS) 센서 제어
│ ├── src/ # 장치 제어 소스 코드
│ │ ├── device_manage.c # 통합 장치 제어 구현 (단일 .so로 빌드)
│ │ ├── wiringLED.c
│ │ ├── wiringBuzzer.c
│ │ ├── wiring7Seg.c
│ │ └── wiringCDS.c
│ └── Makefile # 장치 라이브러리 전용 빌드 스크립트
├── exec/ # 실행 파일 및 라이브러리
│ ├── client # 클라이언트 실행 파일
│ ├── server # 서버 실행 파일
│ ├── lib/
│ │ └── libdevice_manage.so # 통합 장치 제어 라이브러리
│ └── misc/
│ └── device_server.log # 서버 데몬 로그 파일
├── docs/ # 문서
├── Makefile # 상위 빌드 스크립트 (클라이언트/서버 + 하위 Make 호출)
└── README.md # 프로젝트 설명
```

## 빌드 방법 (계층형 Makefile)

```bash
# 전체 빌드 (클라이언트, 서버, 장치 제어 라이브러리)
make

# 장치 제어 라이브러리만 빌드
make libs        # 내부적으로 `make -C code/device_control libs` 호출

# 정리
make clean

# 재빌드
make rebuild

# 빌드된 실행 파일/라이브러리 확인
make check
```

### `code/device_control/Makefile`

- 역할: `wiringLED / wiringBuzzer / wiring7Seg / wiringCDS / device_manage` 를 **단일 통합 라이브러리**(`libdevice_manage.so`)로 빌드
- 주요 타겟:
  - `make -C code/device_control libs` → `exec/lib/libdevice_manage.so` 생성
  - `make -C code/device_control clean` → 통합 라이브러리 삭제

## 장치 제어 통합 라이브러리 구조 (`libdevice_manage.so`)

### 1) LED (`wiringLED`)
- 파일: `code/device_control/src/wiringLED.c`, `code/device_control/include/wiringLED.h`
- 빌드 결과: 통합 라이브러리 `exec/lib/libdevice_manage.so` 내부에 포함
- 사용 라이브러리: `wiringPi`
- 제공 함수:
  - `int led_init(void);`
  - `int led_on(void);`
  - `int led_off(void);`
  - `int led_set_brightness(int level);   // 1: 최저, 2: 중간, 3: 최대`

### 2) 부저 (`wiringBuzzer`)
- 파일: `code/device_control/src/wiringBuzzer.c`, `code/device_control/include/wiringBuzzer.h`
- 빌드 결과: 통합 라이브러리 `exec/lib/libdevice_manage.so` 내부에 포함
- 사용 라이브러리: `wiringPi` (+ 환경에 따라 `softTone` 포함)
- 제공 함수:
  - `int buzzer_init(void);`
  - `int buzzer_on(void);`
  - `int buzzer_off(void);`

### 3) 7세그먼트 (`wiring7Seg`)
- 파일: `code/device_control/src/wiring7Seg.c`, `code/device_control/include/wiring7Seg.h`
- 빌드 결과: 통합 라이브러리 `exec/lib/libdevice_manage.so` 내부에 포함
- 사용 라이브러리: `wiringPi`
- 제공 함수:
  - `int segment_init(void);`
  - `int segment_display(int number);`   // 0~9
  - `int segment_countdown(int start);`  // start → 0, 1초 간격, 0에서 부저 연동

### 4) 조도 센서 (`wiringCDS`)
- 파일: `code/device_control/src/wiringCDS.c`, `code/device_control/include/wiringCDS.h`
- 빌드 결과: 통합 라이브러리 `exec/lib/libdevice_manage.so` 내부에 포함
- 사용 라이브러리: `wiringPi`
- 제공 함수:
  - `int sensor_init(void);`
  - `int sensor_get_value(int *value);`  // 디지털 입력 기반 값 읽기

### 5) 통합 관리 (`device_manage`)
- 파일: `code/device_control/src/device_manage.c`, `code/device_control/include/device_manage.h`
- 역할:
  - `int device_init_all(void);` : `wiringPiSetupSys()`를 1회 호출해 전체 장치 공통 초기화
  - LED/BUZZER/7SEG/CDS 함수 시그니처를 한 헤더에 모아 서버에서 한 번에 `dlsym` 가능하도록 제공
  - 장치 간 상호작용(예: 7세그 카운트다운 0에서 부저 울림)을 위한 허브 역할

## 서버에서의 동적 로딩 구조


### dlopen/dlsym 기반 장치 로딩

- 서버(`server.c`)는 시작 시 다음 순서로 장치 라이브러리를 로딩합니다.
  1. `dlopen("exec/lib/libdevice_manage.so", RTLD_LAZY)` → 통합 라이브러리 핸들 로드
  2. `dlsym(handle, "device_init_all")` → 전체 장치 초기화 함수 로드
  3. `dlsym(handle, "led_*" / "buzzer_*" / "segment_*" / "sensor_*")` → 개별 장치 제어 함수 심볼 로드
- 심볼 로딩 실패 시: 필수 장치 심볼(LED/BUZZER/7SEG/CDS)이 없으면 서버 실행을 중단합니다.
- 서버 종료(SIGINT/SIGTERM 등) 시 `dlclose()` 로 통합 라이브러리 핸들을 정리합니다.
### 클라이언트 명령 ↔ 서버 장치 제어 매핑

서버의 `handle_command()` 에서 문자열 명령을 각 라이브러리 함수로 매핑합니다.

- `"LED_ON"`           → `led_on()`
- `"LED_OFF"`          → `led_off()`
- `"LED_BRIGHTNESS N"` → `led_set_brightness(N)`
- `"BUZZER_ON"`        → `buzzer_on()`
- `"BUZZER_OFF"`       → `buzzer_off()`
- `"SEGMENT_DISPLAY N"`→ `segment_display(N)`
- `"SEGMENT_STOP"`     → (간단히) `segment_display(0)`
- `"SENSOR_ON"`        → `sensor_get_value()` 로 현재 값 1회 측정 후 서버 콘솔에 출력 (stub)
- `"SENSOR_OFF"`       → 현재는 동작 없음 (추후 감시 쓰레드 종료 등 구현 예정)

## 실행 방법

### 서버 실행 (라즈베리파이 4)
```bash
./start_server.sh
```
### 서버 종료 (라즈베리파이 4)
```bash
  ./stop_server.sh
```
### 클라이언트 실행 (우분투 리눅스)
```bash
# 기본 (localhost:8080)
./exec/client

# 서버 IP 지정
./exec/client <서버_IP>

# 서버 IP와 포트 지정
./exec/client <서버_IP> <포트>
```


## 구현 상태

### 1단계: 기본 인프라 구축
- [x] 프로젝트 폴더 구조 생성
- [x] 빌드 시스템 구성 (상위 + 하위 Makefile)
- [x] TCP 서버/클라이언트 기본 통신 구현
- [x] 기본 메뉴 UI 구현
- [x] 시그널 처리 (SIGINT만 종료)

### 2단계: 하드웨어 제어 라이브러리 개발
- [x] 장치별 기능 구현 함수 작성  
- [x] Shared Library 구조 설계 (`exec/lib/libdevice_manage.so`)
- [x] LED 제어 라이브러리 (밝기 3단계 조절 포함)
- [x] 부저 제어 라이브러리
- [x] 조도 센서 라이브러리 (디지털 입력 기반 값 읽기)
- [x] 7세그먼트 라이브러리 (숫자 표시 및 카운트다운 로직)
- [x] `code/device_control/Makefile` 을 이용한 계층형 빌드 구성

### 3단계: 서버 구현
- [x] 데몬 프로세스로 변환
- [x] 멀티 프로세스/스레드 구조 -> 7segment , CDS
- [x] 동적 라이브러리 로딩 (`dlopen/dlsym` 으로 장치별 `.so` 로딩)
- [x] 클라이언트 명령 문자열과 장치 제어 함수 매핑 (기본 동작 완료)

### 4단계: 클라이언트 구현
- [x] TCP 클라이언트 소켓 구현
- [x] 메뉴 UI 구현
- [x] 시그널 처리 (SIGINT만 종료)
- [x] 클라이언트에서 CDS 센서 값 변경 시 확인 
- [x] 서버와의 통신 프로토콜 기본 완성 (명령 문자열 정의 및 송신)

### 5단계: 고급 기능 구현
- [x] 조도 센서 자동 제어 (센서 감시 쓰레드 + LED 제어)
- [x] 7세그먼트 카운트다운 기능 (segment_countdown 활용, 0에서 부저 울림)
- [x] 멀티 프로세스/스레드를 통한 동시 장치 제어

### 6단계: 테스트 및 문서화
- [ ] 실행 테스트 및 캡처
- [ ] 개발 문서 작성
- [ ] running.txt 작성

