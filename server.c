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

// 타임아웃 확인
void check_timeout() {
    if (server_state != SERVER_GAME_IN_PROGRESS) {
        return;
    }
    
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    double elapsed = (current_time.tv_sec - turn_start_time.tv_sec) + 
                    (current_time.tv_usec - turn_start_time.tv_usec) / 1000000.0;
    
    if (elapsed > TIMEOUT_SEC) {
        printf("플레이어 %s 타임아웃. 턴을 넘깁니다.\n", clients[current_player_idx].username);
        
        // 패스 메시지 보내기
        int next_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
        JsonValue *pass_msg = createPassMessage(clients[next_player_idx].username);
        char *json_str = json_stringify(pass_msg);
        
        // 모든 클라이언트에게 패스 메시지 전송
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                send(clients[i].socket, json_str, strlen(json_str), 0);
            }
        }
        
        free(json_str);
        json_free(pass_msg);
        
        // 다음 플레이어로 변경
        current_player_idx = next_player_idx;
        gettimeofday(&turn_start_time, NULL);
        send_your_turn(current_player_idx);
    }
}

// 게임 상태 로깅
void log_game_state(const char *action, int player_idx, Move *move) {
    static FILE *log_file = NULL;
    
    // 로그 파일 초기화
    if (log_file == NULL) {
        log_file = fopen("game_log.txt", "w");
        if (log_file == NULL) {
            perror("로그 파일 열기 실패");
            return;
        }
    }
    
    // 현재 시간 가져오기
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // 로그 작성
    fprintf(log_file, "[%s] %s", time_str, action);
    
    if (player_idx >= 0) {
        fprintf(log_file, " - Player: %s (%c)",
                clients[player_idx].username, clients[player_idx].color);
    }
    
    fprintf(log_file, "\n");
    
    // 이동 정보 기록
    if (move != NULL) {
        fprintf(log_file, "Move: (%d,%d) -> (%d,%d)\n", 
                move->sourceRow, move->sourceCol, move->targetRow, move->targetCol);
    }
    
    // 보드 상태 기록
    fprintf(log_file, "Board state:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(log_file, "%s\n", game_board.cells[i]);
    }
    fprintf(log_file, "Red: %d, Blue: %d, Empty: %d\n\n", 
            game_board.redCount, game_board.blueCount, game_board.emptyCount);
    
    // 파일 즉시 저장
    fflush(log_file);
}

// 게임 시작 메시지 브로드캐스트
void broadcast_game_start() {
    char *players[2];
    players[0] = clients[0].username;
    players[1] = clients[1].username;
    
    // 게임 시작 메시지
    const char *player_ptrs[2];
    player_ptrs[0] = players[0];
    player_ptrs[1] = players[1];
    
    JsonValue *start_msg = createGameStartMessage(player_ptrs, players[0]);
    char *json_str = json_stringify(start_msg);
    
    // 모든 클라이언트에게 전송
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
        }
    }
    
    free(json_str);
    json_free(start_msg);
    
    // 첫 번째 플레이어에게 턴 전송
    gettimeofday(&turn_start_time, NULL);
    send_your_turn(0);
    
    // 로그 작성
    log_game_state("Game started", -1, NULL);
}

// 게임 종료 메시지 브로드캐스트
void broadcast_game_over() {
    char *players[2];
    int scores[2];
    
    players[0] = clients[0].username;
    players[1] = clients[1].username;
    
    // 각 플레이어의 점수 계산
    if (clients[0].color == RED_PLAYER) {
        scores[0] = game_board.redCount;
        scores[1] = game_board.blueCount;
    } else {
        scores[0] = game_board.blueCount;
        scores[1] = game_board.redCount;
    }
    
    const char *player_ptrs[2];
    player_ptrs[0] = players[0];
    player_ptrs[1] = players[1];
    
    JsonValue *end_msg = createGameOverMessage(player_ptrs, scores);
    char *json_str = json_stringify(end_msg);
    
    // 모든 클라이언트에게 전송
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
        }
    }
    
    free(json_str);
    json_free(end_msg);
    
    // 서버 상태 업데이트
    server_state = SERVER_GAME_OVER;
    
    // 로그 작성
    log_game_state("Game over", -1, NULL);
    
    // 게임 결과 콘솔에 출력
    printf("게임 종료! 최종 점수 - %s: %d, %s: %d\n", 
           players[0], scores[0], players[1], scores[1]);
}

