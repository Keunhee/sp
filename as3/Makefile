CC = gcc
CFLAGS  := -Wall -Wextra -g -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -I/usr/local/include
LDFLAGS := -Wl,-rpath,'$$ORIGIN'
# .so.1 심볼릭 링크 파일을 최종 빌드 목표에 추가합니다.
all: client

# librgbmatrix.so.1 파일을 client의 의존성에 추가합니다.
client: librgbmatrix.so.1 client.o octaflip.o json.o message_handler.o led_matrix.o ai_engine.o winning_strategy.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# .so.1 심볼릭 링크를 생성하는 규칙을 추가합니다.
# 이 규칙은 librgbmatrix.so 파일이 있을 때만 동작하며,
# librgbmatrix.so.1 파일이 없으면 'ln -sf' 명령으로 링크를 생성합니다.
librgbmatrix.so.1: librgbmatrix.so
	ln -sf $< $@

# 객체 파일 빌드 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 클린 타겟 (생성된 링크 파일도 삭제하도록 추가)
clean:
	rm -f *.o server client client_as2 librgbmatrix.so.1

# 실행 테스트
run_client:
	./client -ip 127.0.0.1 -port 8888 -username Player1 -led

# 종속성
# (이하 생략, 기존과 동일)
server.o: server.c octaflip.h json.h message_handler.h
client.o: client.c octaflip.h json.h message_handler.h led_matrix.h ai_engine.h
octaflip.o: octaflip.c octaflip.h
led_matrix.o: led_matrix.c led_matrix.h octaflip.h
json.o: json.c json.h
message_handler.o: message_handler.c message_handler.h json.h octaflip.h
ai_engine.o: ai_engine.c ai_engine.h winning_strategy.h octaflip.h
winning_strategy.o: winning_strategy.c winning_strategy.h ai_engine.h octaflip.h

.PHONY: all clean run_server run_client
