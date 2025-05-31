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

#define PORT 8888
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5.0

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

// 클라이언트 연결 끊김 처리
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
    
    // 소켓 닫기
    close(socket_fd);
    clients[disconnected_idx].socket = -1;
    memset(clients[disconnected_idx].username, 0, sizeof(clients[disconnected_idx].username));
    client_count--;
    
    printf("플레이어 %s 연결 끊김\n", clients[disconnected_idx].username);
    
    // 게임 중이면 상대방에게 승리 선언
    if (server_state == SERVER_GAME_IN_PROGRESS) {
        int other_idx = (disconnected_idx + 1) % MAX_CLIENTS;
        
        if (clients[other_idx].socket != -1) {
            // 게임 종료 처리 (상대방 승리)
            const char *players[2] = {clients[0].username, clients[1].username};
            int scores[2] = {0, 0};
            
            // 연결 끊긴 플레이어가 아닌 쪽을 승자로 설정
            if (disconnected_idx == 0) {
                scores[1] = 1; // 플레이어 1 승리
            } else {
                scores[0] = 1; // 플레이어 0 승리
            }
            
            JsonValue *game_over_msg = createGameOverMessage(players, scores);
            char *json_str = json_stringify(game_over_msg);
            
            send(clients[other_idx].socket, json_str, strlen(json_str), 0);
            send(clients[other_idx].socket, "\n", 1, 0);
            free(json_str);
            json_free(game_over_msg);
            
            printf("플레이어 %s가 상대방 연결 끊김으로 승리\n", clients[other_idx].username);
        }
        
        // 게임 상태 초기화
        server_state = SERVER_WAITING_PLAYERS;
        initializeBoard(&game_board);
        current_player_idx = 0;
    }
}

void check_timeout() {
    if (server_state != SERVER_GAME_IN_PROGRESS) {
        return;
    }

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    double elapsed = (current_time.tv_sec - turn_start_time.tv_sec) +
                     (current_time.tv_usec - turn_start_time.tv_usec) / 1000000.0;

    if (elapsed > TIMEOUT_SEC) {
        char player_color = clients[current_player_idx].color;

        if (hasValidMove(&game_board, player_color)) {
            printf("합법적인 수가 남아 있으므로 invalid_move 메시지 전송\n");

            JsonValue *invalid_msg = createInvalidMoveMessage(&game_board,
                                                              clients[current_player_idx].username);
            char *invalid_json = json_stringify(invalid_msg);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket != -1) {
                    send(clients[i].socket, invalid_json, strlen(invalid_json), 0);
                    send(clients[i].socket, "\n", 1, 0);
                }
            }

            free(invalid_json);
            json_free(invalid_msg);

            gettimeofday(&turn_start_time, NULL);
            return;
        }

        printf("합법적인 수가 없어서 진짜 패스 처리 (move_ok로 간주)\n");

        game_board.consecutivePasses++;

        int next_player_idx = (current_player_idx + 1) % MAX_CLIENTS;

        JsonValue *move_ok_msg = createMoveOkMessage(&game_board,
                                                     clients[next_player_idx].username);
        char *move_ok_json = json_stringify(move_ok_msg);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                send(clients[i].socket, move_ok_json, strlen(move_ok_json), 0);
                send(clients[i].socket, "\n", 1, 0);
            }
        }

        free(move_ok_json);
        json_free(move_ok_msg);

        if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
            broadcast_game_over();
        } else {
            current_player_idx = next_player_idx;
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

// 등록 메시지 처리
void handle_register_message(int client_idx, JsonValue *json_obj) {
    JsonValue *username_json = json_object_get(json_obj, "username");
    if (!username_json || username_json->type != JSON_STRING) {
        fprintf(stderr, "유효하지 않은 등록 메시지\n");
        return;
    }
    
    const char *username = json_string_value(username_json);
    strncpy(clients[client_idx].username, username, sizeof(clients[client_idx].username) - 1);
    clients[client_idx].username[sizeof(clients[client_idx].username) - 1] = '\0';
    
    // 색상 할당
    clients[client_idx].color = (client_idx == 0) ? RED_PLAYER : BLUE_PLAYER;
    
    printf("플레이어 등록: %s (색상: %c)\n", username, clients[client_idx].color);
    
    // 등록 확인 메시지 전송
    JsonValue *ack_msg = createRegisterAckMessage();
    char *json_str = json_stringify(ack_msg);
    send(clients[client_idx].socket, json_str, strlen(json_str), 0);
    send(clients[client_idx].socket, "\n", 1, 0);
    free(json_str);
    json_free(ack_msg);
    
    // 두 플레이어가 모두 등록되면 게임 시작
    if (client_count == MAX_CLIENTS && 
        strlen(clients[0].username) > 0 && strlen(clients[1].username) > 0) {
        broadcast_game_start();
    }
}

// 이동 메시지 처리
void handle_move_message(int client_idx, JsonValue *json_obj) {
    if (server_state != SERVER_GAME_IN_PROGRESS) {
        printf("게임이 진행 중이 아님\n");
        return;
    }
    
    if (client_idx != current_player_idx) {
        printf("현재 턴이 아닌 플레이어의 이동 시도\n");
        return;
    }
    
    char *username;
    Move move;
    if (!parseMoveMessage(json_obj, &username, &move)) {
        printf("이동 메시지 파싱 실패\n");
        return;
    }
    
    move.player = clients[client_idx].color;
    
    printf("이동 시도: 플레이어 %s (%d,%d)->(%d,%d)\n", 
           clients[client_idx].username,
           move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
    
    // 패스 확인 (모든 좌표가 0인 경우)
    if (move.sourceRow == 0 && move.sourceCol == 0 && 
        move.targetRow == 0 && move.targetCol == 0) {
        
        printf("패스 시도 감지\n");
        
        // 실제로 유효한 이동이 있는지 확인
        if (hasValidMove(&game_board, move.player)) {
            // 유효한 이동이 있으면 패스 불가
            printf("유효한 이동이 있어 패스 거부\n");
            
            // 에러 메시지 전송 (여기서는 단순히 다시 턴 요청)
            send_your_turn(client_idx);
            return;
        } else {
            // 유효한 이동이 없으면 패스
            printf("유효한 이동이 없어 패스 허용\n");
            
            // 다음 플레이어로 전환
            int next_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
            
            // 패스 메시지 전송
            JsonValue *pass_msg = createPassMessage(clients[next_player_idx].username);
            char *json_str = json_stringify(pass_msg);
            
            // 모든 클라이언트에게 패스 메시지 전송
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket != -1) {
                    send(clients[i].socket, json_str, strlen(json_str), 0);
                    send(clients[i].socket, "\n", 1, 0);
                }
            }
            
            free(json_str);
            json_free(pass_msg);
            
            printf("플레이어 %s가 패스했습니다.\n", clients[client_idx].username);
            
            // 연속 패스 증가
            game_board.consecutivePasses++;
            
            // 게임 종료 확인
            if (game_board.consecutivePasses >= 2 || hasGameEnded(&game_board)) {
                broadcast_game_over();
            } else {
                current_player_idx = next_player_idx;
                send_your_turn(current_player_idx);
            }
            
            log_game_state("패스", client_idx, &move);
            return;
        }
    }
    
    // 일반 이동 유효성 검사
    if (!isValidMove(&game_board, &move)) {
        printf("유효하지 않은 이동\n");
        
        // 다시 턴 요청
        send_your_turn(client_idx);
        return;
    }
    
    // 이동 적용
    applyMove(&game_board, &move);
    
    // 연속 패스 초기화
    game_board.consecutivePasses = 0;
    
    log_game_state("이동", client_idx, &move);
    
    // 모든 클라이언트에게 보드 상태 전송 (단순화)
    printf("보드 업데이트 전송\n");
    printBoard(&game_board);
    
    // 게임 종료 확인
    if (hasGameEnded(&game_board)) {
        broadcast_game_over();
    } else {
        // 다음 플레이어 턴
        current_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
        send_your_turn(current_player_idx);
    }
    
    // username 메모리 해제
    if (username) {
        free(username);
    }
}

