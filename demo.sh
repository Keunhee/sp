#!/bin/bash

# OctaFlip 게임 데모 스크립트

echo "OctaFlip 게임 데모를 시작합니다."
echo "------------------------------"

# 컴파일 확인
if [ ! -f "./server" ] || [ ! -f "./client" ]; then
    echo "컴파일이 필요합니다."
    make clean
    make
    
    if [ $? -ne 0 ]; then
        echo "컴파일 오류 발생!"
        exit 1
    fi
fi

# 서버 시작
echo "서버를 시작합니다. (백그라운드)"
./server > server_demo.log 2>&1 &
SERVER_PID=$!
echo "서버 PID: $SERVER_PID"
sleep 1

# 서버 동작 확인
if ! ps -p $SERVER_PID > /dev/null; then
    echo "서버 시작 실패!"
    exit 1
fi

echo "서버가 포트 8888에서 실행 중입니다."
echo "------------------------------"

# 클라이언트 설명
echo "이제 두 명의 클라이언트를 실행해 게임을 시작할 수 있습니다."
echo "새 터미널을 열고 다음 명령을 실행하세요:"
echo "  ./client -ip 127.0.0.1 -port 8888 -username Player1"
echo ""
echo "또 다른 터미널에서:"
echo "  ./client -ip 127.0.0.1 -port 8888 -username Player2"
echo ""
echo "LED 매트릭스 디스플레이를 사용하려면 -led 옵션을 추가하세요:"
echo "  ./client -ip 127.0.0.1 -port 8888 -username Player1 -led"
echo "------------------------------"

# 데모 정리 방법
echo "데모를 종료하려면 Ctrl+C를 누르거나 다른 터미널에서 다음을 실행하세요:"
echo "  kill $SERVER_PID"
echo "------------------------------"

# 서버 로그 모니터링
echo "서버 로그 모니터링 중... (Ctrl+C로 종료)"
tail -f server_demo.log