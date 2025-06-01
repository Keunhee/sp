#ifndef OCTAFLIP_H
#define OCTAFLIP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOARD_SIZE 8
#define RED_PLAYER 'R'
#define BLUE_PLAYER 'B'
#define EMPTY_CELL '.'

// 8방향 이동 벡터 (상하좌우 및 대각선)
extern int dRow[8];
extern int dCol[8];

// 게임 보드 구조체
typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE + 1]; // +1 for null terminator
    char currentPlayer;
    int redCount;
    int blueCount;
    int emptyCount;
    int consecutivePasses;
} GameBoard;

// 이동 구조체
typedef struct {
    int sourceRow;
    int sourceCol;
    int targetRow;
    int targetCol;
    char player;
} Move;

// 보드 초기화 함수
void initializeBoard(GameBoard *board);

// 보드 상태 출력 함수
void printBoard(const GameBoard *board);

// 보드에서 말의 개수 계산
void countPieces(GameBoard *board);

// 빈 칸의 개수 계산
int countEmpty(const GameBoard *board);

// 유효한 이동인지 확인
int isValidMove(const GameBoard *board, Move *move);

// 유효한 이동이 있는지 확인
int hasValidMove(const GameBoard *board, char player);

// 이동 적용 함수
void applyMove(GameBoard *board, Move *move);

// 절대값 계산 함수
int absVal(int x);

// 게임 결과 출력 함수
void printResult(const GameBoard *board);

// 보드 문자열 변환 함수 (JSON용)
char** boardToStringArray(const GameBoard *board);

// 문자열 배열에서 보드로 변환 (JSON용)
void stringArrayToBoard(GameBoard *board, char **boardArray);

// 보드 문자열 배열 메모리 해제 함수
void freeBoardStringArray(char **boardArray);

// 게임 종료 여부 확인
int hasGameEnded(const GameBoard *board);

// 자동 이동 생성 함수
Move generateMove(const GameBoard *board);

#endif /* OCTAFLIP_H */