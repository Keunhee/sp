ARCH := $(shell uname -m)

ifeq ($(ARCH),x86_64)
    LIBRGBMATRIX_FILE := librgbmatrix_x64.so
else ifeq ($(ARCH),aarch64)
    LIBRGBMATRIX_FILE := librgbmatrix_arm.so
else
    $(error Unsupported architecture: $(ARCH))
endif

SYMLINK := librgbmatrix.so.1
LIBRGBMATRIX := ./$(SYMLINK)

CC = gcc
CFLAGS  := -Wall -Wextra -g -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -I/usr/local/include
LDFLAGS := -L. -Wl,-rpath=. $(LIBRGBMATRIX)

all: symlink server client

# 심볼릭 링크 생성 (컴파일 전 필요)
symlink:
	ln -sf $(LIBRGBMATRIX_FILE) $(SYMLINK)

# 서버 빌드 (LED 없음)
server: server.o octaflip.o json.o message_handler.o
	$(CC) $(CFLAGS) -o $@ $^

# 클라이언트 빌드 (LED 포함)
client: client.o octaflip.o json.o message_handler.o led_matrix.o ai_engine.o winning_strategy.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 객체 파일 빌드 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 클린 타겟
clean:
	rm -f *.o server client client_as2 $(SYMLINK)

# 실행 테스트
run_server:
	./server

run_client:
	./client -ip 127.0.0.1 -port 8888 -username Player1 -led

# 종속성
server.o: server.c octaflip.h json.h message_handler.h
client.o: client.c octaflip.h json.h message_handler.h led_matrix.h ai_engine.h
octaflip.o: octaflip.c octaflip.h
led_matrix.o: led_matrix.c led_matrix.h octaflip.h
json.o: json.c json.h
message_handler.o: message_handler.c message_handler.h json.h octaflip.h
ai_engine.o: ai_engine.c ai_engine.h winning_strategy.h octaflip.h
winning_strategy.o: winning_strategy.c winning_strategy.h ai_engine.h octaflip.h

.PHONY: all clean run_server run_client symlink