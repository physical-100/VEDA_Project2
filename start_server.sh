#!/bin/bash
# start_server.sh

# 실행 파일 경로 (절대 경로로 지정하는 것이 안전합니다)
SERVER_EXE="./exec/server"
LOG_FILE="./exec/misc/device_server.log"

echo "=== 장치 제어 서버 시작 ==="

# 1. 혹시 돌고 있을지 모르는 기존 서버 종료
sudo killall server 2>/dev/null
sleep 1

# 2. 서버 실행 (sudo 권한)
echo "서버 데몬을 실행합니다..."
sudo "$SERVER_EXE"

# 3. 실행 결과 확인
sleep 1
if pgrep -x "server" > /dev/null; then
    PID=$(pgrep -x "server")
    echo "✅ 서버 시작 성공 (PID: $PID)"
    echo "📜 로그 확인: tail -f $LOG_FILE"
else
    echo "❌ 서버 시작 실패! 로그를 확인하세요."
    tail -n 20 "$LOG_FILE"
fi