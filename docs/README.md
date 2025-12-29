# TCP 원격 장치 제어 프로그램

## 프로젝트 개요

TCP를 이용한 원격 장치 제어 프로그램으로, 라즈베리파이 4를 서버로, 우분투 리눅스를 클라이언트로 사용합니다.

## 평가 기준 항목별 구현 기능

### ✅ 멀티 프로세스/스레드 사용
- **서버**: 멀티 스레드 구조로 구현
  - 클라이언트별 처리 스레드 (동시 접속 지원)
  - CDS 센서 모니터링 전용 스레드
  - 7세그먼트 카운트다운 전용 스레드
  - 퀴즈 처리 전용 스레드
- **클라이언트**: 멀티 스레드 구조로 구현
  - 서버 메시지 수신 전용 스레드
  - 서버 재연결 전용 스레드

### ✅ 데몬 프로세스 구현
- 서버는 `daemon(0, 0)` 함수를 사용하여 데몬 프로세스로 실행
- PID 파일 관리 (`exec/device_server.pid`)
- 로그 파일 자동 기록 (`misc/device_server.log`)
- 실행 파일 기준 절대 경로 계산으로 데몬 환경에서도 정상 동작
- `start_server.sh`, `stop_server.sh` 스크립트로 데몬 관리

### ✅ 장치 제어 (Shared Library 형식)
- **통합 라이브러리**: `exec/lib/libdevice_manage.so`
- **장치별 기능**:
  - LED: ON/OFF, 밝기 조절 (최저/중간/최대)
  - 부저: ON/OFF, 경고음/비상음/실패음/성공음
  - 7세그먼트: 숫자 표시, 카운트다운
  - 조도 센서: 값 읽기, 자동 LED 제어

### ✅ 동적 라이브러리 기능
- 서버에서 `dlopen/dlsym`을 사용하여 `libdevice_manage.so` 동적 로딩
- 장치 기능 변경 시 라이브러리만 업로드하여 수행 가능
- 실행 파일 기준 경로로 라이브러리 자동 로드

### ✅ 시그널 처리
- **클라이언트**: SIGINT(Ctrl+C)만 종료 처리, 다른 시그널(SIGTERM, SIGHUP, SIGQUIT, SIGUSR1, SIGUSR2) 무시
- **서버**: SIGINT/SIGTERM에서 정상 종료 및 리소스 정리

### ✅ 빌드 자동화
- 상위 Makefile과 하위 Makefile로 계층형 빌드 구조
- `make`: 전체 빌드
- `make libs`: 장치 라이브러리만 빌드
- `make client`: 클라이언트만 빌드
- `make server`: 서버만 빌드
- `make clean`: 정리
- `make rebuild`: 재빌드

## 프로젝트 구조

```
Veda_Project/
├── code/
│   ├── client/              # 클라이언트 소스 코드
│   │   └── client.c
│   ├── server/              # 서버 소스 코드
│   │   └── server.c
│   ├── device_control/      # 장치 제어 통합 라이브러리
│   │   ├── include/         # 헤더 파일
│   │   │   ├── device_manage.h
│   │   │   ├── wiringLED.h
│   │   │   ├── wiringBuzzer.h
│   │   │   ├── wiring7Seg.h
│   │   │   └── wiringCDS.h
│   │   ├── src/             # 소스 파일
│   │   │   ├── device_manage.c
│   │   │   ├── wiringLED.c
│   │   │   ├── wiringBuzzer.c
│   │   │   ├── wiring7Seg.c
│   │   │   └── wiringCDS.c
│   │   └── Makefile         # 장치 라이브러리 빌드 스크립트
│   └── README.md            # 코드 구조 설명
├── exec/                    # 실행 파일 및 라이브러리
│   ├── client               # 클라이언트 실행 파일
│   ├── server               # 서버 실행 파일
│   ├── device_server.pid    # 서버 PID 파일
│   └── lib/
│       └── libdevice_manage.so  # 통합 장치 제어 라이브러리
├── misc/
│   └── device_server.log        # 서버 데몬 로그 파일
├── docs/                    # 문서
├── Makefile                 # 상위 빌드 스크립트
├── start_server.sh          # 서버 시작 스크립트
├── stop_server.sh           # 서버 종료 스크립트
├── README.md                # 프로젝트 설명 (본 파일)
└── Project_Dev.md           # 개발 진행 현황

```

