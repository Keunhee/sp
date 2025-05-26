#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "octaflip.h"
#include "json.h"
#include "message_handler.h"
#include "led_matrix.h"

#define BUFFER_SIZE 1024

// 클라이언트 상태
typedef enum {
    CLIENT_CONNECTING,
    CLIENT_REGISTERING,
    CLIENT_WAITING,
    CLIENT_YOUR_TURN,
    CLIENT_GAME_OVER
} ClientState;

// 전역 변수
int client_socket = -1;
ClientState client_state = CLIENT_CONNECTING;
GameBoard game_board;
char my_username[64];
char my_color;
char opponent_username[64];
pthread_t receiver_thread;
pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;
int led_enabled = 0;

// 함수 선언
void *receiver_function(void *arg);
void handle_server_message(char *buffer);
void send_register_message();
void send_move_message(Move *move);
Move generate_smart_move();
void cleanup_and_exit(int status);
void sigint_handler(int sig);

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
    JsonValue *json_obj = createMoveMessage(my_username, move);
    char *json_str = json_stringify(json_obj);
    
    send(client_socket, json_str, strlen(json_str), 0);
    
    free(json_str);
    json_free(json_obj);
    
    printf("이동 (%d,%d) -> (%d,%d)를 서버에 전송했습니다.\n", 
           move->sourceRow, move->sourceCol, move->targetRow, move->targetCol);
}

/**
 * 스마트 이동 생성 함수 (간단한 휴리스틱 적용)
 * 
 * 현재 게임 상태를 분석하여 가장 좋은 이동을 생성합니다.
 * 사용된 휴리스틱:
 * - 각 가능한 이동에 대해 이동 후 상태를 시뮬레이션
 * - 점수 = (자신의 말 수) - (상대의 말 수)
 * - 가장 높은 점수를 가진 이동 선택
 * 
 * @return 생성된 최적의 이동
 */
Move generate_smart_move() {
    pthread_mutex_lock(&board_mutex);
    
    Move best_move;
    best_move.player = my_color;
    best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
    
    int best_score = -1;
    
    // 모든 가능한 이동 탐색
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            // 현재 플레이어의 말이 있는 위치만 고려
            if (game_board.cells[r][c] != my_color) continue;
            
            // 8방향으로 이동 가능성 탐색
            for (int d = 0; d < 8; d++) {
                // 1칸 또는 2칸 이동 시도
                for (int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    
                    // 보드 범위 검사
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    
                    // 2칸 이동 시 중간 칸이 비어있는지 검사
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (game_board.cells[mr][mc] == RED_PLAYER || 
                            game_board.cells[mr][mc] == BLUE_PLAYER) continue;
                    }
                    
                    // 목적지가 빈 칸인지 검사
                    if (game_board.cells[nr][nc] == EMPTY_CELL) {
                        // 임시 이동 생성
                        Move move;
                        move.player = my_color;
                        move.sourceRow = r;
                        move.sourceCol = c;
                        move.targetRow = nr;
                        move.targetCol = nc;
                        
                        // 임시 보드 생성하여 이동 시뮬레이션
                        GameBoard temp_board;
                        memcpy(&temp_board, &game_board, sizeof(GameBoard));
                        
                        // 이동 적용
                        applyMove(&temp_board, &move);
                        
                        // 점수 계산 (자신의 말 수 - 상대 말 수)
                        int my_pieces = (my_color == RED_PLAYER) ? temp_board.redCount : temp_board.blueCount;
                        int opponent_pieces = (my_color == RED_PLAYER) ? temp_board.blueCount : temp_board.redCount;
                        int score = my_pieces - opponent_pieces;
                        
                        // 더 좋은 이동을 찾았으면 업데이트
                        if (score > best_score) {
                            best_score = score;
                            best_move = move;
                        }
                    }
                }
            }
        }
    }
    
    pthread_mutex_unlock(&board_mutex);
    
    // 유효한 이동이 없으면 패스 (0,0,0,0)
    if (best_score == -1) {
        printf("유효한 이동이 없습니다. 패스합니다.\n");
        best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
    }
    
    return best_move;
}

