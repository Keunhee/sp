#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "octaflip.h"
#include "json.h"
#include "message_handler.h"

#define SERVER_WAITING_OPPONENT 3 
#define PORT 8888
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5.0
Move move;
// 클라이언트 구조체
typedef struct {
    int socket;
    char username[64];
    char color;  // 'R' 또는 'B'
} Client;

// 서버 상태 관리
typedef enum {
    SERVER_WAITING_PLAYERS,
    SERVER_GAME_IN_PROGRESS,
    SERVER_GAME_OVER
} ServerState;

// 전역 변수
ServerState server_state = SERVER_WAITING_PLAYERS;
Client clients[MAX_CLIENTS];
int client_count = 0;
GameBoard game_board;
int current_player_idx = 0;
struct timeval turn_start_time;
typedef struct {
    char buffer[BUFFER_SIZE * 2]; // 넉넉히 여유 공간 확보
    size_t length;                // buffer에 현재 담긴 바이트 수
} ClientBuffer;
ClientBuffer client_buffers[MAX_CLIENTS];
// 함수 선언
void handle_client_message(int client_idx, char *buffer);
void handle_register_message(int client_idx, JsonValue *json_obj);
void handle_move_message(int client_idx, JsonValue *json_obj);
void handle_client_disconnect(int socket_fd);
void broadcast_game_start();
void send_your_turn(int client_idx);
void check_timeout();
void broadcast_game_over();
void log_game_state(const char *action, int player_idx, Move *move);
int set_socket_nonblocking(int socket_fd);
void cleanup_and_exit(int signal);

// 시그널 핸들러
void cleanup_and_exit(int signal __attribute__((unused))) {
    printf("\n서버 종료 중...\n");
    
    // 클라이언트 소켓 닫기
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            close(clients[i].socket);
        }
    }
    
    exit(0);
}
void process_client_buffer(int client_idx) {
    ClientBuffer *cb = &client_buffers[client_idx];
    char *newline_pos;

    // `\n`이 나올 때까지 반복
    while ((newline_pos = memchr(cb->buffer, '\n', cb->length)) != NULL) {
        size_t msg_len = newline_pos - cb->buffer;
        // 임시로 한 줄의 끝에 NULL-터미널 추가
        char tmp = cb->buffer[msg_len];
        cb->buffer[msg_len] = '\0';

        // 완전한 JSON 문자열(한 줄)을 파싱 처리
        handle_client_message(client_idx, cb->buffer);

        // 원래 값 복원
        cb->buffer[msg_len] = tmp;

        // 남은 부분을 앞으로 당기기
        size_t remaining = cb->length - (msg_len + 1);
        memmove(cb->buffer, newline_pos + 1, remaining);
        cb->length = remaining;
    }
}
void send_your_turn(int client_idx) {
    // 1) 현재 보드 상태와 타임아웃을 이용해 "your_turn" 메시지 생성
    JsonValue *turn_msg = createYourTurnMessage(&game_board, TIMEOUT_SEC);
    char *turn_str = json_stringify(turn_msg);

    // 2) 해당 클라이언트 소켓으로 JSON+개행 전송
    send(clients[client_idx].socket, turn_str, strlen(turn_str), 0);
    send(clients[client_idx].socket, "\n", 1, 0);

    // 3) 메모리 해제
    free(turn_str);
    json_free(turn_msg);

    printf("[Server] sent YOUR_TURN to %s\n", clients[client_idx].username);
}
// 비차단 소켓 설정
int set_socket_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }
    
    return 0;
}

/* 새 상태값을 하나 추가하세요. */
#define SERVER_WAITING_OPPONENT 3   /* 1명 남아 새 상대 기다리는 중 */

