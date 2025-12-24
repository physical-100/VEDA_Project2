#!/bin/bash
# stop_server.sh

echo "=== 장치 제어 서버 종료 ==="

# 1. killall 명령어로 모든 server 프로세스 종료
# -q: 프로세스가 없어도 에러 메시지를 출력하지 않음
sudo killall -q server

# 2. 종료 확인 루프 (최대 3초 대기)
for i in {1..3}
do
    if ! pgrep -x "server" > /dev/null; then
        echo "✅ 서버가 완전히 종료되었습니다."
        
        # PID 파일이 남아있다면 정리 (C 코드에서 생성하는 경우)
        sudo rm -f "/home/physical-100/Veda_Project/exec/device_server.pid"
        exit 0
    fi
    sleep 1
    echo "종료 대기 중... ($i/3)"
done

# 3. 만약 안 꺼졌다면 강제 종료
echo "⚠️ 정상 종료되지 않아 강제 종료(SIGKILL)를 수행합니다."
sudo killall -9 -q server

echo "✅ 종료 처리가 완료되었습니다."