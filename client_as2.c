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
#include "ai_engine.h"
#include "board.h"        // 보드 모듈 추가

#define BUFFER_SIZE 1024

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
char my_username[64];
char my_color;
char opponent_username[64];

// 함수 선언
void handle_server_message(char *buffer);
void send_register_message();
void send_move_message(Move *move);
Move generate_smart_move();
void cleanup_and_exit(int status);
void sigint_handler(int sig);
int receive_message(char *buffer, int buffer_size);

void cleanup_and_exit(int status) {
    if (client_socket != -1)
        close(client_socket);
    exit(status);
}

void sigint_handler(int sig __attribute__((unused))) {
    printf("\n프로그램을 종료합니다.\n");
    cleanup_and_exit(0);
}

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

void send_move_message(Move *move) {
    if (move->sourceRow == 0 && move->sourceCol == 0 && move->targetRow == 0 && move->targetCol == 0) {
        JsonValue *json_obj = createMoveMessage(my_username, move);
        char *json_str = json_stringify(json_obj);
        send(client_socket, json_str, strlen(json_str), 0);
        send(client_socket, "\n", 1, 0);
        free(json_str);
        json_free(json_obj);
        printf("[Client] move JSON sent for (0,0)->(0,0)\n");
        return;
    }

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
    printf("[Client] move JSON sent for (%d,%d)->(%d,%d)\n",
           move->sourceRow, move->sourceCol, move->targetRow, move->targetCol);
}

Move generate_smart_move() {
    printf("\n=== AI 엔진 시작 ===\n");
    printf("현재 플레이어: %c\n", my_color);

    // board.h에서 제공하는 함수로 보드 상태 가져오기
    GameBoard *board_ptr = getBoard();
    printf("보드 상태: R=%d, B=%d, Empty=%d\n",
           board_ptr->redCount, board_ptr->blueCount, board_ptr->emptyCount);

    Move best_move = generateWinningMove(board_ptr, my_color);
    if (best_move.sourceRow == 0 && best_move.sourceCol == 0 && best_move.targetRow == 0 && best_move.targetCol == 0) {
        printf("AI 판단: 유효한 이동이 없어 패스합니다.\n");
    } else {
        printf("AI 선택: (%d,%d) -> (%d,%d)\n",
               best_move.sourceRow + 1, best_move.sourceCol + 1,
               best_move.targetRow + 1, best_move.targetCol + 1);
    }
    printf("=== AI 엔진 종료 ===\n\n");
    return best_move;
}

int receive_message(char *buffer, int buffer_size) {
    int bytes_received = recv(client_socket, buffer, buffer_size - 1, MSG_DONTWAIT);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        return bytes_received;
    }
    return 0;
}

void handle_server_message(char *buffer) {
    JsonValue *json_obj = json_parse(buffer);
    if (!json_obj) {
        fprintf(stderr, "유효하지 않은 JSON 메시지: %s\n", buffer);
        return;
    }

    MessageType msg_type = parseMessageType(json_obj);

    switch (msg_type) {
        case MSG_REGISTER_ACK:
            printf("등록 성공! 다른 플레이어를 기다립니다...\n");
            client_state = CLIENT_WAITING;
            break;

        case MSG_REGISTER_NACK: {
            const char *reason = json_get_string(json_obj, "reason", "unknown");
            printf("[Client] register_nack received. Reason: %s\n", reason);
            cleanup_and_exit(1);
            break;
        }

        case MSG_GAME_START: {
            char players[2][64], first_player[64];
            if (parseGameStartMessage(json_obj, players, first_player)) {
                printf("게임 시작! 플레이어: %s vs %s\n", players[0], players[1]);
                printf("첫 번째 플레이어: %s\n", first_player);

                if (strcmp(players[0], my_username) == 0) {
                    strncpy(opponent_username, players[1], 63);
                    my_color = RED_PLAYER;
                } else {
                    strncpy(opponent_username, players[0], 63);
                    my_color = BLUE_PLAYER;
                }

                printf("내 색상: %c\n", my_color);

                // 보드 초기화
                initBoard();
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
            GameBoard updated_board;
            if (parseYourTurnMessage(json_obj, &updated_board, &timeout)) {
                // 보드 상태 갱신
                updateBoard(&updated_board);

                printf("[Client] Your turn. Timeout: %.1f sec\n", timeout);

                // 화면에 보드 출력
                displayBoard();

                client_state = CLIENT_YOUR_TURN;
                Move move = generate_smart_move();
                send_move_message(&move);
                client_state = CLIENT_WAITING;
            }
            break;
        }

        case MSG_MOVE_OK: {
            GameBoard updated_board;
            char nextPlayer[64];
            if (parseMoveResultMessage(json_obj, &updated_board, nextPlayer)) {
                // 보드 상태 갱신
                updateBoard(&updated_board);

                printf("[Client] Received move_ok. Board updated:\n");
                displayBoard();
            }
            break;
        }

        case MSG_PASS: {
            char nextPlayer[64];
            GameBoard updated_board;
            if (parseMoveResultMessage(json_obj, &updated_board, nextPlayer)) {
                // 보드 상태 갱신
                updateBoard(&updated_board);
            }
            printf("[Client] Opponent passed. Board now:\n");
            displayBoard();
            break;
        }

        case MSG_GAME_OVER: {
            char players[2][64];
            int scores[2];
            if (parseGameOverMessage(json_obj, players, scores)) {
                printf("[Client] Game over. Scores: %s=%d, %s=%d\n",
                       players[0], scores[0], players[1], scores[1]);
                if ((strcmp(players[0], my_username) == 0 && scores[0] > scores[1]) ||
                    (strcmp(players[1], my_username) == 0 && scores[1] > scores[0])) {
                    printf("[Client] You won!\n");
                } else if (scores[0] == scores[1]) {
                    printf("[Client] Draw!\n");
                } else {
                    printf("[Client] You lost.\n");
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

int main(int argc, char *argv[]) {
    char ip_address[32] = "127.0.0.1";
    int port = 8888;

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
        }
    }

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

    signal(SIGINT, sigint_handler);

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        cleanup_and_exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        perror("유효하지 않은 주소/주소가 지원되지 않음");
        cleanup_and_exit(1);
    }

    if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("연결 실패");
        cleanup_and_exit(1);
    }
    printf("서버에 연결되었습니다: %s:%d\n", ip_address, port);

    send_register_message();

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
        usleep(100000);
    }

    printf("[Client] 게임이 종료되었습니다.\n");
    cleanup_and_exit(0);
    return 0;
}
