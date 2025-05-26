#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include "octaflip.h"
#include <jansson.h>

// JSON 메시지 유형 정의
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
    MSG_GAME_OVER
} MessageType;

// 클라이언트 -> 서버: 등록 메시지 생성
json_t* createRegisterMessage(const char *username);

// 클라이언트 -> 서버: 이동 메시지 생성
json_t* createMoveMessage(const char *username, Move *move);

// 서버 -> 클라이언트: 등록 성공 메시지 생성
json_t* createRegisterAckMessage();

// 서버 -> 클라이언트: 등록 실패 메시지 생성
json_t* createRegisterNackMessage(const char *reason);

// 서버 -> 클라이언트: 게임 시작 메시지 생성
json_t* createGameStartMessage(const char *players[], const char *firstPlayer);

// 서버 -> 클라이언트: 당신 차례 메시지 생성
json_t* createYourTurnMessage(const GameBoard *board, double timeout);

// 서버 -> 클라이언트: 이동 성공 메시지 생성
json_t* createMoveOkMessage(const GameBoard *board, const char *nextPlayer);

// 서버 -> 클라이언트: 유효하지 않은 이동 메시지 생성
json_t* createInvalidMoveMessage(const GameBoard *board, const char *nextPlayer);

// 서버 -> 클라이언트: 패스 메시지 생성
json_t* createPassMessage(const char *nextPlayer);

// 서버 -> 클라이언트: 게임 종료 메시지 생성
json_t* createGameOverMessage(const char *players[], int scores[]);

// 메시지 유형 파싱
MessageType parseMessageType(json_t *jsonObj);

// 등록 메시지 파싱
char* parseRegisterMessage(json_t *jsonObj);

// 이동 메시지 파싱
int parseMoveMessage(json_t *jsonObj, char **username, Move *move);

// 게임 시작 메시지 파싱
int parseGameStartMessage(json_t *jsonObj, char players[2][64], char *firstPlayer);

// 당신 차례 메시지 파싱
int parseYourTurnMessage(json_t *jsonObj, GameBoard *board, double *timeout);

// 이동 결과 메시지 파싱 (move_ok, invalid_move, pass)
int parseMoveResultMessage(json_t *jsonObj, GameBoard *board, char *nextPlayer);

// 게임 종료 메시지 파싱
int parseGameOverMessage(json_t *jsonObj, char players[2][64], int scores[2]);

// JSON 객체를 문자열로 변환
char* jsonToString(json_t *jsonObj);

// 문자열에서 JSON 객체 생성
json_t* stringToJson(const char *jsonStr);

#endif /* JSON_HANDLER_H */