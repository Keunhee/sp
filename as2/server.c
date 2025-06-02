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
#include <stdbool.h>
#define DEFAULT_PORT 8888
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
typedef struct {
    char buffer[BUFFER_SIZE * 2];  // 더 큰 버퍼
    size_t length;
} ClientBuffer;

// 전역 변수에 추가
ClientBuffer client_buffers[MAX_CLIENTS];
// 전역 변수
ServerState server_state = SERVER_WAITING_PLAYERS;
Client clients[MAX_CLIENTS];
int client_count = 0;
GameBoard game_board;
int current_player_idx = 0;
struct timeval turn_start_time;
char server_ip[INET_ADDRSTRLEN] = "127.0.0.1";  // 기본값: 모든 인터페이스
int server_port = DEFAULT_PORT;
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
void init_client_buffers() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        memset(&client_buffers[i], 0, sizeof(ClientBuffer));
    }
}
void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p, --port <port>    서버 포트 번호 (기본값: %d)\n", DEFAULT_PORT);
    printf("  -i, --ip <ip>        서버 IP 주소 (기본값: 0.0.0.0 - 모든 인터페이스)\n");
    printf("  -h, --help           이 도움말 표시\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                           # 기본 설정으로 실행 (0.0.0.0:8888)\n", program_name);
    printf("  %s -p 9999                   # 포트 9999로 실행\n", program_name);
    printf("  %s -i 127.0.0.1 -p 8080      # 127.0.0.1:8080으로 실행\n", program_name);
    printf("  %s --ip 192.168.1.100 --port 7777  # 192.168.1.100:7777로 실행\n", program_name);
}
int parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s 옵션에 포트 번호가 필요합니다.\n", argv[i]);
                return -1;
            }
            
            server_port = atoi(argv[i + 1]);
            if (server_port <= 0 || server_port > 65535) {
                fprintf(stderr, "Error: 유효하지 않은 포트 번호: %s (1-65535 범위여야 합니다)\n", argv[i + 1]);
                return -1;
            }
            i++; // 다음 인자 건너뛰기
            
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ip") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s 옵션에 IP 주소가 필요합니다.\n", argv[i]);
                return -1;
            }
            
            // IP 주소 유효성 검사
            struct sockaddr_in sa;
            int result = inet_pton(AF_INET, argv[i + 1], &(sa.sin_addr));
            if (result == 0) {
                fprintf(stderr, "Error: 유효하지 않은 IP 주소: %s\n", argv[i + 1]);
                return -1;
            }
            
            strncpy(server_ip, argv[i + 1], sizeof(server_ip) - 1);
            server_ip[sizeof(server_ip) - 1] = '\0';
            i++; // 다음 인자 건너뛰기
            
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
            
        } else {
            fprintf(stderr, "Error: 알 수 없는 옵션: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    
    return 0;
}
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
void process_client_data(int client_idx, char *new_data, size_t data_len) {
    ClientBuffer *cb = &client_buffers[client_idx];
    
    // 새 데이터를 버퍼에 추가
    if (cb->length + data_len >= sizeof(cb->buffer) - 1) {
        printf("[Server] Buffer overflow for client %d, resetting\n", client_idx);
        cb->length = 0;  // 버퍼 리셋
    }
    
    memcpy(cb->buffer + cb->length, new_data, data_len);
    cb->length += data_len;
    cb->buffer[cb->length] = '\0';
    
    // 개행문자('\n')로 구분된 완전한 메시지들 처리
    char *start = cb->buffer;
    char *newline_pos;
    
    while ((newline_pos = strchr(start, '\n')) != NULL) {
        *newline_pos = '\0';  // 개행문자를 null terminator로 변경
        
        // 빈 메시지가 아닌 경우에만 처리
        if (strlen(start) > 0) {
            printf("[Server] Processing complete JSON: %s\n", start);
            handle_client_message(client_idx, start);
        }
        
        start = newline_pos + 1;  // 다음 메시지로 이동
    }
    
    // 처리되지 않은 부분적 메시지를 버퍼 앞으로 이동
    size_t remaining = strlen(start);
    if (remaining > 0) {
        memmove(cb->buffer, start, remaining + 1);  // null terminator 포함
        cb->length = remaining;
    } else {
        cb->length = 0;
        cb->buffer[0] = '\0';
    }
}
// process_client_data 함수 다음에 추가

void handle_client_disconnect(int socket_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    // 연결 정보 출력
    if (getpeername(socket_fd, (struct sockaddr*)&address, &addrlen) == 0) {
        printf("연결 종료, IP: %s, 포트: %d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    }
    
    // 클라이언트 찾기
    int disconnected_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket_fd) {
            disconnected_idx = i;
            break;
        }
    }
    
    if (disconnected_idx == -1) return;
    
    printf("플레이어 %s 연결 끊김\n", clients[disconnected_idx].username);
    
    // ✅ 클라이언트 정리 (공통 처리)
    close(socket_fd);
    clients[disconnected_idx].socket = -1;
    memset(clients[disconnected_idx].username, 0, sizeof(clients[disconnected_idx].username));
    client_count--;
    
    // ✅ 게임 중인 경우 처리
    if (server_state == SERVER_GAME_IN_PROGRESS) {
        int other_idx = (disconnected_idx + 1) % MAX_CLIENTS;
        
        // ✅ 상대방이 아직 연결되어 있는 경우 (첫 번째 disconnect)
        if (clients[other_idx].socket != -1) {
            printf("[Server] Player %s disconnected, continuing game with remaining player\n", 
                   clients[disconnected_idx].username);
            
            // ✅ 상대방에게 opponent_left 메시지만 전송 (게임 종료 아님)
            JsonValue *opponent_left_msg = createOpponentLeftMessage(clients[disconnected_idx].username);
            char *left_str = json_stringify(opponent_left_msg);
            send(clients[other_idx].socket, left_str, strlen(left_str), 0);
            send(clients[other_idx].socket, "\n", 1, 0);
            free(left_str);
            json_free(opponent_left_msg);
            
            // ✅ 현재 턴이 연결 끊긴 플레이어였다면 상대방으로 턴 변경
            if (current_player_idx == disconnected_idx) {
                printf("[Server] Disconnected player's turn, switching to remaining player\n");
                current_player_idx = other_idx;
                send_your_turn(current_player_idx);
            } else {
                printf("[Server] Remaining player's turn continues\n");
            }
            
            // ✅ 게임은 계속 진행 (게임 종료 메시지 전송하지 않음)
            
        } else {
            // ✅ 두 번째 disconnect (상대방도 이미 끊김) → 서버 종료
            printf("[Server] Second player disconnected - both players gone\n");
            printf("[Server] All players disconnected. Shutting down server...\n");
            cleanup_and_exit(0);
        }
        
    } else if (server_state == SERVER_WAITING_PLAYERS) {
        // ✅ 대기 중 disconnect
        printf("[Server] Player disconnected while waiting for game to start\n");
    }
    
    // ✅ 남은 연결된 클라이언트가 있는지 확인
    int remaining_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            remaining_clients++;
        }
    }
    
    printf("[Server] Remaining connected clients: %d\n", remaining_clients);
    
    // ✅ 모든 클라이언트가 disconnect된 경우 → 서버 종료
    if (remaining_clients == 0) {
        printf("[Server] All players disconnected. Shutting down server...\n");
        cleanup_and_exit(0);
    }
}