void handle_client_disconnect(int socket_fd)
{
    struct sockaddr_in address; socklen_t addrlen = sizeof address;
    if (getpeername(socket_fd,(struct sockaddr*)&address,&addrlen)==0) {
        printf("연결 종료, IP:%s, 포트:%d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    }

    /* 1) 끊긴 플레이어 인덱스 찾기 */
    int disconnected_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].socket == socket_fd) { disconnected_idx = i; break; }
    }
    if (disconnected_idx == -1) return;

    printf("플레이어 %s 연결 끊김\n", clients[disconnected_idx].username);

    /* 2) 게임 중인데 다른 플레이어가 살아 있으면 게임을 계속한다 */
    if (server_state == SERVER_GAME_IN_PROGRESS && client_count > 1) {

        int other_idx = (disconnected_idx + 1) % MAX_CLIENTS;

        /* a) 남아 있는 플레이어에게 '상대가 나갔음' 통보 */
        JsonValue *left = createOpponentLeftMessage(clients[disconnected_idx].username);
        char *txt = json_stringify(left);

        send(clients[other_idx].socket, txt, strlen(txt), 0);
        send(clients[other_idx].socket, "\n", 1, 0);

        free(txt); json_free(left);

        /* b) 끊긴 쪽이 현재 턴이었다면 바로 턴 넘김 */
        if (current_player_idx == disconnected_idx) {
            current_player_idx = other_idx;
            send_your_turn(current_player_idx);
        }

        /* c) 상태를 '1명 남음' 으로 표시 */
        server_state = SERVER_WAITING_OPPONENT;
    }

    /* 3) 끊긴 소켓 정리 */
    close(clients[disconnected_idx].socket);
    clients[disconnected_idx].socket = -1;
    memset(clients[disconnected_idx].username, 0,
           sizeof clients[disconnected_idx].username);
    client_count--;

    /* 4) 모두 끊겼으면 서버 종료 */
    if (client_count == 0) {
        printf("[Server] 모든 플레이어 연결 종료 → 서버 종료\n");
        cleanup_and_exit(0);
    }
}


void check_timeout() {
    if (server_state != SERVER_GAME_IN_PROGRESS) return;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    double elapsed = (current_time.tv_sec - turn_start_time.tv_sec) +
                     (current_time.tv_usec - turn_start_time.tv_usec) / 1000000.0;

    if (elapsed > TIMEOUT_SEC) {
        const char *tname = clients[current_player_idx].username;
        printf("[Server] %s timed out (%.2f sec).\n", tname, elapsed);

        // 유효한 수가 남아 있으면 invalid_move
        if (hasValidMove(&game_board, clients[current_player_idx].color)) {
            printf("[Server] %s has valid moves → sending invalid_move due to timeout\n", tname);
            JsonValue *inv = createInvalidMoveMessage(&game_board, tname);
            char *inv_str = json_stringify(inv);
            send(clients[current_player_idx].socket, inv_str, strlen(inv_str), 0);
            send(clients[current_player_idx].socket, "\n", 1, 0);
            free(inv_str); json_free(inv);
        } else {
            // 진짜 패스
            printf("[Server] %s has no valid moves → auto-pass due to timeout\n", tname);
            game_board.consecutivePasses++;
            JsonValue *pass_msg = createPassMessage(clients[(current_player_idx + 1) % MAX_CLIENTS].username);
            char *pass_str = json_stringify(pass_msg);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket != -1) {
                    send(clients[i].socket, pass_str, strlen(pass_str), 0);
                    send(clients[i].socket, "\n", 1, 0);
                }
            }
            free(pass_str); json_free(pass_msg);
            printBoard(&game_board); printf("\n");
        }

        // 게임 종료 여부
        if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
            broadcast_game_over();
        } else {
            // 다음 플레이어 턴
            current_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
            send_your_turn(current_player_idx);
        }
    }
}
// 게임 상태 로그
void log_game_state(const char *action, int player_idx, Move *move) {
    printf("[게임 로그] %s - 플레이어: %s", action, 
           (player_idx >= 0) ? clients[player_idx].username : "시스템");
    
    if (move && (move->sourceRow != 0 || move->sourceCol != 0 || 
                 move->targetRow != 0 || move->targetCol != 0)) {
        printf(", 이동: (%d,%d)->(%d,%d)", 
               move->sourceRow, move->sourceCol, 
               move->targetRow, move->targetCol);
    }
    
    printf(", 보드상태: R=%d B=%d Empty=%d\n", 
           game_board.redCount, game_board.blueCount, game_board.emptyCount);
}

