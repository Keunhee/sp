#include "json_handler.h"
#include <string.h>

// 클라이언트 -> 서버: 등록 메시지 생성
json_t* createRegisterMessage(const char *username) {
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("register"));
    json_object_set_new(root, "username", json_string(username));
    return root;
}

// 클라이언트 -> 서버: 이동 메시지 생성
json_t* createMoveMessage(const char *username, Move *move) {
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("move"));
    json_object_set_new(root, "username", json_string(username));
    json_object_set_new(root, "sx", json_integer(move->sourceRow));
    json_object_set_new(root, "sy", json_integer(move->sourceCol));
    json_object_set_new(root, "tx", json_integer(move->targetRow));
    json_object_set_new(root, "ty", json_integer(move->targetCol));
    return root;
}

// 서버 -> 클라이언트: 등록 성공 메시지 생성
json_t* createRegisterAckMessage() {
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("register_ack"));
    return root;
}

// 서버 -> 클라이언트: 등록 실패 메시지 생성
json_t* createRegisterNackMessage(const char *reason) {
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("register_nack"));
    json_object_set_new(root, "reason", json_string(reason));
    return root;
}

// 서버 -> 클라이언트: 게임 시작 메시지 생성
json_t* createGameStartMessage(const char *players[], const char *firstPlayer) {
    json_t *root = json_object();
    json_t *playersArray = json_array();
    
    json_object_set_new(root, "type", json_string("game_start"));
    json_array_append_new(playersArray, json_string(players[0]));
    json_array_append_new(playersArray, json_string(players[1]));
    json_object_set_new(root, "players", playersArray);
    json_object_set_new(root, "first_player", json_string(firstPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 당신 차례 메시지 생성
json_t* createYourTurnMessage(const GameBoard *board, double timeout) {
    json_t *root = json_object();
    json_t *boardArray = json_array();
    
    json_object_set_new(root, "type", json_string("your_turn"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append_new(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set_new(root, "board", boardArray);
    json_object_set_new(root, "timeout", json_real(timeout));
    
    return root;
}

// 서버 -> 클라이언트: 이동 성공 메시지 생성
json_t* createMoveOkMessage(const GameBoard *board, const char *nextPlayer) {
    json_t *root = json_object();
    json_t *boardArray = json_array();
    
    json_object_set_new(root, "type", json_string("move_ok"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append_new(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set_new(root, "board", boardArray);
    json_object_set_new(root, "next_player", json_string(nextPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 유효하지 않은 이동 메시지 생성
json_t* createInvalidMoveMessage(const GameBoard *board, const char *nextPlayer) {
    json_t *root = json_object();
    json_t *boardArray = json_array();
    
    json_object_set_new(root, "type", json_string("invalid_move"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append_new(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set_new(root, "board", boardArray);
    json_object_set_new(root, "next_player", json_string(nextPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 패스 메시지 생성
json_t* createPassMessage(const char *nextPlayer) {
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("pass"));
    json_object_set_new(root, "next_player", json_string(nextPlayer));
    return root;
}

// 서버 -> 클라이언트: 게임 종료 메시지 생성
json_t* createGameOverMessage(const char *players[], int scores[]) {
    json_t *root = json_object();
    json_t *scoresObj = json_object();
    
    json_object_set_new(root, "type", json_string("game_over"));
    json_object_set_new(scoresObj, players[0], json_integer(scores[0]));
    json_object_set_new(scoresObj, players[1], json_integer(scores[1]));
    json_object_set_new(root, "scores", scoresObj);
    
    return root;
}

// 메시지 유형 파싱
MessageType parseMessageType(json_t *jsonObj) {
    json_t *typeJson = json_object_get(jsonObj, "type");
    if (!typeJson || !json_is_string(typeJson)) {
        return -1;  // 유효하지 않은 메시지
    }
    
    const char *type = json_string_value(typeJson);
    
    if (strcmp(type, "register") == 0) {
        return MSG_REGISTER;
    } else if (strcmp(type, "move") == 0) {
        return MSG_MOVE;
    } else if (strcmp(type, "register_ack") == 0) {
        return MSG_REGISTER_ACK;
    } else if (strcmp(type, "register_nack") == 0) {
        return MSG_REGISTER_NACK;
    } else if (strcmp(type, "game_start") == 0) {
        return MSG_GAME_START;
    } else if (strcmp(type, "your_turn") == 0) {
        return MSG_YOUR_TURN;
    } else if (strcmp(type, "move_ok") == 0) {
        return MSG_MOVE_OK;
    } else if (strcmp(type, "invalid_move") == 0) {
        return MSG_INVALID_MOVE;
    } else if (strcmp(type, "pass") == 0) {
        return MSG_PASS;
    } else if (strcmp(type, "game_over") == 0) {
        return MSG_GAME_OVER;
    }
    
    return -1;  // 알 수 없는 메시지 유형
}

// 등록 메시지 파싱
char* parseRegisterMessage(json_t *jsonObj) {
    json_t *usernameJson = json_object_get(jsonObj, "username");
    if (!usernameJson || !json_is_string(usernameJson)) {
        return NULL;
    }
    
    return strdup(json_string_value(usernameJson));
}

// 이동 메시지 파싱
int parseMoveMessage(json_t *jsonObj, char **username, Move *move) {
    json_t *usernameJson = json_object_get(jsonObj, "username");
    json_t *sxJson = json_object_get(jsonObj, "sx");
    json_t *syJson = json_object_get(jsonObj, "sy");
    json_t *txJson = json_object_get(jsonObj, "tx");
    json_t *tyJson = json_object_get(jsonObj, "ty");
    
    if (!usernameJson || !json_is_string(usernameJson) ||
        !sxJson || !json_is_integer(sxJson) ||
        !syJson || !json_is_integer(syJson) ||
        !txJson || !json_is_integer(txJson) ||
        !tyJson || !json_is_integer(tyJson)) {
        return 0;  // 실패
    }
    
    *username = strdup(json_string_value(usernameJson));
    move->sourceRow = json_integer_value(sxJson);
    move->sourceCol = json_integer_value(syJson);
    move->targetRow = json_integer_value(txJson);
    move->targetCol = json_integer_value(tyJson);
    
    return 1;  // 성공
}

// 게임 시작 메시지 파싱
int parseGameStartMessage(json_t *jsonObj, char players[2][64], char *firstPlayer) {
    json_t *playersJson = json_object_get(jsonObj, "players");
    json_t *firstPlayerJson = json_object_get(jsonObj, "first_player");
    
    if (!playersJson || !json_is_array(playersJson) || json_array_size(playersJson) != 2 ||
        !firstPlayerJson || !json_is_string(firstPlayerJson)) {
        return 0;  // 실패
    }
    
    json_t *player1Json = json_array_get(playersJson, 0);
    json_t *player2Json = json_array_get(playersJson, 1);
    
    if (!player1Json || !json_is_string(player1Json) ||
        !player2Json || !json_is_string(player2Json)) {
        return 0;  // 실패
    }
    
    strncpy(players[0], json_string_value(player1Json), 63);
    players[0][63] = '\0';
    strncpy(players[1], json_string_value(player2Json), 63);
    players[1][63] = '\0';
    strncpy(firstPlayer, json_string_value(firstPlayerJson), 63);
    firstPlayer[63] = '\0';
    
    return 1;  // 성공
}

// 당신 차례 메시지 파싱
int parseYourTurnMessage(json_t *jsonObj, GameBoard *board, double *timeout) {
    json_t *boardJson = json_object_get(jsonObj, "board");
    json_t *timeoutJson = json_object_get(jsonObj, "timeout");
    
    if (!boardJson || !json_is_array(boardJson) || json_array_size(boardJson) != BOARD_SIZE ||
        !timeoutJson || !json_is_real(timeoutJson)) {
        return 0;  // 실패
    }
    
    // 보드 파싱
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_t *rowJson = json_array_get(boardJson, i);
        if (!rowJson || !json_is_string(rowJson)) {
            return 0;  // 실패
        }
        
        const char *rowStr = json_string_value(rowJson);
        strncpy(board->cells[i], rowStr, BOARD_SIZE);
        board->cells[i][BOARD_SIZE] = '\0';
    }
    
    *timeout = json_real_value(timeoutJson);
    countPieces(board);
    
    return 1;  // 성공
}

// 이동 결과 메시지 파싱 (move_ok, invalid_move, pass)
int parseMoveResultMessage(json_t *jsonObj, GameBoard *board, char *nextPlayer) {
    json_t *boardJson = json_object_get(jsonObj, "board");
    json_t *nextPlayerJson = json_object_get(jsonObj, "next_player");
    
    if (!nextPlayerJson || !json_is_string(nextPlayerJson)) {
        return 0;  // 실패
    }
    
    // 보드 파싱 (pass 메시지에는 board가 없을 수 있음)
    if (boardJson && json_is_array(boardJson) && json_array_size(boardJson) == BOARD_SIZE) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            json_t *rowJson = json_array_get(boardJson, i);
            if (!rowJson || !json_is_string(rowJson)) {
                return 0;  // 실패
            }
            
            const char *rowStr = json_string_value(rowJson);
            strncpy(board->cells[i], rowStr, BOARD_SIZE);
            board->cells[i][BOARD_SIZE] = '\0';
        }
        
        countPieces(board);
    }
    
    strncpy(nextPlayer, json_string_value(nextPlayerJson), 63);
    nextPlayer[63] = '\0';
    
    return 1;  // 성공
}

// 게임 종료 메시지 파싱
int parseGameOverMessage(json_t *jsonObj, char players[2][64], int scores[2]) {
    json_t *scoresJson = json_object_get(jsonObj, "scores");
    
    if (!scoresJson || !json_is_object(scoresJson)) {
        return 0;  // 실패
    }
    
    // 첫 번째 플레이어/점수 쌍
    const char *key;
    json_t *value;
    int playerIndex = 0;
    
    json_object_foreach(scoresJson, key, value) {
        if (playerIndex >= 2 || !json_is_integer(value)) {
            return 0;  // 실패
        }
        
        strncpy(players[playerIndex], key, 63);
        players[playerIndex][63] = '\0';
        scores[playerIndex] = json_integer_value(value);
        playerIndex++;
    }
    
    if (playerIndex != 2) {
        return 0;  // 정확히 2명의 플레이어가 필요
    }
    
    return 1;  // 성공
}

// JSON 객체를 문자열로 변환
char* jsonToString(json_t *jsonObj) {
    char *jsonStr = json_dumps(jsonObj, JSON_COMPACT);
    return jsonStr;
}

// 문자열에서 JSON 객체 생성
json_t* stringToJson(const char *jsonStr) {
    json_error_t error;
    json_t *jsonObj = json_loads(jsonStr, 0, &error);
    
    if (!jsonObj) {
        fprintf(stderr, "JSON 파싱 오류: %s\n", error.text);
        return NULL;
    }
    
    return jsonObj;
}