// 당신 차례 메시지 전송
void send_your_turn(int client_idx) {
    JsonValue *turn_msg = createYourTurnMessage(&game_board, TIMEOUT_SEC);
    char *json_str = json_stringify(turn_msg);
    
    send(clients[client_idx].socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    json_free(turn_msg);
    
    // 로그 작성
    log_game_state("Your turn", client_idx, NULL);
    
    printf("플레이어 %s의 차례입니다.\n", clients[client_idx].username);
}

// 등록 메시지 처리
void handle_register_message(int client_idx, JsonValue *json_obj) {
    char *username = parseRegisterMessage(json_obj);
    
    if (username == NULL) {
        fprintf(stderr, "유효하지 않은 등록 메시지\n");
        return;
    }
    
    // 이미 등록된 사용자인지 확인
    for (int i = 0; i < client_count; i++) {
        if (i != client_idx && strcmp(clients[i].username, username) == 0) {
            // 이미 등록된 사용자
            JsonValue *nack_msg = createRegisterNackMessage("Username already exists");
            char *json_str = json_stringify(nack_msg);
            
            send(clients[client_idx].socket, json_str, strlen(json_str), 0);
            
            free(json_str);
            json_free(nack_msg);
            free(username);
            return;
        }
    }
    
    // 게임이 이미 진행 중인지 확인
    if (server_state == SERVER_GAME_IN_PROGRESS) {
        JsonValue *nack_msg = createRegisterNackMessage("game is already running");
        char *json_str = json_stringify(nack_msg);
        
        send(clients[client_idx].socket, json_str, strlen(json_str), 0);
        
        free(json_str);
        json_free(nack_msg);
        free(username);
        return;
    }
    
    // 사용자 등록
    strncpy(clients[client_idx].username, username, sizeof(clients[client_idx].username) - 1);
    
    // 사용자 색상 할당
    clients[client_idx].color = (client_idx == 0) ? RED_PLAYER : BLUE_PLAYER;
    
    // 등록 성공 메시지
    JsonValue *ack_msg = createRegisterAckMessage();
    char *json_str = json_stringify(ack_msg);
    
    send(clients[client_idx].socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    json_free(ack_msg);
    free(username);
    
    printf("플레이어 %s가 등록되었습니다. (%c)\n", 
           clients[client_idx].username, clients[client_idx].color);
    
    // 두 명의 플레이어가 모두 등록되었는지 확인
    if (client_count == MAX_CLIENTS) {
        printf("두 명의 플레이어가 모두 등록되었습니다. 게임을 시작합니다.\n");
        server_state = SERVER_GAME_IN_PROGRESS;
        broadcast_game_start();
    }
}

// 이동 메시지 처리
void handle_move_message(int client_idx, JsonValue *json_obj) {
    char *username;
    Move move;
    
    if (!parseMoveMessage(json_obj, &username, &move)) {
        fprintf(stderr, "유효하지 않은 이동 메시지\n");
        return;
    }
    
    // 현재 플레이어 차례인지 확인
    if (client_idx != current_player_idx) {
        fprintf(stderr, "현재 턴이 아닌 플레이어의 이동 시도\n");
        free(username);
        return;
    }
    
    // 패스 확인 (모든 좌표가 0인 경우)
    // 참고: 좌표는 0부터 시작하므로 실제로는 (0,0) -> (0,0) 이동도 가능한 경우가 있음
    // 여기서는 명시적으로 패스를 (0,0,0,0)으로 약속하였으므로 이 값을 사용
    if (move.sourceRow == 0 && move.sourceCol == 0 && 
        move.targetRow == 0 && move.targetCol == 0) {
        
        // 유효한 이동이 있는지 확인
        if (hasValidMove(&game_board, clients[client_idx].color)) {
            // 유효한 이동이 있으면 패스 불가
            JsonValue *invalid_msg = createInvalidMoveMessage(&game_board, clients[client_idx].username);
            char *json_str = json_stringify(invalid_msg);
            
            send(clients[client_idx].socket, json_str, strlen(json_str), 0);
            
            free(json_str);
            json_free(invalid_msg);
        } else {
            // 유효한 이동이 없으면 패스
            game_board.consecutivePasses++;
            
            // 다음 플레이어
            int next_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
            
            // 패스 메시지 전송
            JsonValue *pass_msg = createPassMessage(clients[next_player_idx].username);
            char *json_str = json_stringify(pass_msg);
            
            // 모든 클라이언트에게 전송
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket != -1) {
                    send(clients[i].socket, json_str, strlen(json_str), 0);
                }
            }
            
            free(json_str);
            json_free(pass_msg);
            
            printf("플레이어 %s가 패스했습니다.\n", clients[client_idx].username);
            
            // 게임 종료 확인
            if (hasGameEnded(&game_board)) {
                broadcast_game_over();
                free(username);
                return;
            }
            
            // 다음 플레이어 턴
            current_player_idx = next_player_idx;
            gettimeofday(&turn_start_time, NULL);
            send_your_turn(current_player_idx);
        }
        
        free(username);
        return;
    }
    
    // 플레이어의 말 설정
    move.player = clients[client_idx].color;
    
    // 이동 유효성 검사
    if (!isValidMove(&game_board, &move)) {
        // 유효하지 않은 이동 로그
        printf("유효하지 않은 이동 검출: 플레이어=%c, (%d,%d) -> (%d,%d)\n", 
               move.player, move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
        
        // 유효하지 않은 이동
        JsonValue *invalid_msg = createInvalidMoveMessage(&game_board, clients[client_idx].username);
        char *json_str = json_stringify(invalid_msg);
        
        send(clients[client_idx].socket, json_str, strlen(json_str), 0);
        
        free(json_str);
        json_free(invalid_msg);
        
        printf("플레이어 %s의 유효하지 않은 이동: (%d,%d) -> (%d,%d)\n",
               clients[client_idx].username, move.sourceRow, move.sourceCol, 
               move.targetRow, move.targetCol);
        
        free(username);
        return;
    }
    
    // 연속 패스 초기화
    game_board.consecutivePasses = 0;
    
    // 이동 적용
    applyMove(&game_board, &move);
    
    // 로그 작성
    log_game_state("Move applied", client_idx, &move);
    
    // 다음 플레이어
    int next_player_idx = (current_player_idx + 1) % MAX_CLIENTS;
    
    // 이동 성공 메시지 전송
    JsonValue *ok_msg = createMoveOkMessage(&game_board, clients[next_player_idx].username);
    char *json_str = json_stringify(ok_msg);
    
    // 모든 클라이언트에게 전송
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, json_str, strlen(json_str), 0);
        }
    }
    
    free(json_str);
    json_free(ok_msg);
    
    printf("플레이어 %s의 이동: (%d,%d) -> (%d,%d)\n",
           clients[client_idx].username, move.sourceRow, move.sourceCol, 
           move.targetRow, move.targetCol);
    
    // 게임 종료 확인
    if (hasGameEnded(&game_board)) {
        broadcast_game_over();
        free(username);
        return;
    }
    
    // 다음 플레이어 턴
    current_player_idx = next_player_idx;
    gettimeofday(&turn_start_time, NULL);
    send_your_turn(current_player_idx);
    
    free(username);
}

