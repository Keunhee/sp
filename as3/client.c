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
#include "json.h"
#include "message_handler.h"
#include "board.h"
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
int led_enabled = 1;

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
    send(client_socket, "\n", 1, 0);

    free(json_str);
    json_free(json_obj);

    printf("[Client] Sent register: %s\n", my_username);
    client_state = CLIENT_REGISTERING;
}

// 이동 메시지 전송
void send_move_message(Move *move) {
    if (move->sourceRow == 0 && move->sourceCol == 0 && move->targetRow == 0 && move->targetCol == 0) {
        // Pass move (0,0,0,0) - send as is
        JsonValue *json_obj = createMoveMessage(my_username, move);
        char *json_str = json_stringify(json_obj);

        send(client_socket, json_str, strlen(json_str), 0);
        send(client_socket, "\n", 1, 0);

        free(json_str);
        json_free(json_obj);
        printf("[Client] move JSON sent for (0,0)->(0,0)\n");
        goto end;
    } else {
        // Convert to 1-based for sending
        Move converted = *move;
        converted.sourceRow += 1;
        converted.sourceCol += 1;
        converted.targetRow += 1;
        converted.targetCol += 1;
        JsonValue *json_obj = createMoveMessage(my_username, &converted);
        char *json_str = json_stringify(json_obj);

        send(client_socket, json_str, strlen(json_str), 0);
        send(client_socket, "\n", 1, 0);

        free(json_str);
        json_free(json_obj);

        // 0-based → 1-based로 콘솔 로그
        printf("[Client] move JSON sent for (%d,%d)->(%d,%d)\n",
               move->sourceRow, move->sourceCol,
               move->targetRow, move->targetCol);
        goto end;
    }
end:
    return;
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
               best_move.sourceRow+1, best_move.sourceCol+1, 
               best_move.targetRow+1, best_move.targetCol+1);
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
            const char *reason = (reason_json && json_is_string(reason_json))
                                 ? json_string_value(reason_json)
                                 : "unknown";
            printf("[Client] register_nack received. Reason: %s\n", reason);
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
                
                // LED 매트릭스에 초기 보드 표시 (과제 요구사항)
                if (led_enabled) {
                    printf("[LED] 초기 게임 보드를 64x64 LED 패널에 표시합니다.\n");
                    drawBoardOnLED(&game_board);
                }
                
                client_state = CLIENT_WAITING;
            }
            break;
        }
        
        case MSG_INVALID_MOVE: {
            printf("[Client] Received invalid_move. Retrying...\n");
 
            Move retry_move = generate_smart_move();
            printf("[Client] Retrying Move: (%d,%d)->(%d,%d)\n",
                   retry_move.sourceRow + 1, retry_move.sourceCol + 1,
                   retry_move.targetRow + 1, retry_move.targetCol + 1);
            send_move_message(&retry_move);
            break;
        }
        
        case MSG_YOUR_TURN: {
            double timeout;

            if (parseYourTurnMessage(json_obj, &game_board, &timeout)) {
                printf("[Client] Your turn. Timeout: %.1f sec\n", timeout);
                printf("[Client] Current board:\n");
                printBoard(&game_board);

                // LED 매트릭스 업데이트 (과제 요구사항: 내 차례 시)
                if (led_enabled) {
                    printf("[LED] 내 차례 - 보드 상태를 LED 패널에 업데이트합니다.\n");
                    drawBoardOnLED(&game_board);
                }

                client_state = CLIENT_YOUR_TURN;

                // --- 1-base 좌표로 로그 찍으면서 Move 생성 ---
                Move move = generate_smart_move();
                int sR = move.sourceRow + 1;
                int sC = move.sourceCol + 1;
                int tR = move.targetRow + 1;
                int tC = move.targetCol + 1;

                if (sR == 1 && sC == 1 && tR == 1 && tC == 1) {
                    // (0,0,0,0)을 1-based로 치환할 수 없으므로
                    // 실제 패스일 땐 로그만 "0,0,0,0"로 찍고 전송
                    printf("[Client] No valid moves → passing (0,0,0,0)\n");
                } else {
                    printf("[Client] Sending Move: (%d,%d)->(%d,%d)\n", sR, sC, tR, tC);
                }

                printf("[Client] Board after move generation (예상):\n");
                printBoard(&game_board);

                send_move_message(&move);  // 내부에서 "JSON + '\n'" 전송됨
                client_state = CLIENT_WAITING;
            }
            break;
        }
        
        case MSG_MOVE_OK: {
            // parseMoveResultMessage()로 board 정보, next_player 추출
            GameBoard updated_board;
            char nextPlayer[64];
            if (parseMoveResultMessage(json_obj, &updated_board, nextPlayer)) {
                memcpy(&game_board, &updated_board, sizeof(GameBoard)); // 로컬 보드 동기화
                printf("[Client] Received move_ok.\n");
                
                if (strcmp(nextPlayer, my_username) == 0) {
                    printf("[Client] It's your turn next.\n");
                } else {
                    printf("[Client] Waiting for opponent (%s).\n", nextPlayer);
                }
            }
            break;
        }
        
        case MSG_PASS: {
            char nextPlayer[64];
            // pass 메시지는 보드 정보가 없을 수 있음 → parseMoveResultMessage() 사용
            GameBoard updated_board;
            if (parseMoveResultMessage(json_obj, &updated_board, nextPlayer)) {
                memcpy(&game_board, &updated_board, sizeof(GameBoard));
            }
            printf("[Client] Opponent passed.\n");
            break;
        }
        
        case MSG_GAME_OVER: {
            char players[2][64];
            int scores[2];
            if (parseGameOverMessage(json_obj, players, scores)) {
                printf("[Client] Game over. Scores: %s=%d, %s=%d\n",
                       players[0], scores[0], players[1], scores[1]);
                if (strcmp(players[0], my_username) == 0 && scores[0] > scores[1]) {
                    printf("[Client] You (%s) won!\n", players[0]);
                } else if (strcmp(players[1], my_username) == 0 && scores[1] > scores[0]) {
                    printf("[Client] You (%s) won!\n", players[1]);
                } else if (scores[0] == scores[1]) {
                    printf("[Client] Draw!\n");
                } else {
                    printf("[Client] You lost.\n");
                }
                
                // 게임 종료 시 최종 보드 상태 LED에 표시 (과제 요구사항)
                if (led_enabled) {
                    printf("[LED] 게임 종료 - 최종 보드 상태를 LED 패널에 표시합니다.\n");
                    drawBoardOnLED(&game_board);
                    
                    // 3초간 최종 결과 표시
                    printf("[LED] 최종 결과를 3초간 표시합니다...\n");
                    sleep(3);
                    
                    printf("[LED] LED 패널을 정리합니다.\n");
                }
                
                client_state = CLIENT_GAME_OVER;
                cleanup_and_exit(0);
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
    } else if (strncmp(argv[i], "--led-", 6) == 0) {
        // hzeller 라이브러리용 옵션: 무시하고 그대로 전달
        continue;
    } else {
        printf("사용법: %s -ip <IP주소> -port <포트> -username <사용자명> [-led] [--led-* 옵션들]\n", argv[0]);
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
        size_t len = strlen(my_username);
        if (len > 0 && my_username[len - 1] == '\n') {
            my_username[len - 1] = '\0';
        }
    }

    // SIGINT 핸들러 등록
    signal(SIGINT, sigint_handler);

    // LED 매트릭스 초기화 (과제 요구사항: 64x64 LED 패널)
    if (led_enabled) {
        printf("[LED] === 64x64 LED Matrix 초기화 (과제 요구사항) ===\n");
        if (ledMatrixInit() != 0) {
            fprintf(stderr, "LED 매트릭스 초기화 실패\n");
            led_enabled = 1;
        } else {
            printf("[LED] LED Matrix 초기화 완료!\n");
            printf("[LED] 과제 규격: 64x64 픽셀 패널, 8x8 게임 그리드\n");
            printf("[LED] 색상 매핑: R=빨강(255,0,0), B=파랑(0,0,255)\n");
            printf("[LED]           .=회색(17,17,17), #=노랑(255,255,0)\n");
            printf("[LED] 그리드: 1픽셀 회색 라인, 6x6 픽셀 게임 말\n");
            printf("[LED] ============================================\n");
        }
    }

    // 클라이언트 소켓 생성
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        cleanup_and_exit(1);
    }

    // 서버 주소 설정
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
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

    // 메인 루프 (poll 사용)
    char buffer[BUFFER_SIZE];
    struct pollfd fds[1];
    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    while (client_state != CLIENT_GAME_OVER) {
        int poll_result = poll(fds, 1, 100);
        if (poll_result > 0 && (fds[0].revents & POLLIN)) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';

                // '\n' 단위로 분리하여 각 JSON 메시지를 처리
                char *line = strtok(buffer, "\n");
                while (line) {
                    handle_server_message(line);
                    line = strtok(NULL, "\n");
                }
            } else if (bytes_received == 0) {
                printf("[Client] 서버 연결이 종료되었습니다.\n");
                break;
            } else {
                perror("[Client] 메시지 수신 오류");
                break;
            }
        } else if (poll_result < 0 && errno != EINTR) {
            perror("[Client] poll 오류");
            break;
        }
        usleep(100000); // 100ms 대기
    }

    printf("[Client] 게임이 종료되었습니다.\n");
    cleanup_and_exit(0);
    return 0;
}

