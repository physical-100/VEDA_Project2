# Title: To submit 2nd project


## 주제

TCP를 이용한 원격 장치 제어 프로그램 

- 서버 : 라즈베리파이 4 
- 클라이언트 :  우분투 리눅스 

### 구현 내용
 
- 멀티 프로세스 또는 스레드를 이용한 장치 제어 
- 대상 원격 장치 : LED/부저/조도 센서 / 7세그먼트
- 장치 제어 : shared_library 형식으로 작성 
    - 장치 기능 변경 시 라이브러리만 업로드 하여 수행 가능 [동적 라이브러리 기능]
- 클라이언트 프로그램 실행 도중 강제 종료되지 않도록 시그널 처리 (int 신호에만 종료 )
- 서버는 Daemon process 형식으로 구성 
- 빌드 자동화 (make or Cmake)


### 장치별 제어 구현 기능 

- LED 
    - 클라이언트에서 on/off 및 밝기(최대/ 중간/ 최저 조절 제어)
- 부저 
    - 클라이언트에서 소리 on/off 제어
- 조도센서 
    - 클라이언트에서 조도 센서 값 확인 
    - 빛이 감지되지 않으면 LED on 
    - 빛이 감지되면 LED off
- 7세그먼트 
    - 클라이언트에서 전송한 숫자 (0~9)표시 (초 단위 시간 )
    - 1초 지날대마다 -1씩 감소 표시 
    - 0이 되면 부저 울림 


--------------------------------

## 진행 현황 (2025-12-23 기준)
- 코드 구조: `code/client`, `code/server`, `code/device_control` (장치별 .so 빌드) 완료
- 빌드: 상위 Makefile + 하위(device_control) Makefile로 계층형 빌드 구성 (`make`, `make libs`)
- 장치 라이브러리: LED/부저/7세그/조도 센서 개별 .so 생성 및 wiringPi 연동
- 서버: `dlopen/dlsym`으로 장치 .so 동적 로딩, 멀티스레드(클라이언트별)로 연결 유지 및 명령 처리
- 클라이언트: 메뉴 기반 명령 전송 (LED on/off/밝기, BUZZER on/off, SEGMENT, SENSOR)
- LED: ACTIVE LOW 회로 대응, PWM 밝기 반전 맵핑 적용
- 권한: PWM 사용 시 `sudo ./exec/server` 권장
- 남은 작업: 데몬 프로세스화, 센서 자동제어/카운트다운 고도화, 문서/캡처 정리


--------------------------------

제출 형식 

	1. 폴더 구조
		1. code/
			1. client/	//클라이언트소스들
			2. server/	//서어어어버소스들 //라이브러리소스들
			
		2. docs/
			1. manual.pdf( or .pptx )
			2. README.md
				+ 각 평가 기준항목에 따른 구현 기능 여부 내용을 명시할 것
				ie, 
					[v] 멀티 프로세스 사용
					[v] 데몬 프로세스 구현
					......
				
			3. running.txt
				1. client
					client 쪽의 실행(화면 출력 내용)을 capture 하여 포함
				

### ie,

$ ./exec/client

[ Device Control Menu ]
1. LED ON
2. LED OFF
3. Set Brightness
4. BUZZER ON (play melody)
5. BUZZER OFF (stop)
6. SENSOR ON (감시 시작)
7. SENSOR OFF (감시 종료)
8. SEGMENT DISPLAY (숫자 표시 후 카운트다운)
9. SEGMENT STOP (카운트다운 중단)
0. Exit



    Select: 1  => command exec 
	
    2. server : server 쪽의 실행(화면 출력 내용)을 capture 하여 포함
    
    3. exec/
		1. client	// client 실행 파일
		2. server	// server 실행 파일
		3. lib/  	// .so 파일들
		4. misc/    // 기타파일들 - 각종 그림이나 회로도등....
	
	2. 제출 파일 형식
		
		상기 4 개의 폴더 = { code, docs, exec, misc } 를 포함한 파일을 
		본인이름.tar [ie, 홍길동.tar] 의 파일로 만들어 제출


제출 내용
- 개발 문서 , 개발 일정,  세부 구현 내용 , 문제점 및 보완 사항 .
- 개발 소스 코드 및 빌드 방법( README 파일 작성 )
- 실행 과정 Text 파일