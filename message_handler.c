#include "message_handler.h"

/**
 * 메시지 처리 모듈
 * 
 * 이 모듈은 OctaFlip 게임의 클라이언트-서버 통신에 사용되는 메시지를 생성하고
 * 파싱하는 기능을 제공합니다. 모든 메시지는 JSON 형식으로 처리됩니다.
 * 
 * 메시지 유형:
 * - 등록 메시지: 클라이언트가 서버에 자신을 등록
 * - 이동 메시지: 클라이언트가 서버에 이동 요청
 * - 게임 시작 메시지: 서버가 클라이언트에게 게임 시작 알림
 * - 턴 메시지: 서버가 클라이언트에게 현재 차례 알림
 * - 이동 결과 메시지: 서버가 클라이언트에게 이동 결과 알림
 * - 게임 종료 메시지: 서버가 클라이언트에게 게임 종료 알림
 */

/**
 * 클라이언트 -> 서버: 등록 메시지 생성
 * 
 * 클라이언트가 서버에 자신의 사용자 이름을 등록하기 위한 메시지를 생성합니다.
 * 
 * @param username 등록할 사용자 이름
 * @return JSON 형식의 등록 메시지
 */
JsonValue* createRegisterMessage(const char *username) {
    JsonValue *root = json_object();
    json_object_set(root, "type", json_string("register"));
    json_object_set(root, "username", json_string(username));
    return root;
}

// 클라이언트 -> 서버: 이동 메시지 생성
JsonValue* createMoveMessage(const char *username, Move *move) {
    JsonValue *root = json_object();
    json_object_set(root, "type", json_string("move"));
    json_object_set(root, "username", json_string(username));
    json_object_set(root, "sx", json_number(move->sourceRow));
    json_object_set(root, "sy", json_number(move->sourceCol));
    json_object_set(root, "tx", json_number(move->targetRow));
    json_object_set(root, "ty", json_number(move->targetCol));
    return root;
}

// 서버 -> 클라이언트: 등록 성공 메시지 생성
JsonValue* createRegisterAckMessage() {
    JsonValue *root = json_object();
    json_object_set(root, "type", json_string("register_ack"));
    return root;
}

// 서버 -> 클라이언트: 등록 실패 메시지 생성
JsonValue* createRegisterNackMessage(const char *reason) {
    JsonValue *root = json_object();
    json_object_set(root, "type", json_string("register_nack"));
    json_object_set(root, "reason", json_string(reason));
    return root;
}

