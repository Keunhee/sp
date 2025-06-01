#ifndef WINNING_STRATEGY_H
#define WINNING_STRATEGY_H

#include "octaflip.h"

// 초반 정석 이동들
typedef struct {
    char initial_board[BOARD_SIZE][BOARD_SIZE + 1];
    Move best_move;
    int confidence;  // 0-100, 높을수록 확실한 승리 이동
} OpeningMove;

// 초반 정석 데이터베이스
extern OpeningMove opening_book[];
extern int opening_book_size;

// 종반 완전 해결 임계값 (남은 빈 칸이 이 수 이하일 때 완전 계산)
#define ENDGAME_THRESHOLD 8

// 함수 선언
Move checkOpeningBook(const GameBoard *board, char player);
Move solveEndgame(const GameBoard *board, char player);
int isOpeningPhase(const GameBoard *board);
int isEndgamePhase(const GameBoard *board);
int calculateMaterial(const GameBoard *board, char player);
int calculateThreatLevel(const GameBoard *board, char player);
Move findKillerMove(const GameBoard *board, char player);
int isWinningPosition(const GameBoard *board, char player);

#endif /* WINNING_STRATEGY_H */