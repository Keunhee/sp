#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include "octaflip.h"
#include "json.h"
#include "message_handler.h"
#include "led_matrix.h"
#include "ai_engine.h"

#define BUFFER_SIZE 1024

// 클라이언트 상태
typedef enum {
    CLIENT_CONNECTING,
    CLIENT_REGISTERING,
    CLIENT_WAITING,
    CLIENT_YOUR_TURN,
    CLIENT_GAME_OVER
} ClientState;

// 전역 변수 (pthread 제거됨)
int client_socket = -1;
ClientState client_state = CLIENT_CONNECTING;
GameBoard game_board;
char my_username[64];
char my_color;
char opponent_username[64];
int led_enabled = 0;

// 함수 선언
void handle_server_message(char *buffer);
void send_register_message();
void send_move_message(Move *move);
Move generate_smart_move();
void cleanup_and_exit(int status);
void sigint_handler(int sig);
int receive_message(char *buffer, int buffer_size);

// 정리 및 종료 함수
void cleanup_and_exit(int status) {
    if (client_socket != -1) {
        close(client_socket);
    }
    
    if (led_enabled) {
        ledMatrixClose();
    }
    
    exit(status);
}

// SIGINT 핸들러 (Ctrl+C)
void sigint_handler(int sig __attribute__((unused))) {
    printf("\n프로그램을 종료합니다.\n");
    cleanup_and_exit(0);
}

// 등록 메시지 전송
void send_register_message() {
    JsonValue *json_obj = createRegisterMessage(my_username);
    char *json_str = json_stringify(json_obj);
    
    send(client_socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    json_free(json_obj);
    
    printf("서버에 등록 요청을 보냈습니다.\n");
    client_state = CLIENT_REGISTERING;
}

// 이동 메시지 전송
void send_move_message(Move *move) {
    // **화면에 출력할 땐 1-base로 보여주기**
    if (move->sourceRow == 0 && move->sourceCol == 0 &&
        move->targetRow == 0 && move->targetCol == 0) {
        
        printf("이동 (패스) 를 서버에 전송합니다.\n");
    } else {
        printf("이동 (%d,%d) -> (%d,%d) 를 서버에 전송합니다.\n",
               move->sourceRow + 1,   // 화면 출력은 1-base
               move->sourceCol + 1,
               move->targetRow + 1,
               move->targetCol + 1);
    }

   
    JsonValue *json_obj = createMoveMessage(my_username, move);
    char *json_str = json_stringify(json_obj);

    send(client_socket, json_str, strlen(json_str), 0);
    send(client_socket, "\n", 1, 0);

    free(json_str);
    json_free(json_obj);
}


/**
 * 강력한 AI 엔진을 사용한 최적 이동 생성 함수 (pthread 제거됨)
 */
Move generate_smart_move() {
    printf("\n=== AI 엔진 시작 ===\n");
    printf("현재 플레이어: %c\n", my_color);
    printf("보드 상태: R=%d, B=%d, Empty=%d\n", 
           game_board.redCount, game_board.blueCount, game_board.emptyCount);
    
    // 강력한 AI 엔진을 사용하여 최적 이동 생성
    Move best_move = generateWinningMove(&game_board, my_color);
    
    if (best_move.sourceRow == 0 && best_move.sourceCol == 0 && 
        best_move.targetRow == 0 && best_move.targetCol == 0) {
        printf("AI 판단: 유효한 이동이 없어 패스합니다.\n");
    } else {
        printf("AI 선택: (%d,%d) -> (%d,%d)\n", 
               best_move.sourceRow, best_move.sourceCol, 
               best_move.targetRow, best_move.targetCol);
    }
    printf("=== AI 엔진 종료 ===\n\n");
    
    return best_move;
}

// 메시지 수신 함수 (비차단)
int receive_message(char *buffer, int buffer_size) {
    int bytes_received = recv(client_socket, buffer, buffer_size - 1, MSG_DONTWAIT);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        return bytes_received;
    }
    return 0;
}

