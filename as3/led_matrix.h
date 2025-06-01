#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include "octaflip.h"

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

#endif /* LED_MATRIX_H */