// 서버 -> 클라이언트: 게임 시작 메시지 생성
JsonValue* createGameStartMessage(const char *players[2], const char *firstPlayer) {
    JsonValue *root = json_object();
    JsonValue *playersArray = json_array();
    
    json_object_set(root, "type", json_string("game_start"));
    
    json_array_append(playersArray, json_string(players[0]));
    json_array_append(playersArray, json_string(players[1]));
    
    json_object_set(root, "players", playersArray);
    json_object_set(root, "first_player", json_string(firstPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 당신 차례 메시지 생성
JsonValue* createYourTurnMessage(const GameBoard *board, double timeout) {
    JsonValue *root = json_object();
    JsonValue *boardArray = json_array();
    
    json_object_set(root, "type", json_string("your_turn"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set(root, "board", boardArray);
    json_object_set(root, "timeout", json_number(timeout));
    
    return root;
}

// 서버 -> 클라이언트: 이동 성공 메시지 생성
JsonValue* createMoveOkMessage(const GameBoard *board, const char *nextPlayer) {
    JsonValue *root = json_object();
    JsonValue *boardArray = json_array();
    
    json_object_set(root, "type", json_string("move_ok"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set(root, "board", boardArray);
    json_object_set(root, "next_player", json_string(nextPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 유효하지 않은 이동 메시지 생성
JsonValue* createInvalidMoveMessage(const GameBoard *board, const char *nextPlayer) {
    JsonValue *root = json_object();
    JsonValue *boardArray = json_array();
    
    json_object_set(root, "type", json_string("invalid_move"));
    
    // 보드 상태 변환
    for (int i = 0; i < BOARD_SIZE; i++) {
        json_array_append(boardArray, json_string(board->cells[i]));
    }
    
    json_object_set(root, "board", boardArray);
    json_object_set(root, "next_player", json_string(nextPlayer));
    
    return root;
}

// 서버 -> 클라이언트: 패스 메시지 생성
JsonValue* createPassMessage(const char *nextPlayer) {
    JsonValue *root = json_object();
    json_object_set(root, "type", json_string("pass"));
    json_object_set(root, "next_player", json_string(nextPlayer));
    return root;
}

// 서버 -> 클라이언트: 게임 종료 메시지 생성
JsonValue* createGameOverMessage(const char *players[2], int scores[2]) {
    JsonValue *root = json_object();
    JsonValue *scoresObj = json_object();
    
    json_object_set(root, "type", json_string("game_over"));
    
    // 점수 설정
    json_object_set(scoresObj, players[0], json_number(scores[0]));
    json_object_set(scoresObj, players[1], json_number(scores[1]));
    
    json_object_set(root, "scores", scoresObj);
    
    return root;
}

// 메시지 유형 파싱
MessageType parseMessageType(JsonValue *jsonValue) {
    if (!json_is_object(jsonValue)) {
        return MSG_UNKNOWN;
    }
    
    JsonValue *typeValue = json_object_get(jsonValue, "type");
    if (!json_is_string(typeValue)) {
        return MSG_UNKNOWN;
    }
    
    const char *type = json_string_value(typeValue);
    if (!type) {
        return MSG_UNKNOWN;
    }
    
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
    
    return MSG_UNKNOWN;
}

// 등록 메시지 파싱
char* parseRegisterMessage(JsonValue *jsonValue) {
    JsonValue *usernameValue = json_object_get(jsonValue, "username");
    if (!json_is_string(usernameValue)) {
        return NULL;
    }
    
    const char *username = json_string_value(usernameValue);
    if (!username) {
        return NULL;
    }
    
    return strdup(username);
}

// 이동 메시지 파싱
int parseMoveMessage(JsonValue *jsonValue, char **username, Move *move) {
    JsonValue *usernameValue = json_object_get(jsonValue, "username");
    JsonValue *sxValue = json_object_get(jsonValue, "sx");
    JsonValue *syValue = json_object_get(jsonValue, "sy");
    JsonValue *txValue = json_object_get(jsonValue, "tx");
    JsonValue *tyValue = json_object_get(jsonValue, "ty");
    
    if (!json_is_string(usernameValue) || !json_is_number(sxValue) || 
        !json_is_number(syValue) || !json_is_number(txValue) || !json_is_number(tyValue)) {
        return 0;
    }
    
    const char *user = json_string_value(usernameValue);
    if (!user) {
        return 0;
    }
    
    *username = strdup(user);
    move->sourceRow = (int)json_number_value(sxValue);
    move->sourceCol = (int)json_number_value(syValue);
    move->targetRow = (int)json_number_value(txValue);
    move->targetCol = (int)json_number_value(tyValue);
    
    return 1;
}

// 게임 시작 메시지 파싱
int parseGameStartMessage(JsonValue *jsonValue, char players[2][64], char *firstPlayer) {
    JsonValue *playersValue = json_object_get(jsonValue, "players");
    JsonValue *firstPlayerValue = json_object_get(jsonValue, "first_player");
    
    if (!json_is_array(playersValue) || !json_is_string(firstPlayerValue) || 
        json_array_size(playersValue) != 2) {
        return 0;
    }
    
    for (int i = 0; i < 2; i++) {
        JsonValue *playerValue = json_array_get(playersValue, i);
        if (!json_is_string(playerValue)) {
            return 0;
        }
        
        const char *player = json_string_value(playerValue);
        if (!player) {
            return 0;
        }
        
        strncpy(players[i], player, 63);
        players[i][63] = '\0';
    }
    
    const char *first = json_string_value(firstPlayerValue);
    if (!first) {
        return 0;
    }
    
    strncpy(firstPlayer, first, 63);
    firstPlayer[63] = '\0';
    
    return 1;
}

// 당신 차례 메시지 파싱
int parseYourTurnMessage(JsonValue *jsonValue, GameBoard *board, double *timeout) {
    JsonValue *boardValue = json_object_get(jsonValue, "board");
    JsonValue *timeoutValue = json_object_get(jsonValue, "timeout");
    
    if (!json_is_array(boardValue) || json_array_size(boardValue) != BOARD_SIZE || 
        !json_is_number(timeoutValue)) {
        return 0;
    }
    
    *timeout = json_number_value(timeoutValue);
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        JsonValue *rowValue = json_array_get(boardValue, i);
        if (!json_is_string(rowValue)) {
            return 0;
        }
        
        const char *row = json_string_value(rowValue);
        if (!row) {
            return 0;
        }
        
        strncpy(board->cells[i], row, BOARD_SIZE);
        board->cells[i][BOARD_SIZE] = '\0';
    }
    
    countPieces(board);
    return 1;
}

// 이동 결과 메시지 파싱 (move_ok, invalid_move, pass)
int parseMoveResultMessage(JsonValue *jsonValue, GameBoard *board, char *nextPlayer) {
    JsonValue *nextPlayerValue = json_object_get(jsonValue, "next_player");
    JsonValue *boardValue = json_object_get(jsonValue, "board");
    
    if (!json_is_string(nextPlayerValue)) {
        return 0;
    }
    
    const char *next = json_string_value(nextPlayerValue);
    if (!next) {
        return 0;
    }
    
    strncpy(nextPlayer, next, 63);
    nextPlayer[63] = '\0';
    
    // 보드가 있는 경우에만 파싱 (pass 메시지에는 보드가 없을 수 있음)
    if (json_is_array(boardValue) && json_array_size(boardValue) == BOARD_SIZE) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            JsonValue *rowValue = json_array_get(boardValue, i);
            if (!json_is_string(rowValue)) {
                return 0;
            }
            
            const char *row = json_string_value(rowValue);
            if (!row) {
                return 0;
            }
            
            strncpy(board->cells[i], row, BOARD_SIZE);
            board->cells[i][BOARD_SIZE] = '\0';
        }
        
        countPieces(board);
    }
    
    return 1;
}

// 게임 종료 메시지 파싱
int parseGameOverMessage(JsonValue *jsonValue, char players[2][64], int scores[2]) {
    JsonValue *scoresValue = json_object_get(jsonValue, "scores");
    
    if (!json_is_object(scoresValue)) {
        return 0;
    }
    
    // 첫 번째 플레이어/점수 쌍 찾기
    JsonObjectItem *item = scoresValue->value.object;
    int playerIndex = 0;
    
    while (item && playerIndex < 2) {
        if (!json_is_number(item->value)) {
            return 0;
        }
        
        strncpy(players[playerIndex], item->key, 63);
        players[playerIndex][63] = '\0';
        scores[playerIndex] = (int)json_number_value(item->value);
        
        playerIndex++;
        item = item->next;
    }
    
    return playerIndex == 2;
}