// 게임 시작 브로드캐스트
void broadcast_game_start() {
    server_state = SERVER_GAME_IN_PROGRESS;
    
    const char *usernames[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        usernames[i] = clients[i].username;
    }
    
    JsonValue *start_msg = createGameStartMessage(usernames, clients[0].username);
    char *json_str = json_stringify(start_msg);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
            send(clients[i].socket, "\n", 1, 0);
        }
    }
    
    free(json_str);
    json_free(start_msg);
    
    printf("게임 시작: %s vs %s\n", clients[0].username, clients[1].username);
    
    // 첫 번째 플레이어에게 턴 시작 알림
    current_player_idx = 0;
    send_your_turn(current_player_idx);
}

// 턴 시작 메시지 전송
void send_your_turn(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS || clients[client_idx].socket == -1) {
        return;
    }
    
    JsonValue *turn_msg = createYourTurnMessage(&game_board, TIMEOUT_SEC);
    char *json_str = json_stringify(turn_msg);
    
    send(clients[client_idx].socket, json_str, strlen(json_str), 0);
    send(clients[client_idx].socket, "\n", 1, 0);
    free(json_str);
    json_free(turn_msg);
    
    // 턴 시작 시간 기록
    gettimeofday(&turn_start_time, NULL);
    
    printf("플레이어 %s의 턴 시작\n", clients[client_idx].username);
}

// 게임 종료 브로드캐스트
void broadcast_game_over() {
    server_state = SERVER_GAME_OVER;
    
    // 승자 결정
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
    
    free(json_str);
    json_free(game_over_msg);
    
    if (game_board.redCount > game_board.blueCount) {
        printf("게임 종료: %s 승리! (R=%d, B=%d)\n", 
               clients[0].username, game_board.redCount, game_board.blueCount);
    } else if (game_board.blueCount > game_board.redCount) {
        printf("게임 종료: %s 승리! (R=%d, B=%d)\n", 
               clients[1].username, game_board.redCount, game_board.blueCount);
    } else {
        printf("게임 종료: 무승부! (R=%d, B=%d)\n", 
               game_board.redCount, game_board.blueCount);
    }
    
    log_game_state("게임종료", -1, NULL);
    
    // 게임 상태 초기화
    server_state = SERVER_WAITING_PLAYERS;
    initializeBoard(&game_board);
    current_player_idx = 0;
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
    
    // 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        memset(clients[i].username, 0, sizeof(clients[i].username));
    }
    
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
                int valread = read(fds[i].fd, buffer, BUFFER_SIZE - 1);
                
                if (valread > 0) {
                    buffer[valread] = '\0';
                    
                    // 해당 클라이언트 인덱스 찾기
                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == fds[i].fd) {
                            client_idx = j;
                            break;
                        }
                    }
                    
                    if (client_idx != -1) {
                        handle_client_message(client_idx, buffer);
                    }
                } else {
                    // 연결 종료 또는 오류
                    handle_client_disconnect(fds[i].fd);
                    
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