// 서버 메시지 처리 (pthread 제거됨)
void handle_server_message(char *buffer) {
    JsonValue *json_obj = json_parse(buffer);
    if (!json_obj) {
        fprintf(stderr, "유효하지 않은 JSON 메시지: %s\n", buffer);
        return;
    }
    
    MessageType msg_type = parseMessageType(json_obj);
    
    switch (msg_type) {
        case MSG_REGISTER_ACK: {
            printf("등록 성공! 다른 플레이어를 기다립니다...\n");
            client_state = CLIENT_WAITING;
            break;
        }
        
        case MSG_REGISTER_NACK: {
            JsonValue *reason_json = json_object_get(json_obj, "reason");
            if (reason_json && json_is_string(reason_json)) {
                printf("등록 실패: %s\n", json_string_value(reason_json));
            } else {
                printf("등록 실패: 알 수 없는 이유\n");
            }
            cleanup_and_exit(1);
            break;
        }
        
        case MSG_GAME_START: {
            char players[2][64];
            char first_player[64];
            
            if (parseGameStartMessage(json_obj, players, first_player)) {
                printf("게임 시작! 플레이어: %s vs %s\n", players[0], players[1]);
                printf("첫 번째 플레이어: %s\n", first_player);
                
                // 상대 플레이어 이름 저장
                if (strcmp(players[0], my_username) == 0) {
                    strncpy(opponent_username, players[1], sizeof(opponent_username) - 1);
                    opponent_username[sizeof(opponent_username) - 1] = '\0';
                } else {
                    strncpy(opponent_username, players[0], sizeof(opponent_username) - 1);
                    opponent_username[sizeof(opponent_username) - 1] = '\0';
                }
                
                // 내 색상 결정
                if (strcmp(players[0], my_username) == 0) {
                    my_color = RED_PLAYER;
                } else {
                    my_color = BLUE_PLAYER;
                }
                
                printf("내 색상: %c\n", my_color);
                
                // 보드 초기화
                initializeBoard(&game_board);
                
                // LED 매트릭스에 초기 보드 표시
                if (led_enabled) {
                    drawBoardOnLED(&game_board);
                }
                
                client_state = CLIENT_WAITING;
            }
            break;
        }
        
        case MSG_YOUR_TURN: {
            double timeout;
            
            if (parseYourTurnMessage(json_obj, &game_board, &timeout)) {
                printf("당신의 차례입니다. 제한 시간: %.1f초\n", timeout);
                
                // 보드 출력
                printf("현재 보드 상태:\n");
                printBoard(&game_board);
                
                // LED 매트릭스에 보드 표시
                if (led_enabled) {
                    drawBoardOnLED(&game_board);
                }
                
                client_state = CLIENT_YOUR_TURN;
                
                // 자동 이동 생성 및 전송
                Move move = generate_smart_move();
                
                // 이동 전 유효성 로컬 검증
                int valid = isValidMove(&game_board, &move);
                
                if (valid || (move.sourceRow == 0 && move.sourceCol == 0 && 
                             move.targetRow == 0 && move.targetCol == 0)) {
                    send_move_message(&move);
                    client_state = CLIENT_WAITING;
                } else {
                    printf("로컬 검증 실패. 패스합니다.\n");
                    Move pass_move = {my_color, 0, 0, 0, 0};
                    send_move_message(&pass_move);
                    client_state = CLIENT_WAITING;
                }
            }
            break;
        }
        
        case MSG_PASS: {
            printf("패스 메시지 수신\n");
            // 패스 메시지는 단순히 로그만 출력
            break;
        }
        
        case MSG_GAME_OVER: {
            char players[2][64];
            int scores[2];
            
            if (parseGameOverMessage(json_obj, players, scores)) {
                if (scores[0] > scores[1]) {
                    printf("게임 종료! 승자: %s\n", players[0]);
                    if (strcmp(players[0], my_username) == 0) {
                        printf("축하합니다! 당신이 승리했습니다!\n");
                    } else {
                        printf("아쉽습니다. 다음 기회에 도전하세요.\n");
                    }
                } else if (scores[1] > scores[0]) {
                    printf("게임 종료! 승자: %s\n", players[1]);
                    if (strcmp(players[1], my_username) == 0) {
                        printf("축하합니다! 당신이 승리했습니다!\n");
                    } else {
                        printf("아쉽습니다. 다음 기회에 도전하세요.\n");
                    }
                } else {
                    printf("게임 종료! 무승부입니다.\n");
                }
                
                client_state = CLIENT_GAME_OVER;
            }
            break;
        }
        
        default:
            fprintf(stderr, "알 수 없는 메시지 유형\n");
            break;
    }
    
    json_free(json_obj);
}