void handle_register_message(int client_idx, JsonValue *json_obj) {
    JsonValue *username_json = json_object_get(json_obj, "username");
    if (!username_json || username_json->type != JSON_STRING) {
        fprintf(stderr, "유효하지 않은 등록 메시지\n");
        return;
    }
    const char *username = json_string_value(username_json);

    // --- ① 중복 등록 체크 ---
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != client_idx && clients[i].socket != -1) {
            if (strcmp(clients[i].username, username) == 0) {
                // 중복일 때 register_nack 전송 후 연결 종료
                JsonValue *nack = createRegisterNackMessage("duplicate username");
                char *nack_str = json_stringify(nack);
                send(clients[client_idx].socket, nack_str, strlen(nack_str), 0);
                send(clients[client_idx].socket, "\n", 1, 0);
                free(nack_str);
                json_free(nack);
                close(clients[client_idx].socket);
                clients[client_idx].socket = -1;
                client_count--;   // 연결 카운트도 하나 줄여줌
                printf("[Server] register_nack sent to %s (duplicate)\n", username);
                return;
            }
        }
    }

    // --- ② 정상 등록 처리 ---
// 올바른 사용: clients[client_idx].username (char[64])만 dest로 전달
strncpy(clients[client_idx].username,
        username,
        sizeof(clients[client_idx].username) - 1);
clients[client_idx].username[sizeof(clients[client_idx].username) - 1] = '\0';

    printf("[DEBUG] Registered client_idx=%d, username=%s, assigned_color=%c\n",
       client_idx, clients[client_idx].username, clients[client_idx].color);

    // 색상 할당 (0번 클라이언트: RED, 1번 클라이언트: BLUE)
clients[client_idx].color = (client_idx == 0) ? RED_PLAYER : BLUE_PLAYER;
printf("[Server] Player registered: %s (color: %c)\n", username, clients[client_idx].color);

    // ACK 메시지 전송
    JsonValue *ack_msg = createRegisterAckMessage();
    char *ack_str = json_stringify(ack_msg);
    send(clients[client_idx].socket, ack_str, strlen(ack_str), 0);
    send(clients[client_idx].socket, "\n", 1, 0);
    free(ack_str);
    json_free(ack_msg);

    // --- ③ 두 플레이어가 모두 등록되면 게임 시작 ---
    if (client_count == MAX_CLIENTS &&
        strlen(clients[0].username) > 0 && strlen(clients[1].username) > 0) {
        broadcast_game_start();
    }
}

