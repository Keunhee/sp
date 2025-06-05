#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include "board.h"
#include <time.h>
#include <limits.h>
#include <stdbool.h>
// AI 설정 상수
#define MAX_DEPTH 8
#define TIME_LIMIT 2.5  // 4.5초 제한 (서버 5초 제한보다 여유)
#define TRANSPOSITION_TABLE_SIZE 1000003  // 소수로 설정

// 무한대 값 정의
#define INFINITY_VAL 1000000
#define NEG_INFINITY_VAL -1000000

#define BLOCKED_CELL '#' // Added for clarity, though also in octaflip.h

// Game Phase constants
#define PHASE_EARLY 0
#define PHASE_MID 1
#define PHASE_LATE 2

// Helper function to determine the game phase (moved from ai_engine.c)
static inline int get_game_phase(const GameBoard *board) {
    // Ensure board->emptyCount is up-to-date if relying on it.
    // Alternatively, manually count empty cells if GameBoard struct might not be updated by all callers.
    // For now, assume board->emptyCount is reliable.
    int empty_tiles = 0;
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board->cells[r][c] == EMPTY_CELL) {
                empty_tiles++;
            }
        }
    }
    // Thresholds (PHASE_EARLY, etc., must be defined before this point in ai_engine.h)
    if (empty_tiles > 40) return PHASE_EARLY;
    if (empty_tiles > 15) return PHASE_MID;
    return PHASE_LATE;
}

// 위치별 가중치 테이블 (코너가 가장 좋고, 가장자리는 약간 좋음)
extern short POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Evaluation weights - Early Game
#define PIECE_COUNT_WEIGHT_EARLY 10
#define MOBILITY_WEIGHT_EARLY 5
#define STABILITY_WEIGHT_EARLY 20
#define POSITIONAL_WEIGHT_FACTOR_EARLY 1

// Evaluation weights - Mid Game
#define PIECE_COUNT_WEIGHT_MID 15
#define MOBILITY_WEIGHT_MID 3
#define STABILITY_WEIGHT_MID 25
#define POSITIONAL_WEIGHT_FACTOR_MID 1

// Evaluation weights - Late Game
#define PIECE_COUNT_WEIGHT_LATE 20
#define MOBILITY_WEIGHT_LATE 2
#define STABILITY_WEIGHT_LATE 15
#define POSITIONAL_WEIGHT_FACTOR_LATE 1

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
           char maximizing_player, char original_player, int game_phase);
int evaluateBoard(const GameBoard *board, char player, int game_phase);
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
// int get_game_phase(const GameBoard *board); // Prototype removed, function moved inline
Move generateWinningMove(const GameBoard *board, char player);

#endif /* AI_ENGINE_H */