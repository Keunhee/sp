#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include "octaflip.h"
#include "json.h"
#include "message_handler.h"

#define PORT "8888"
#define SERVER_IP "0.0.0.0"  
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 3.0

pthread_mutex_t player_lock;
pthread_mutex_t move_lock;
pthread_cond_t move_cv;
int player_count = 0;
int turn = 1;
char* userlist[MAX_CLIENTS] = {NULL};
int fdlist[MAX_CLIENTS] = {0};
int score[MAX_CLIENTS] = {0};

Move latest_move;
char* latest_name = NULL;
int move_ready = 0;

void io_register(const int clientfd, const char* name);
void io_move(const int clientfd, const int validity, GameBoard* board, char* next_player);
void io_start(const int clientfd);
void io_turn(const int clientfd, const GameBoard* board);
void io_over(const int clientfd, int* score);
void game_start(GameBoard* board);
void game_turn(GameBoard* board);
void game_over();

void send_free(const int clientfd, JsonValue* message){
    if (message == NULL) return;  // message가 NULL인 경우 처리하지 않음

    char* final_message = json_stringify(message);
    if (final_message == NULL) {  // JSON 변환 실패 체크
        json_free(message);
        return;
    }

    send(clientfd, final_message, strlen(final_message), 0);
    send(clientfd, "\n", 1, 0);
    json_free(message);
    free(final_message);
}

void* io_thread(void* arg) {
    int sockfd = *((int*)arg);
    while (1) {
        int clientfd = accept(sockfd, NULL, NULL);
        if (clientfd == -1) continue; // accept error

        char buf[1024] = {0}; size_t len = 0; char *p;
        len += recv(clientfd, buf+len, sizeof(buf) - len - 1, 0);
        buf[len] = '\0';
        while ((p = strchr(buf, '\n'))){
            *p = '\0';
        }

        JsonValue* json_parsed = json_parse(buf);
        if (json_parsed == NULL) {
            fprintf(stderr, "JSON parse error\n");
            close(clientfd);
            continue;
        }

        const char* type = json_string_value(json_object_get(json_parsed, "type"));
        const char* name = json_string_value(json_object_get(json_parsed, "username"));

        if (!strcmp(type, "register")) {
            io_register(clientfd, name);
        } else if (!strcmp(type, "move")) {
            pthread_mutex_lock(&move_lock);
            latest_move.sourceRow = json_boolean_value(json_object_get(json_parsed, "sx"));
            latest_move.sourceCol = json_boolean_value(json_object_get(json_parsed, "sy"));
            latest_move.targetRow = json_boolean_value(json_object_get(json_parsed, "tx"));
            latest_move.targetCol = json_boolean_value(json_object_get(json_parsed, "ty"));
            if (latest_name != NULL) {
                free(latest_name);
            }
            latest_name = strdup(name);
            move_ready = 1;

            pthread_cond_signal(&move_cv);
            pthread_mutex_unlock(&move_lock);
        }
        json_free(json_parsed);
    }
    return NULL;
}

void io_register(const int clientfd, const char* name){
    pthread_mutex_lock(&player_lock);
    JsonValue* message;

    // check whether if the name is already registered
    int already_registered = 0;
    for(int i = 0; i < player_count; i++) {
        if(userlist[i] != NULL && strcmp(userlist[i], name) == 0) {
            already_registered = 1;
            break;
        }
    }
    if(already_registered) {
        message = createRegisterNackMessage("You already registered"); 
        close(clientfd); 
    }
    else if (player_count < 2) {
        userlist[player_count] = strdup(name);
        if (userlist[player_count] == NULL) {
            message = createRegisterNackMessage("Memory allocation failed");
            close(clientfd);
        } else {
            fdlist[player_count] = clientfd;
            player_count++;
            message = createRegisterAckMessage();
        }
    } else {
        message = createRegisterNackMessage("game is already running.");
        close(clientfd);
    }
    send_free(clientfd, message);
    pthread_mutex_unlock(&player_lock);
}

void io_move(const int clientfd, const int validity, GameBoard* board, char* next_player){
    pthread_mutex_lock(&move_lock);

    JsonValue* message = NULL;
    if(validity == 0){
        message = createMoveOkMessage(board, next_player);
    }
    else if(validity == 1){
        message = createInvalidMoveMessage(board, next_player);
    }
    else if(validity == 2){
        message = createPassMessage(next_player);
    }

    if (message != NULL) {
        send_free(clientfd, message);
    }

    pthread_cond_signal(&move_cv);
    pthread_mutex_unlock(&move_lock);
}