void handle_move_message(int client_idx, JsonValue *json_obj) {
    if (server_state != SERVER_GAME_IN_PROGRESS) {
        printf("[Server] Received move but game not in progress.\n");
        return;
    }

    // 현재 차례 아닌 플레이어가 보냈으면 invalid_move
    if (client_idx != current_player_idx) {
        printf("[Server] %s tried to move out of turn.\n", clients[client_idx].username);
        JsonValue *inv = createInvalidMoveMessage(&game_board, clients[current_player_idx].username);
        char *inv_str = json_stringify(inv);
        send(clients[client_idx].socket, inv_str, strlen(inv_str), 0);
        send(clients[client_idx].socket, "\n", 1, 0);
        free(inv_str); json_free(inv);
        return;
    }

    char *username = NULL;
   
    if (!parseMoveMessage(json_obj, &username, &move)) {
        printf("[Server] Failed to parse move JSON from %s.\n", clients[client_idx].username);
        return;
    }printf("[DEBUG] parsed move: player=%c, src=(%d,%d), dst=(%d,%d)\n",
        move.player, move.sourceRow, move.sourceCol,
        move.targetRow, move.targetCol);

    // 0-based → 1-based 로 로그에 출력
    printf("[Server] Move received from %s: (%d,%d)->(%d,%d)\n",
           clients[client_idx].username,
           move.sourceRow + 1, move.sourceCol + 1,
           move.targetRow + 1, move.targetCol + 1);

    // --- 패스(0,0,0,0) 검사 ---
    if (move.sourceRow == 0 && move.sourceCol == 0 &&
        move.targetRow == 0 && move.targetCol == 0) {
        // 유효한 이동이 하나라도 남아 있으면 패스 불가 → invalid_move
        if (hasValidMove(&game_board, move.player)) {
            printf("[Server] %s sent pass but valid moves remain → invalid_move\n",
                   clients[client_idx].username);
            JsonValue *inv = createInvalidMoveMessage(&game_board, clients[current_player_idx].username);
            char *inv_str = json_stringify(inv);
            send(clients[client_idx].socket, inv_str, strlen(inv_str), 0);
            send(clients[client_idx].socket, "\n", 1, 0);
            free(inv_str); json_free(inv);
            free(username);
            return;
        }
        // 진짜 패스
        printf("[Server] %s passes (no moves left).\n", clients[client_idx].username);
        game_board.consecutivePasses++;

        // 패스 메시지 모두 전송
        JsonValue *pass_msg = createPassMessage(clients[current_player_idx].username);
        char *pass_str = json_stringify(pass_msg);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                send(clients[i].socket, pass_str, strlen(pass_str), 0);
                send(clients[i].socket, "\n", 1, 0);
            }
        }
        free(pass_str); json_free(pass_msg);

        printBoard(&game_board);
        printf("\n");

        // 연속 패스 2회면 게임 종료, 아니면 다음 턴
        if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
            broadcast_game_over();
        } else {
            current_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
            send_your_turn(current_player_idx);
        }

        free(username);
        return;
    }
     Move adjusted_move = {
    .player = move.player,
    .sourceRow = move.sourceRow - 1,
    .sourceCol = move.sourceCol - 1,
    .targetRow = move.targetRow - 1,
    .targetCol = move.targetCol - 1
};

    // --- 일반 이동 유효성 검사 ---
    if (!isValidMove(&game_board, &adjusted_move)) {
        printf("[Server] Invalid move by %s: (%d,%d)->(%d,%d)\n",
               clients[client_idx].username,
               move.sourceRow , move.sourceCol ,
               move.targetRow , move.targetCol );

        // invalid_move 응답 (board + next_player 포함)
        JsonValue *inv = createInvalidMoveMessage(&game_board, clients[current_player_idx].username);
        char *inv_str = json_stringify(inv);
        send(clients[client_idx].socket, inv_str, strlen(inv_str), 0);
        send(clients[client_idx].socket, "\n", 1, 0);
        free(inv_str); json_free(inv);

        free(username);
        return;
    }

    // --- 합법적 이동 처리 --- 
    applyMove(&game_board, &adjusted_move);              // 보드에 반영
    game_board.consecutivePasses = 0;           // 패스 카운트 리셋
    printf("[Server] Move applied. New board:\n");
    printBoard(&game_board);
    printf("\n");

    // move_ok 메시지 생성 (board + next_player)
    int next_idx = (current_player_idx + 1) % MAX_CLIENTS;
    JsonValue *ok = createMoveOkMessage(&game_board, clients[next_idx].username);
    char *ok_str = json_stringify(ok);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, ok_str, strlen(ok_str), 0);
            send(clients[i].socket, "\n", 1, 0);
        }
    }
    free(ok_str); json_free(ok);

    // --- 게임 종료 검사 ---
    if (hasGameEnded(&game_board)) {
        broadcast_game_over();
    } else {
        // 다음 턴
        current_player_idx = next_idx;
        send_your_turn(current_player_idx);
    }

    free(username);
}

