#!/bin/bash

# 실행 권한 확인
chmod +x server client test_octaflip

# 기존 서버 프로세스 종료
pkill -f ./server

# 서버 백그라운드로 실행
./server &
SERVER_PID=$!

echo "서버가 시작되었습니다. (PID: $SERVER_PID)"
sleep 1

# 첫 번째 클라이언트 실행
./client -ip 127.0.0.1 -port 8888 -username Player1 &
CLIENT1_PID=$!

echo "클라이언트 1이 시작되었습니다. (PID: $CLIENT1_PID)"
sleep 1

# 두 번째 클라이언트 실행
./client -ip 127.0.0.1 -port 8888 -username Player2 &
CLIENT2_PID=$!

echo "클라이언트 2가 시작되었습니다. (PID: $CLIENT2_PID)"

# 메인 프로세스가 종료되지 않도록 대기
echo "게임이 진행 중입니다. 종료하려면 Ctrl+C를 누르세요."
echo "프로세스를 모니터링하려면 다른 터미널에서 'ps aux | grep \"./server\\|./client\"'를 실행하세요."

# Ctrl+C 핸들러
trap cleanup INT

# 정리 함수
cleanup() {
    echo "프로세스를 종료합니다..."
    kill $SERVER_PID $CLIENT1_PID $CLIENT2_PID 2>/dev/null
    exit 0
}

# 무한 대기
while true; do
    sleep 1
done