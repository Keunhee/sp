#!/bin/bash

# 컴파일 및 정리
echo "프로젝트 컴파일 중..."
make clean
make

if [ $? -ne 0 ]; then
    echo "컴파일 오류 발생!"
    exit 1
fi

echo "컴파일 성공!"

# 단위 테스트 실행
echo -e "\n게임 로직 단위 테스트 실행 중..."
./test_octaflip > test_results.log

if [ $? -ne 0 ]; then
    echo "단위 테스트 실패!"
    exit 1
fi

echo "단위 테스트 성공!"

# 서버 시작
echo -e "\n서버 시작 중..."
./server > server.log 2>&1 &
SERVER_PID=$!
echo "서버 PID: $SERVER_PID"
sleep 2

# 게임 테스트
echo -e "\n자동 게임 테스트 시작..."
./client -ip 127.0.0.1 -port 8888 -username Player1 > client1.log 2>&1 &
CLIENT1_PID=$!
echo "클라이언트 1 PID: $CLIENT1_PID"
sleep 1

./client -ip 127.0.0.1 -port 8888 -username Player2 > client2.log 2>&1 &
CLIENT2_PID=$!
echo "클라이언트 2 PID: $CLIENT2_PID"

# 게임 진행 대기
echo -e "\n게임 진행 중... (최대 30초)"
for i in {1..30}; do
    echo -n "."
    sleep 1
    
    # 서버 및 클라이언트 프로세스 확인
    if ! ps -p $SERVER_PID > /dev/null; then
        echo -e "\n서버 프로세스가 종료되었습니다. 로그 확인..."
        break
    fi
    
    # 클라이언트 2개가 모두 종료되었는지 확인
    if ! ps -p $CLIENT1_PID > /dev/null && ! ps -p $CLIENT2_PID > /dev/null; then
        echo -e "\n클라이언트 프로세스가 모두 종료되었습니다. 게임이 완료되었을 수 있습니다."
        break
    fi
done

# 프로세스 정리
echo -e "\n테스트 정리 중..."
kill $SERVER_PID $CLIENT1_PID $CLIENT2_PID 2>/dev/null

# 로그 확인
echo -e "\n로그 요약:"
echo "서버 로그 확인:"
grep -i "error\|exception\|fail" server.log || echo "오류 없음"

echo -e "\n클라이언트 1 로그 확인:"
grep -i "error\|exception\|fail" client1.log || echo "오류 없음"

echo -e "\n클라이언트 2 로그 확인:"
grep -i "error\|exception\|fail" client2.log || echo "오류 없음"

echo -e "\n테스트 완료!"