void broadcast_game_start() {
    server_state = SERVER_GAME_IN_PROGRESS;

    // --- 보드 초기화 (한 번만) ---
    initializeBoard(&game_board);

    // 클라이언트 이름을 배열로 추출
    const char *usernames[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        usernames[i] = clients[i].username;
    }

    // game_start 메시지 생성
    JsonValue *start_msg = createGameStartMessage(usernames, clients[0].username);
    char *start_str = json_stringify(start_msg);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, start_str, strlen(start_str), 0);
            send(clients[i].socket, "\n", 1, 0);
        }
    }
    free(start_str);
    json_free(start_msg);

    printf("[Server] game_start sent: players=[%s,%s], first_player=%s\n",
           clients[0].username, clients[1].username, clients[0].username);

    // 첫 번째 플레이어 턴 설정
    current_player_idx = 0;

    // ==========================
    // ★ 신규 추가 부분: 게임 시작 시,
    //   글로벌 move.player 에 첫 번째 플레이어 색상(R 또는 B) 할당
    move.player = clients[current_player_idx].color;
    // (선택) 초기 좌표를 0으로 리셋해 두고 싶으면 아래처럼 해도 됩니다.
    move.sourceRow = move.sourceCol = 0;
    move.targetRow = move.targetCol = 0;
    // ==========================

    // 첫 번째 플레이어에게 턴 알림
    send_your_turn(current_player_idx);
}

void broadcast_game_over() {
    server_state = SERVER_GAME_OVER;

    const char *players[2] = {clients[0].username, clients[1].username};
    int scores[2] = {game_board.redCount, game_board.blueCount};

    JsonValue *game_over_msg = createGameOverMessage(players, scores);
    char *json_str = json_stringify(game_over_msg);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
            send(clients[i].socket, "\n", 1, 0);
        }
    }
    free(json_str); json_free(game_over_msg);

    if (game_board.redCount > game_board.blueCount) {
        printf("[Server] Game over: %s wins! (R=%d, B=%d)\n",
               clients[0].username, game_board.redCount, game_board.blueCount);
    } else if (game_board.blueCount > game_board.redCount) {
        printf("[Server] Game over: %s wins! (R=%d, B=%d)\n",
               clients[1].username, game_board.redCount, game_board.blueCount);
    } else {
        printf("[Server] Game over: Draw! (R=%d, B=%d)\n",
               game_board.redCount, game_board.blueCount);
    }

}