// 서버 메시지 처리
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
            // 등록 실패 처리
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
                pthread_mutex_lock(&board_mutex);
                initializeBoard(&game_board);
                pthread_mutex_unlock(&board_mutex);
                
                // LED 매트릭스에 초기 보드 표시
                if (led_enabled) {
                    drawBoardOnLED(&game_board);
                }
                
                client_state = CLIENT_WAITING;
            }
            break;
        }
        
        case MSG_YOUR_TURN: {
            pthread_mutex_lock(&board_mutex);
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
            }
            pthread_mutex_unlock(&board_mutex);
            
            // 자동 이동 생성 및 전송
            if (client_state == CLIENT_YOUR_TURN) {
                Move move = generate_smart_move();
                // 이동 전 유효성 로컬 검증
                pthread_mutex_lock(&board_mutex);
                int valid = isValidMove(&game_board, &move);
                pthread_mutex_unlock(&board_mutex);
                
                if (valid) {
                    send_move_message(&move);
                    client_state = CLIENT_WAITING;
                } else {
                    printf("경고: 생성된 이동이 유효하지 않습니다. 패스합니다.\n");
                    move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
                    send_move_message(&move);
                    client_state = CLIENT_WAITING;
                }
            }
            break;
        }
        
        case MSG_MOVE_OK:
        case MSG_INVALID_MOVE:
        case MSG_PASS: {
            pthread_mutex_lock(&board_mutex);
            char next_player[64];
            
            if (parseMoveResultMessage(json_obj, &game_board, next_player)) {
                if (msg_type == MSG_MOVE_OK) {
                    printf("이동 성공. 다음 플레이어: %s\n", next_player);
                } else if (msg_type == MSG_INVALID_MOVE) {
                    printf("유효하지 않은 이동. 다음 플레이어: %s\n", next_player);
                } else {
                    printf("패스. 다음 플레이어: %s\n", next_player);
                }
                
                // 보드 출력
                if (msg_type != MSG_PASS) {
                    printf("현재 보드 상태:\n");
                    printBoard(&game_board);
                    
                    // LED 매트릭스에 보드 표시
                    if (led_enabled) {
                        drawBoardOnLED(&game_board);
                    }
                }
                
                // 내 차례인지 확인
                if (strcmp(next_player, my_username) == 0) {
                    client_state = CLIENT_YOUR_TURN;
                    
                    // 자동 이동 생성 및 전송
                    Move move = generate_smart_move();
                    // 이동 전 유효성 로컬 검증
                    int valid = isValidMove(&game_board, &move);
                    
                    if (valid) {
                        send_move_message(&move);
                        client_state = CLIENT_WAITING;
                    } else {
                        printf("경고: 생성된 이동이 유효하지 않습니다. 패스합니다.\n");
                        move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
                        send_move_message(&move);
                        client_state = CLIENT_WAITING;
                    }
                } else {
                    client_state = CLIENT_WAITING;
                }
            }
            pthread_mutex_unlock(&board_mutex);
            break;
        }
        
        case MSG_GAME_OVER: {
            char players[2][64];
            int scores[2];
            
            if (parseGameOverMessage(json_obj, players, scores)) {
                printf("게임 종료!\n");
                printf("최종 점수: %s: %d, %s: %d\n", 
                       players[0], scores[0], players[1], scores[1]);
                
                // 승자 결정
                if (scores[0] > scores[1]) {
                    printf("승자: %s\n", players[0]);
                } else if (scores[1] > scores[0]) {
                    printf("승자: %s\n", players[1]);
                } else {
                    printf("무승부!\n");
                }
                
                // 현재 보드 출력
                printf("최종 보드 상태:\n");
                printBoard(&game_board);
                
                // LED 매트릭스에 최종 보드 표시
                if (led_enabled) {
                    drawBoardOnLED(&game_board);
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

// 수신 스레드 함수
void *receiver_function(void *arg __attribute__((unused))) {
    char buffer[BUFFER_SIZE];
    
    while (1) {
        // 서버로부터 데이터 수신
        int valread = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (valread > 0) {
            // 항상 null 종료 보장
            if (valread < BUFFER_SIZE)
                buffer[valread] = '\0';
            else
                buffer[BUFFER_SIZE - 1] = '\0';
            handle_server_message(buffer);
            
            // 게임 종료 확인
            if (client_state == CLIENT_GAME_OVER) {
                break;
            }
        } else if (valread == 0) {
            // 서버 연결 종료
            printf("서버와의 연결이 종료되었습니다.\n");
            break;
        } else {
            // 오류 발생
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv failed");
                break;
            }
            
            // 잠시 대기
            usleep(10000);  // 10ms
        }
    }
    
    return NULL;
}

// 메인 함수
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
    
    printf("서버에 연결되었습니다. (IP: %s, 포트: %d)\n", ip_address, port);
    
    // 비차단 모드로 설정
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    
    // 등록 메시지 전송
    send_register_message();
    
    // 수신 스레드 생성
    if (pthread_create(&receiver_thread, NULL, receiver_function, NULL) != 0) {
        perror("스레드 생성 실패");
        cleanup_and_exit(1);
    }
    
    // 메인 루프
    while (client_state != CLIENT_GAME_OVER) {
        // 잠시 대기
        usleep(100000);  // 100ms
        
        // YOUR_TURN 상태인 경우 자동 이동 생성 및 전송
        if (client_state == CLIENT_YOUR_TURN) {
            Move move = generate_smart_move();
            // 이동 전 유효성 로컬 검증
            pthread_mutex_lock(&board_mutex);
            int valid = isValidMove(&game_board, &move);
            pthread_mutex_unlock(&board_mutex);
            
            if (valid) {
                send_move_message(&move);
                client_state = CLIENT_WAITING;
            } else {
                printf("경고: 생성된 이동이 유효하지 않습니다. 패스합니다.\n");
                move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
                send_move_message(&move);
                client_state = CLIENT_WAITING;
            }
        }
    }
    
    // 스레드 조인
    pthread_join(receiver_thread, NULL);
    
    // 정리 및 종료
    cleanup_and_exit(0);
    
    return 0;
}