// 클라이언트 메시지 처리
void handle_client_message(int client_idx, char *buffer) {
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
    int server_fd, new_socket, max_sd, activity;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    
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
    
    while (1) {
        // fd_set 초기화
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        
        // 클라이언트 소켓 추가
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            
            // 유효한 소켓이면 fd_set에 추가
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            
            // 최대 소켓 번호 갱신
            if (sd > max_sd) {
                max_sd = sd;
            }
        }
        
        // 타임아웃 설정 (0.1초)
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        
        // select 호출
        activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            perror("select error");
        }
        
        // 타임아웃 확인
        check_timeout();
        
        // 새 연결 확인
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            
            // 비차단 모드로 설정
            set_socket_nonblocking(new_socket);
            
            printf("새 연결, 소켓 fd: %d, IP: %s, 포트: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            
            // 최대 두 명의 클라이언트만 허용
            if (client_count < MAX_CLIENTS) {
                clients[client_count].socket = new_socket;
                client_count++;
            } else {
                // 추가 연결 거부
                close(new_socket);
                printf("최대 클라이언트 수에 도달. 연결 거부.\n");
            }
        }
        
        // 클라이언트 메시지 확인
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            
            if (FD_ISSET(sd, &readfds)) {
                // 클라이언트로부터 데이터 읽기
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                
                // 버퍼 오버플로우 방지
                if (valread > 0) {
                    buffer[valread] = '\0';
                }
                
                if (valread <= 0) {
                    // 연결 종료
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("연결 종료, IP: %s, 포트: %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    
                    // 소켓 닫기
                    close(sd);
                    clients[i].socket = -1;
                    
                    // 게임 중이면 다른 플레이어에게 패스 메시지 전송
                    if (server_state == SERVER_GAME_IN_PROGRESS) {
                        if (i == current_player_idx) {
                            int other_idx = (i + 1) % MAX_CLIENTS;
                            if (clients[other_idx].socket != -1) {
                                // 패스 메시지 전송
                                JsonValue *pass_msg = createPassMessage(clients[other_idx].username);
                                char *json_str = json_stringify(pass_msg);
                                
                                send(clients[other_idx].socket, json_str, strlen(json_str), 0);
                                
                                free(json_str);
                                json_free(pass_msg);
                                
                                // 다음 플레이어 턴
                                current_player_idx = other_idx;
                                gettimeofday(&turn_start_time, NULL);
                                send_your_turn(current_player_idx);
                            } else {
                                // 모든 플레이어 연결 해제, 게임 종료
                                server_state = SERVER_WAITING_PLAYERS;
                                printf("모든 플레이어 연결 해제. 게임 종료.\n");
                            }
                        }
                    }
                } else {
                    // 버퍼 NULL 종료
                    buffer[valread] = '\0';
                    
                    // 메시지 처리
                    handle_client_message(i, buffer);
                }
            }
        }
    }
    
    return 0;
}