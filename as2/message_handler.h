#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "octaflip.h"
#include "json.h"

// 메시지 유형 정의
typedef enum {
    // 클라이언트에서 서버로
    MSG_REGISTER,
    MSG_MOVE,
    
    // 서버에서 클라이언트로
    MSG_REGISTER_ACK,
    MSG_REGISTER_NACK,
    MSG_GAME_START,
    MSG_YOUR_TURN,
    MSG_MOVE_OK,
    MSG_INVALID_MOVE,
    MSG_PASS,
    MSG_GAME_OVER,
    
    // 오류
    MSG_UNKNOWN
} MessageType;
JsonValue* createOpponentLeftMessage(const char *leftUsername);
// 클라이언트 -> 서버: 등록 메시지 생성
JsonValue* createRegisterMessage(const char *username);

// 클라이언트 -> 서버: 이동 메시지 생성
JsonValue* createMoveMessage(const char *username, Move *move);

// 서버 -> 클라이언트: 등록 성공 메시지 생성
JsonValue* createRegisterAckMessage();

// 서버 -> 클라이언트: 등록 실패 메시지 생성
JsonValue* createRegisterNackMessage(const char *reason);

// 서버 -> 클라이언트: 게임 시작 메시지 생성
JsonValue* createGameStartMessage(const char *players[2], const char *firstPlayer);

// 서버 -> 클라이언트: 당신 차례 메시지 생성
JsonValue* createYourTurnMessage(const GameBoard *board, double timeout);

// 서버 -> 클라이언트: 이동 성공 메시지 생성
JsonValue* createMoveOkMessage(const GameBoard *board, const char *nextPlayer);

// 서버 -> 클라이언트: 유효하지 않은 이동 메시지 생성
JsonValue* createInvalidMoveMessage(const GameBoard *board, const char *nextPlayer);

// 서버 -> 클라이언트: 패스 메시지 생성
JsonValue* createPassMessage(const char *nextPlayer);

// 서버 -> 클라이언트: 게임 종료 메시지 생성
JsonValue* createGameOverMessage(const char *players[2], int scores[2]);

// 메시지 유형 파싱
MessageType parseMessageType(JsonValue *jsonValue);

// 등록 메시지 파싱
char* parseRegisterMessage(JsonValue *jsonValue);

// 이동 메시지 파싱
int parseMoveMessage(JsonValue *jsonValue, char **username, Move *move);

// 게임 시작 메시지 파싱
int parseGameStartMessage(JsonValue *jsonValue, char players[2][64], char *firstPlayer);

// 당신 차례 메시지 파싱
int parseYourTurnMessage(JsonValue *jsonValue, GameBoard *board, double *timeout);

// 이동 결과 메시지 파싱 (move_ok, invalid_move, pass)
int parseMoveResultMessage(JsonValue *jsonValue, GameBoard *board, char *nextPlayer);

// 게임 종료 메시지 파싱
int parseGameOverMessage(JsonValue *jsonValue, char players[2][64], int scores[2]);

#endif /* MESSAGE_HANDLER_H */