void io_start(const int clientfd){
    pthread_mutex_lock(&player_lock);
    JsonValue* message = createGameStartMessage((const char**)userlist, userlist[0]);
    send_free(clientfd, message);
    pthread_mutex_unlock(&player_lock);
}

void io_turn(const int clientfd, const GameBoard* board){
    pthread_mutex_lock(&player_lock);
    JsonValue* message = createYourTurnMessage(board, TIMEOUT_SEC);
    send_free(clientfd, message);
    pthread_mutex_unlock(&player_lock);
}

void io_over(const int clientfd, int* score){
    pthread_mutex_lock(&player_lock);
    JsonValue* message = createGameOverMessage((const char**)userlist, score);
    send_free(clientfd, message);
    pthread_mutex_unlock(&player_lock);
}

void* game_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&player_lock);
        if (player_count == 2) {
            pthread_mutex_unlock(&player_lock);
            break;
        }
        pthread_mutex_unlock(&player_lock);
    }
    GameBoard* board = (GameBoard*)malloc(sizeof(GameBoard));
    game_start(board);
    
    int current = 0;
    int game_running = 1;
    
    while (game_running) {
        pthread_mutex_lock(&player_lock);
        if (fdlist[0] < 0 || fdlist[1] < 0) {
            pthread_mutex_unlock(&player_lock);
            game_running = 0;
            break;
        }
        pthread_mutex_unlock(&player_lock);

        game_turn(board);
        countPieces(board);
        if (hasGameEnded(board)) {
            game_running = 0;
            break;
        }
        current = 1 - current;
    }
    game_over();
    free(board);
    
    return NULL;
}

void game_start(GameBoard* board){
    initializeBoard(board);
    
    io_start(fdlist[0]);
    io_start(fdlist[1]);
}

void game_turn(GameBoard* board){
    pthread_mutex_lock(&player_lock);
    int current = (turn - 1) % 2;
    pthread_mutex_unlock(&player_lock);

    io_turn(fdlist[current], board);
    
    pthread_mutex_lock(&move_lock);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (int)TIMEOUT_SEC;
    
    while(move_ready == 0){
        int rc = pthread_cond_timedwait(&move_cv, &move_lock, &ts);
        if (rc != 0){
            break;
        }
    }
    
    if (move_ready) { 
        move_ready = 0; 
        if (!strcmp(latest_name, userlist[current])){
            if (isValidMove(board, &latest_move)) {
                applyMove(board, &latest_move);
                io_move(fdlist[current], 0, board, userlist[current]); // valid move
            }
            else {
                io_move(fdlist[current], 1, board, userlist[current]); // invalid move
            }
        }
    } else { 
        io_move(fdlist[current], 2, board, userlist[current]); // pass
    }

    pthread_mutex_unlock(&move_lock);
    pthread_mutex_lock(&player_lock);
    turn++;
    pthread_mutex_unlock(&player_lock);
}

void game_over() {
    io_over(fdlist[0], score);
    io_over(fdlist[1], score);
    close(fdlist[0]);
    close(fdlist[1]);
    fdlist[0] = -1;
    fdlist[1] = -1;
    score[0] = 0;
    score[1] = 0;
    if (userlist[0] != NULL) {
        free(userlist[0]);
        userlist[0] = NULL;
    }
    if (userlist[1] != NULL) {
        free(userlist[1]);
        userlist[1] = NULL;
    }
    player_count = 0;
    if (latest_name != NULL) {
        free(latest_name);
        latest_name = NULL;
    }
}

int main(void) {
    struct addrinfo hints, *res;
    int sockfd, status;

    // 뮤텍스와 조건 변수 초기화
    pthread_mutex_init(&player_lock, NULL);
    pthread_mutex_init(&move_lock, NULL);
    pthread_cond_init(&move_cv, NULL);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    status = getaddrinfo(SERVER_IP, PORT, &hints, &res);
    
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket error");
        freeaddrinfo(res);
        exit(1);
    }

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        perror("bind error");
        freeaddrinfo(res);
        close(sockfd);
        exit(1);
    }

    status = listen(sockfd, 1);
    if (status == -1) {
        perror("listen error");
        freeaddrinfo(res);
        close(sockfd);
        exit(1);
    }

    pthread_t io_tid;
    pthread_create(&io_tid, NULL, io_thread, &sockfd);

    pthread_t game_tid;
    pthread_create(&game_tid, NULL, game_thread, NULL);

    pthread_join(io_tid, NULL);
    pthread_join(game_tid, NULL);
    
    pthread_mutex_destroy(&player_lock);
    pthread_mutex_destroy(&move_lock);
    pthread_cond_destroy(&move_cv);

    freeaddrinfo(res);
    close(sockfd);
    return 0;
}