void check_timeout() {
    if (server_state != SERVER_GAME_IN_PROGRESS) return;

    // ✅ 현재 플레이어가 연결되어 있는지 확인
    if (clients[current_player_idx].socket == -1) {
        printf("[Server] Current player is disconnected, switching turn\n");
        
        // 다음 플레이어로 변경
        int next_idx = (current_player_idx + 1) % MAX_CLIENTS;
        
        // 다음 플레이어도 연결되어 있는지 확인
        if (clients[next_idx].socket != -1) {
            current_player_idx = next_idx;
            send_your_turn(current_player_idx);
        } else {
            // 두 플레이어 모두 연결 끊김
            printf("[Server] All players disconnected during timeout check\n");
            cleanup_and_exit(0);
        }
        return;
    }

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    double elapsed = (current_time.tv_sec - turn_start_time.tv_sec) +
                     (current_time.tv_usec - turn_start_time.tv_usec) / 1000000.0;

    if (elapsed > TIMEOUT_SEC) {
        const char *tname = clients[current_player_idx].username;
        printf("[Server] %s timed out (%.2f sec). Forcing turn change.\n", tname, elapsed);
        game_board.consecutivePasses++;
        
        // ✅ 연결된 클라이언트들에게만 패스 메시지 전송
        JsonValue *pass_msg = createPassMessage(clients[current_player_idx].username);
        char *pass_str = json_stringify(pass_msg);
        if (clients[current_player_idx].socket != -1) {
            send(clients[current_player_idx].socket, pass_str, strlen(pass_str), 0);
            send(clients[current_player_idx].socket, "\n", 1, 0);
        }
        free(pass_str); 
        json_free(pass_msg);

        // 게임 종료 확인
        if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
            broadcast_game_over();
            return;
        }

        // ✅ 다음 플레이어로 턴 변경
        int next_idx = (current_player_idx + 1) % MAX_CLIENTS;
        
        // 다음 플레이어가 연결되어 있는지 확인
        if (clients[next_idx].socket != -1) {
            current_player_idx = next_idx;
            send_your_turn(current_player_idx);
        } else {
            // 다음 플레이어도 연결 끊김 - 현재 플레이어 턴 유지
            printf("[Server] Next player also disconnected, keeping current turn\n");
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
    strncpy(clients[client_idx].username, username, sizeof(clients[client_idx].username) - 1);
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
    // ✅ 연결이 끊긴 클라이언트인지 확인
    if (clients[client_idx].socket == -1) {
        printf("[Server] Received move from disconnected client, ignoring.\n");
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
        JsonValue *inv = createInvalidMoveMessage(&game_board, clients[current_player_idx].username);
        char *inv_str = json_stringify(inv);
        send(clients[client_idx].socket, inv_str, strlen(inv_str), 0);
        send(clients[client_idx].socket, "\n", 1, 0);
        free(inv_str); json_free(inv);
        return;
    }

    // ✅ 원본 좌표 보존 (로깅용)
    Move original_move = move;
    
    printf("[Server] Move received from %s: (%d,%d)->(%d,%d)\n",
           clients[client_idx].username,
           original_move.sourceRow, original_move.sourceCol,
           original_move.targetRow, original_move.targetCol);

    // --- 패스(0,0,0,0) 검사 ---
    if (move.sourceRow == 0 && move.sourceCol == 0 &&
        move.targetRow == 0 && move.targetCol == 0) {
        
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
        
        // ✅ 진짜 패스 처리
        printf("[Server] %s passes (no moves left).\n", clients[client_idx].username);
        game_board.consecutivePasses++;

        JsonValue *pass_msg = createPassMessage(clients[current_player_idx].username);
        char *pass_str = json_stringify(pass_msg);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                send(clients[i].socket, pass_str, strlen(pass_str), 0);
                send(clients[i].socket, "\n", 1, 0);
            }
        }
        free(pass_str); json_free(pass_msg);

        if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
            broadcast_game_over();
        } else {
            current_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
            send_your_turn(current_player_idx);
        }

        free(username);
        return;
    }

    // ✅ 좌표 변환: 클라이언트 좌표(1-based) → 내부 좌표(0-based)
    Move adjusted_move = {
        .player = move.player,
        .sourceRow = move.sourceRow - 1,
        .sourceCol = move.sourceCol - 1,
        .targetRow = move.targetRow - 1,
        .targetCol = move.targetCol - 1
    };

    // --- 일반 이동 유효성 검사 ---
    if (!isValidMove(&game_board, &adjusted_move)) {
        printf("[Server] Invalid move by %s: (%d,%d)->(%d,%d) [internal: (%d,%d)->(%d,%d)]\n",
               clients[client_idx].username,
               original_move.sourceRow, original_move.sourceCol,
               original_move.targetRow, original_move.targetCol,
               adjusted_move.sourceRow, adjusted_move.sourceCol,
               adjusted_move.targetRow, adjusted_move.targetCol);

        JsonValue *inv = createInvalidMoveMessage(&game_board, clients[current_player_idx].username);
        char *inv_str = json_stringify(inv);
        send(clients[client_idx].socket, inv_str, strlen(inv_str), 0);
        send(clients[client_idx].socket, "\n", 1, 0);
        free(inv_str); json_free(inv);

        free(username);
        return;
    }

    // --- ✅ 합법적 이동 처리 --- 
    printf("[Server] Valid move by %s: (%d,%d)->(%d,%d). Applying...\n",
           clients[client_idx].username,
           original_move.sourceRow, original_move.sourceCol,
           original_move.targetRow, original_move.targetCol);

    // 보드에 이동 적용
    applyMove(&game_board, &adjusted_move);
    game_board.consecutivePasses = 0;  // 패스 카운트 리셋
    
    printf("[Server] Move applied successfully. New board state:\n");
    printBoard(&game_board);
    printf("[Server] Score: R=%d, B=%d, Empty=%d\n", 
           game_board.redCount, game_board.blueCount, game_board.emptyCount);

    // ✅ 다음 플레이어 결정 (게임 종료 확인 전에)
    int next_idx = (current_player_idx + 1) % MAX_CLIENTS;
    
    // ✅ 게임 종료 확인
    bool game_ended = hasGameEnded(&game_board);
    
    if (game_ended) {
        printf("[Server] Game ended after move. Broadcasting game_over...\n");
        
        // ✅ 게임 종료 시에는 next_player를 null로 설정한 move_ok 전송
        JsonValue *ok = createMoveOkMessage(&game_board, NULL);  // next_player = null
        char *ok_str = json_stringify(ok);
        if (clients[current_player_idx].socket != -1) {
            send(clients[current_player_idx].socket, ok_str, strlen(ok_str), 0);
            send(clients[current_player_idx].socket, "\n", 1, 0);
        }
        free(ok_str); json_free(ok);
        
        printf("[Server] move_ok sent (game ended). Broadcasting game_over...\n");
        broadcast_game_over();
    } else {
        // ✅ 게임 계속 시에는 정확한 다음 플레이어와 함께 move_ok 전송
        JsonValue *ok = createMoveOkMessage(&game_board, clients[next_idx].username);
        char *ok_str = json_stringify(ok);
        if (clients[current_player_idx].socket != -1) {
            send(clients[current_player_idx].socket, ok_str, strlen(ok_str), 0);
            send(clients[current_player_idx].socket, "\n", 1, 0);
        }
        free(ok_str); json_free(ok);

        printf("[Server] move_ok sent to all clients. Next player: %s\n", 
               clients[next_idx].username);

        // 다음 턴으로 이동
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

// 턴 시작 메시지 전송
void send_your_turn(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS || clients[client_idx].socket == -1) {
        return;
    }

    // ==========================
    // ★ 신규 추가 부분: 매 턴마다 현재 턴 플레이어 색상을 move.player 에 다시 할당
    move.player = clients[client_idx].color;
    // (필요하다면) 이전 move 좌표를 초기화하거나, 직전에 적용된 색상의 좌표를 남길 수도 있습니다.
    // ==========================

    // 'your_turn' 메시지 생성 시 현재 보드 상태와 타임아웃을 함께 보내줌
    JsonValue *turn_msg = createYourTurnMessage(&game_board, TIMEOUT_SEC);
    char *turn_str = json_stringify(turn_msg);
    send(clients[client_idx].socket, turn_str, strlen(turn_str), 0);
    send(clients[client_idx].socket, "\n", 1, 0);
    free(turn_str);
    json_free(turn_msg);

    // 턴 타이머 시작
    gettimeofday(&turn_start_time, NULL);
    printf("[Server] your_turn sent to %s (timeout=%.1f)\n",
           clients[client_idx].username, TIMEOUT_SEC);
}
void broadcast_game_over() {
    server_state = SERVER_GAME_OVER;

    countPieces(&game_board);
    
    // ✅ 수정: 직접 계산으로 검증
    int manual_red = 0, manual_blue = 0, manual_empty = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            char cell = game_board.cells[i][j];
            if (cell == RED_PLAYER) manual_red++;
            else if (cell == BLUE_PLAYER) manual_blue++;
            else if (cell == EMPTY_CELL) manual_empty++;
        }
    }
    
    printf("[DEBUG] countPieces result: R=%d, B=%d, Empty=%d\n", 
           game_board.redCount, game_board.blueCount, game_board.emptyCount);
    printf("[DEBUG] Manual count: R=%d, B=%d, Empty=%d, Total=%d\n", 
           manual_red, manual_blue, manual_empty, manual_red + manual_blue + manual_empty);

    const char *players[2] = {clients[0].username, clients[1].username};
    int scores[2];
    
    // 검증 후 올바른 값 사용
    if (manual_red + manual_blue + manual_empty == 64) {
        scores[0] = manual_red;
        scores[1] = manual_blue;
        printf("[Server] Using manual count (Total=64 ✓)\n");
    } else {
        scores[0] = game_board.redCount;
        scores[1] = game_board.blueCount;
        printf("[Server] Using countPieces result (Manual total=%d)\n", 
               manual_red + manual_blue + manual_empty);
    }

    JsonValue *game_over_msg = createGameOverMessage(players, scores);
    char *json_str = json_stringify(game_over_msg);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
            send(clients[i].socket, "\n", 1, 0);
        }
    }
    free(json_str); 
    json_free(game_over_msg);

    if (scores[0] > scores[1]) {
        printf("[Server] Game over: %s wins! (R=%d, B=%d)\n",
               clients[0].username, scores[0], scores[1]);
    } else if (scores[1] > scores[0]) {
        printf("[Server] Game over: %s wins! (R=%d, B=%d)\n",
               clients[1].username, scores[0], scores[1]);
    } else {
        printf("[Server] Game over: Draw! (R=%d, B=%d)\n",
               scores[0], scores[1]);
    }
    
    printf("[Server] Game ended normally. Waiting for final messages...\n");
    sleep(2);  // 클라이언트가 메시지를 받을 시간
    
    // ✅ 게임 종료 후 서버도 종료하도록 복원
    printf("[Server] Game completed. Shutting down server...\n");
    cleanup_and_exit(0);
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
    
    // 명령줄 인자 파싱
    if (parse_arguments(argc, argv) != 0) {
        return EXIT_FAILURE;
    }
    
    // 시그널 핸들러 설정
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        memset(clients[i].username, 0, sizeof(clients[i].username));
    }
     init_client_buffers();
    // 게임 보드 초기화
    initializeBoard(&game_board);
    
    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // 소켓 옵션 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // 주소 설정 (수정됨)
    address.sin_family = AF_INET;
    
    // IP 주소 설정
    if (strcmp(server_ip, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;  // 모든 인터페이스
    } else {
        if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
            fprintf(stderr, "Error: IP 주소 변환 실패: %s\n", server_ip);
            exit(EXIT_FAILURE);
        }
    }
    
    address.sin_port = htons(server_port);  // 포트 설정
    
    // 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        printf("Failed to bind to %s:%d\n", server_ip, server_port);
        exit(EXIT_FAILURE);
    }
    
    // 리스닝
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // 서버 시작 메시지 (수정됨)
    printf("OctaFlip 서버가 %s:%d에서 시작되었습니다.\n", 
           strcmp(server_ip, "0.0.0.0") == 0 ? "모든 인터페이스" : server_ip, 
           server_port);
    
    char buffer[BUFFER_SIZE];
    
    // poll을 위한 구조체 배열 (서버 소켓 + 최대 클라이언트 수)
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;  // 현재 활성 파일 디스크립터 수
    
    // 서버 소켓 설정
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    
    // 클라이언트 소켓 초기화
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }
    
    while (1) {
        // 타임아웃 확인
        check_timeout();
        
        // poll 호출 (100ms 타임아웃)
        int poll_result = poll(fds, nfds, 100);
        
        if (poll_result < 0 && errno != EINTR) {
            perror("poll error");
            continue;
        }
        
        // 새 연결 확인
        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
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
                // 추가 연결 거부
                close(new_socket);
                printf("최대 클라이언트 수에 도달. 연결 거부.\n");
            }
        }
        
        // 클라이언트 메시지 확인
                for (int i = 1; i <= MAX_CLIENTS && i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                // 클라이언트로부터 데이터 읽기
                char temp_buffer[BUFFER_SIZE];
                int valread = read(fds[i].fd, temp_buffer, BUFFER_SIZE - 1);
                
                if (valread > 0) {
                    temp_buffer[valread] = '\0';
                    
                    // 해당 클라이언트 인덱스 찾기
                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == fds[i].fd) {
                            client_idx = j;
                            break;
                        }
                    }
                    
                    if (client_idx != -1) {
                        // 청크 단위로 받은 데이터 처리
                        process_client_data(client_idx, temp_buffer, valread);
                    }
                } else {
                    // 연결 종료 또는 오류
                    handle_client_disconnect(fds[i].fd);
                    
                    // 해당 클라이언트의 버퍼도 초기화
                    int disconnected_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == fds[i].fd) {
                            disconnected_idx = j;
                            break;
                        }
                    }
                    if (disconnected_idx != -1) {
                        memset(&client_buffers[disconnected_idx], 0, sizeof(ClientBuffer));
                    }
                    
                    // poll 배열에서 제거
                    fds[i].fd = -1;
                    
                    // nfds 재계산
                    while (nfds > 1 && fds[nfds-1].fd == -1) {
                        nfds--;
                    }
                }
            }
        }
    }
    
    return 0;
}