// 클라이언트 메시지 처리
void handle_client_message(int client_idx, char *buffer) {
    printf("클라이언트 %d로부터 메시지: %s\n", client_idx, buffer);
    
    JsonValue *json_obj = json_parse(buffer);
    if (!json_obj) {
        fprintf(stderr, "유효하지 않은 JSON 메시지: %s\n", buffer);
        return;
    }
    
    MessageType msg_type = parseMessageType(json_obj);
    
    switch (msg_type) {
        case MSG_REGISTER:
            handle_register_message(client_idx, json_obj);
            break;
            
        case MSG_MOVE:
            handle_move_message(client_idx, json_obj);
            break;
            
        default:
            fprintf(stderr, "알 수 없는 메시지 유형\n");
            break;
    }
    
    json_free(json_obj);
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 시그널 핸들러 설정
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // 클라이언트별 수신 버퍼 구조체 정의 및 초기화
    typedef struct {
        char buffer[BUFFER_SIZE * 2];  // 넉넉한 여유 공간
        size_t length;                 // 버퍼에 담긴 바이트 수
    } ClientBuffer;
    static ClientBuffer client_buffers[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_buffers[i].length = 0;
        memset(client_buffers[i].buffer, 0, sizeof(client_buffers[i].buffer));
    }

    // 클라이언트 정보 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        memset(clients[i].username, 0, sizeof(clients[i].username));
    }
    client_count = 0;

    // 게임 보드 초기화
    initializeBoard(&game_board);
    server_state = SERVER_WAITING_PLAYERS;
    current_player_idx = 0;

    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 소켓 옵션 설정 (주소 재사용)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 주소 설정
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 리스닝
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("OctaFlip 서버가 포트 %d에서 시작되었습니다.\n", PORT);

    // poll을 위한 구조체 배열 (서버 소켓 + 최대 클라이언트 수)
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;  // 현재 활성 파일 디스크립터 수

    // 서버 소켓 설정
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    // 클라이언트 소켓 초기화
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }

    // 임시 버퍼
    char temp_buf[BUFFER_SIZE];

    while (1) {
        // 타임아웃 확인
        check_timeout();

        // poll 호출 (100ms 타임아웃)
        int poll_result = poll(fds, nfds, 100);
        if (poll_result < 0 && errno != EINTR) {
            perror("poll error");
            continue;
        }

        // ─ 신규 연결 처리 ─
        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                continue;
            }

            // 비차단 모드로 설정
            set_socket_nonblocking(new_socket);

            printf("새 연결, 소켓 fd: %d, IP: %s, 포트: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // 빈 슬롯 찾기
            int slot_found = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == -1) {
                    clients[i].socket = new_socket;

                    // poll 배열에 추가
                    for (int j = 1; j <= MAX_CLIENTS; j++) {
                        if (fds[j].fd == -1) {
                            fds[j].fd = new_socket;
                            fds[j].events = POLLIN;
                            if (j >= nfds) nfds = j + 1;
                            break;
                        }
                    }

                    client_count++;
                    slot_found = 1;
                    break;
                }
            }

            if (!slot_found) {
                // 최대 클라이언트 도달 시 연결 거부
                close(new_socket);
                printf("최대 클라이언트 수에 도달. 연결 거부.\n");
            }
        }

        // ─ 클라이언트 메시지 수신 처리 ─
        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                // 클라이언트 소켓으로부터 데이터 읽기
                int valread = read(fds[i].fd, temp_buf, sizeof(temp_buf) - 1);
                if (valread > 0) {
                    temp_buf[valread] = '\0';

                    // 어느 클라이언트 인덱스인지 찾기
                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == fds[i].fd) {
                            client_idx = j;
                            break;
                        }
                    }
                    if (client_idx == -1) continue;

                    // 해당 클라이언트 버퍼에 이어 붙이기
                    ClientBuffer *cb = &client_buffers[client_idx];
                    size_t copy_len = (cb->length + valread < sizeof(cb->buffer))
                                          ? valread
                                          : (sizeof(cb->buffer) - 1 - cb->length);
                    memcpy(cb->buffer + cb->length, temp_buf, copy_len);
                    cb->length += copy_len;
                    cb->buffer[cb->length] = '\0';

                    // '\n' 단위로 분리하여 완전한 메시지 처리
                    char *newline_pos;
                    while ((newline_pos = memchr(cb->buffer, '\n', cb->length)) != NULL) {
                        size_t msg_len = newline_pos - cb->buffer;
                        char saved = cb->buffer[msg_len];
                        cb->buffer[msg_len] = '\0';

                        handle_client_message(client_idx, cb->buffer);

                        cb->buffer[msg_len] = saved;
                        size_t remaining = cb->length - (msg_len + 1);
                        memmove(cb->buffer, newline_pos + 1, remaining);
                        cb->length = remaining;
                    }
                } else {
                    // 연결 종료 또는 오류 발생 시 처리
                    handle_client_disconnect(fds[i].fd);

                    // poll 배열에서 제거
                    fds[i].fd = -1;

                    // nfds 재계산
                    while (nfds > 1 && fds[nfds - 1].fd == -1) {
                        nfds--;
                    }
                }
            }
        }
    }

    return 0;
}

