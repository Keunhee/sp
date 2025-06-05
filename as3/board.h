#ifndef BOARD_H
#define BOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOARD_SIZE 8
#define RED_PLAYER 'R'
#define BLUE_PLAYER 'B'
#define EMPTY_CELL '.'
#define BLOCKED_CELL '#'

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
int count_player_pieces_asm(
    const char *board,
    unsigned char target,
    int rows,
    int cols,
    size_t stride
);
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

// LED 매트릭스 색상 구조체
typedef struct {
    int r;  // 빨강 (0~255)
    int g;  // 초록 (0~255)
    int b;  // 파랑 (0~255)
} LEDColor;

// 매트릭스 초기화 및 종료
int ledMatrixInit();       // 매트릭스 초기화
void ledMatrixClose();     // 매트릭스 종료

// 픽셀 조작 함수
void setPixel(int x, int y, LEDColor color);   // 픽셀 색 설정
void clearMatrix();                            // 전체 매트릭스 클리어
void refreshMatrix();                          // 화면 업데이트 (double buffering)

// 게임 보드를 매트릭스에 표시
void drawBoardOnLED(const GameBoard *board);

// 과제 요구 색상 정의
extern const LEDColor COLOR_RED;       // R 플레이어
extern const LEDColor COLOR_BLUE;      // B 플레이어
extern const LEDColor COLOR_EMPTY;     // 빈 공간
extern const LEDColor COLOR_OBSTACLE;  // 장애물
extern const LEDColor COLOR_GRID;      // 격자 라인
extern const LEDColor COLOR_BLACK;     // 배경

// 호환성 유지용 별칭
extern const LEDColor COLOR_WHITE;     // 그리드용 흰색 또는 회색
extern const LEDColor COLOR_GREEN;     // 빈 셀 호환성

#endif /* OCTAFLIP_H */