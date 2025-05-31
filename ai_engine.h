#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include "octaflip.h"
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

// AI 설정 상수
#define MAX_DEPTH 8
#define TIME_LIMIT 4.5  // 4.5초 제한 (서버 5초 제한보다 여유)
#define TRANSPOSITION_TABLE_SIZE 1000003  // 소수로 설정

// 무한대 값 정의
#define INFINITY_VAL 1000000
#define NEG_INFINITY_VAL -1000000

// 위치별 가중치 테이블 (코너가 가장 좋고, 가장자리는 약간 좋음)
// 16비트(short) 타입으로 정의하여 NEON ld1 {v1.8h}, [...]와 맞춥니다.
extern short POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Transposition Table 엔트리
typedef struct {
    unsigned long long hash;
    int depth;
    int value;
    Move best_move;
    char flag;  // 'E' = exact, 'L' = lower bound, 'U' = upper bound
} TTEntry;

// AI 엔진 구조체
typedef struct {
    TTEntry *transposition_table;
    int nodes_searched;
    clock_t start_time;
    int time_limit_exceeded;
} AIEngine;

// 함수 선언
AIEngine* createAIEngine();
void destroyAIEngine(AIEngine *engine);
Move findBestMove(AIEngine *engine, const GameBoard *board, char player);
int minimax(AIEngine *engine, GameBoard *board, int depth, int alpha, int beta, 
            char maximizing_player, char original_player);
int evaluateBoard(const GameBoard *board, char player);
unsigned long long calculateHash(const GameBoard *board);
void storeInTT(AIEngine *engine, unsigned long long hash, int depth, int value, 
               Move move, char flag);
TTEntry* lookupTT(AIEngine *engine, unsigned long long hash);
int getMobility(const GameBoard *board, char player);
int getStability(const GameBoard *board, char player);
bool isCorner(int row, int col);
int isEdge(int row, int col);
void getAllValidMoves(const GameBoard *board, char player, Move *moves, int *count);
int isTimeUp(AIEngine *engine);
Move generateWinningMove(const GameBoard *board, char player);

#endif /* AI_ENGINE_H */