// 메인 함수 (pthread 제거됨)
int main(int argc, char *argv[]) {
    char ip_address[32] = "127.0.0.1";
    int port = 8888;
    
    // 명령줄 인수 파싱
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            strncpy(ip_address, argv[i + 1], sizeof(ip_address) - 1);
            ip_address[sizeof(ip_address) - 1] = '\0';
            i++;
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-username") == 0 && i + 1 < argc) {
            strncpy(my_username, argv[i + 1], sizeof(my_username) - 1);
            my_username[sizeof(my_username) - 1] = '\0';
            i++;
        } else if (strcmp(argv[i], "-led") == 0) {
            led_enabled = 1;
        } else {
            printf("사용법: %s -ip <IP주소> -port <포트> -username <사용자명> [-led]\n", argv[0]);
            return 1;
        }
    }
    
    // 사용자 이름 확인
    if (strlen(my_username) == 0) {
        printf("사용자 이름을 입력하세요: ");
        if (fgets(my_username, sizeof(my_username), stdin) == NULL) {
            fprintf(stderr, "입력 오류\n");
            return 1;
        }
        
        // 개행 문자 제거
        size_t len = strlen(my_username);
        if (len > 0 && my_username[len - 1] == '\n') {
            my_username[len - 1] = '\0';
        }
    }
    
    // 시그널 핸들러 설정
    signal(SIGINT, sigint_handler);
    
    // LED 매트릭스 초기화
    if (led_enabled) {
        if (ledMatrixInit() != 0) {
            fprintf(stderr, "LED 매트릭스 초기화 실패\n");
            led_enabled = 0;
        }
    }
    
    // 소켓 생성
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        cleanup_and_exit(1);
    }
    
    // 서버 주소 설정
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    // IP 주소 변환
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        perror("유효하지 않은 주소/주소가 지원되지 않음");
        cleanup_and_exit(1);
    }
    
    // 서버에 연결
    if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("연결 실패");
        cleanup_and_exit(1);
    }
    
    printf("서버에 연결되었습니다: %s:%d\n", ip_address, port);
    
    // 등록 메시지 전송
    send_register_message();
    
    // 메인 루프 (pthread 대신 poll 사용)
    char buffer[BUFFER_SIZE];
    struct pollfd fds[1];
    fds[0].fd = client_socket;
    fds[0].events = POLLIN;
    
    while (client_state != CLIENT_GAME_OVER) {
        // poll로 소켓 상태 확인 (100ms 타임아웃)
        int poll_result = poll(fds, 1, 100);
        
        if (poll_result > 0 && (fds[0].revents & POLLIN)) {
            // 메시지 수신
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("서버로부터 메시지: %s\n", buffer);
                handle_server_message(buffer);
            } else if (bytes_received == 0) {
                printf("서버 연결이 종료되었습니다.\n");
                break;
            } else {
                perror("메시지 수신 오류");
                break;
            }
        } else if (poll_result < 0 && errno != EINTR) {
            perror("poll 오류");
            break;
        }
        
        // 100ms 대기 (CPU 사용률 감소)
        usleep(100000);
    }
    
    printf("게임이 종료되었습니다.\n");
    
    // 정리 및 종료
    cleanup_and_exit(0);
    
    return 0;
}