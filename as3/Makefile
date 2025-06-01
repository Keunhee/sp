CC = gcc
CFLAGS := -Wall -Wextra -g -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
          -Wformat -Wformat-security -Werror=format-security -I/usr/local/include
# -L. 필요함. ORIGIN은 실행 시점의 현재 디렉토리를 rpath로 등록
LDFLAGS := -L. -Wl,-rpath,'$$ORIGIN' -lrgbmatrix

# 최종 타겟
all: client ensure_lib_links # <-- 여기에 새로운 타겟 추가

# 클라이언트 빌드 (LED 포함)
client: client.o json.o message_handler.o \
        board.o ai_engine.o winning_strategy.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 새로 추가할 부분: 필요한 라이브러리 심볼릭 링크를 생성하는 타겟
ensure_lib_links:
	@echo "Checking for librgbmatrix.so.1 link..."
	@if [ -f librgbmatrix.so ] && [ ! -f librgbmatrix.so.1 ]; then \
		echo "Creating symlink librgbmatrix.so.1 -> librgbmatrix.so"; \
		ln -sf librgbmatrix.so librgbmatrix.so.1; \
	elif [ ! -f librgbmatrix.so ]; then \
		echo "Warning: librgbmatrix.so not found. Cannot create librgbmatrix.so.1 link."; \
	else \
		echo "librgbmatrix.so.1 link already exists or librgbmatrix.so is missing."; \
	fi

# 객체 파일 빌드 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 클린 타겟
clean:
	rm -f *.o server client client_as2
	rm -f librgbmatrix.so.1 # <-- clean 시 링크도 지우도록 추가

# 실행 테스트 (LD_LIBRARY_PATH로 .so를 런타임에 인식시킴)
run_client:
	LD_LIBRARY_PATH=. ./client -ip 127.0.0.1 -port 8888 -username Player1 -led

# 종속성
client.o: client.c json.h message_handler.h board.h ai_engine.h
board.o: board.c board.h
json.o: json.c json.h
message_handler.o: message_handler.c message_handler.h json.h board.h
ai_engine.o: ai_engine.c ai_engine.h winning_strategy.h board.h
winning_strategy.o: winning_strategy.c winning_strategy.h ai_engine.h board.h

.PHONY: all clean run_client ensure_lib_links # <-- 추가된 타겟을 .PHONY에 포함