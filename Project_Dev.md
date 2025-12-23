## 구현 순서

### 1단계: 기본 인프라 구축

1. 프로젝트 폴더 구조 생성 (code/client, code/server, code/device_control, docs, exec, misc)
2. 빌드 시스템 구성 (상위 Makefile)
3. TCP 서버/클라이언트 기본 통신 구현

### 2단계: 하드웨어 제어 라이브러리 개발

1. 장치별 Shared Library 구조 설계
   - `code/device_control/wiringLED.c`, `wiringLED.h`  → `exec/lib/libwiringLED.so`
   - `code/device_control/wiringBuzzer.c`, `wiringBuzzer.h` → `exec/lib/libwiringBuzzer.so`
   - `code/device_control/wiring7Seg.c`, `wiring7Seg.h` → `exec/lib/libwiring7Seg.so`
   - `code/device_control/wiringCDS.c`, `wiringCDS.h` → `exec/lib/libwiringCDS.so`
2. wiringPi를 이용한 하드웨어 연동
   - 각 모듈에서 `wiringPiSetup()` 호출 및 핀 모드 설정
3. 하위 Makefile(`code/device_control/Makefile`)을 이용한 계층적 빌드 구조 설계
   - 상위 Makefile에서 `make libs` 시 하위 Makefile을 호출하여 각 `.so` 빌드

### 3단계: 서버 구현

1. 데몬 프로세스로 변환 (예정)
2. 멀티 프로세스/스레드 구조 설계 (예정)
3. TCP 서버 소켓 구현 (완료)
4. 클라이언트 요청 처리 로직 (기본 수신/응답 완료)
5. 동적 라이브러리 로딩 및 함수 호출 (진행 예정)
   - `dlopen/dlsym` 을 이용해 `libwiringLED.so`, `libwiringBuzzer.so`, `libwiring7Seg.so`, `libwiringCDS.so` 로딩
   - 클라이언트 명령 문자열과 각 모듈 함수 매핑

### 4단계: 클라이언트 구현

1. TCP 클라이언트 소켓 구현 (완료)
2. 메뉴 UI 구현 (완료)
3. 시그널 처리 (SIGINT만 종료, 다른 시그널 무시) (완료)
4. 서버와의 통신 프로토콜 구현 (명령 문자열과 장치 제어 함수 매핑) (서버 측 구현과 함께 진행 예정)

### 5단계: 고급 기능 구현

1. 조도 센서 자동 제어 (빛 없으면 LED on, 있으면 LED off)
2. 7세그먼트 카운트다운 기능 (1초마다 -1, 0되면 부저)
3. 멀티 프로세스/스레드를 통한 동시 장치 제어

### 6단계: 테스트 및 문서화

1. 실행 테스트 및 캡처
2. README.md 작성 (구현 기능 체크리스트 포함)
3. 개발 문서 작성 (manual.pdf/pptx)
4. running.txt 작성 (클라이언트/서버 실행 화면)