## 빌드 방법

```bash
# 전체 빌드 (클라이언트, 서버, 장치 제어 라이브러리)
make

# 장치 제어 라이브러리만 빌드
make libs

# 클라이언트만 빌드
make client

# 서버만 빌드
make server

# 정리
make clean

# 재빌드
make rebuild

# 빌드된 파일 확인
make check
```

## 실행 방법

### 서버 실행 (라즈베리파이 4)

```bash
# 데몬 프로세스로 시작
./start_server.sh

# 서버 상태 확인
ps aux | grep server
cat misc/device_server.log
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
# example: ./exec/client 192.168.0.43 8080

```

## 주요 기능

### 1. LED 제어
- LED ON/OFF
- 밝기 조절 (1: 최저, 2: 중간, 3: 최대)

### 2. 부저 제어
- 부저 ON/OFF
- 경고음/비상음/실패음/성공음 (퀴즈 기능에서 사용)

### 3. 조도 센서 (CDS)
- 센서 값 읽기
- 자동 LED 제어: 빛이 없으면 LED ON, 빛이 있으면 LED OFF
- 센서 값 변경 시 모든 클라이언트로 브로드캐스트

### 4. 7세그먼트 디스플레이
- 숫자 표시 (0~9)
- 카운트다운 기능: 입력한 숫자부터 1초마다 -1씩 감소
- 0이 되면 부저 울림
- 카운트다운 중단 기능

### 5. 퀴즈 기능
- 프로젝트 점수 맞추기 퀴즈
- 5초 시간 제한
- 카운트다운 중 부저 경고음/비상음
- 정답 시 성공음, 시간 초과 시 실패음
- 정답/오답/시간 초과 메시지 표시

### 6. 서버 재연결 기능
- 서버 연결이 끊어지면 자동으로 재연결 시도
- 3초마다 재연결 시도
- 재연결 성공 시 자동으로 메뉴 복귀

### 7. 입력 검증
- 숫자가 아닌 입력 시 경고 메시지
- 유효하지 않은 메뉴 번호 입력 시 경고 메시지

## 클라이언트 메뉴

```
======================================
       DEVICE CONTROL DASHBOARD       
======================================
 1. LED ON        |  6. CDS SENSOR ON
 2. LED OFF       |  7. CDS SENSOR OFF
 3. LED LEVEL ON  |  8. 7SEGMENT DISPLAY
 4. BUZZER ON     |  9. 7SEGMENT COUNTDOWN
 5. BUZZER OFF    | 10. 7SEGMENT STOP
--------------------------------------
11. QUIZ (프로젝트 점수 맞추기)
--------------------------------------
 0. Exit 프로그램 종료
======================================
Select:
```

## 기술 스택

- **언어**: C
- **라이브러리**: 
  - `wiringPi`: 라즈베리파이 GPIO 제어
  - `pthread`: 멀티 스레드
  - `dlopen/dlsym`: 동적 라이브러리 로딩
- **통신**: TCP/IP 소켓
- **프로세스 관리**: 데몬 프로세스

## 개발 환경

- **서버**: 라즈베리파이 4
- **클라이언트**: 우분투 리눅스
- **컴파일러**: GCC
- **빌드 도구**: Make

## 문제점 및 보완 사항

### 해결된 문제
1. ✅ 데몬 프로세스에서 경로 문제 해결 (실행 파일 기준 절대 경로 계산)
2. ✅ 서버 종료 시 스레드 정리 및 리소스 해제
3. ✅ 클라이언트 시그널 처리 개선 (SIGINT만 종료)
4. ✅ 서버 재연결 기능 구현
5. ✅ 입력 검증 및 오류 처리 개선

### 향후 개선 사항
- [ ] 더 많은 장치 제어 기능 추가
- [ ] 웹 인터페이스 추가
- [ ] 보안 기능 강화 (인증, 암호화)
- [ ] 설정 파일 지원

## 라이선스

본 프로젝트는 교육용 프로젝트입니다.
