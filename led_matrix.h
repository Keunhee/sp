#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include "octaflip.h"

// LED 매트릭스 색상 정의
typedef struct {
    unsigned char r;  // 빨간색 (0-255)
    unsigned char g;  // 녹색 (0-255)
    unsigned char b;  // 파란색 (0-255)
} LEDColor;

// LED 매트릭스 초기화
int ledMatrixInit();

// LED 매트릭스 종료
void ledMatrixClose();

// LED 매트릭스 픽셀 설정
void setLEDPixel(int x, int y, LEDColor color);

// LED 매트릭스 화면 업데이트
void updateLEDMatrix();

// LED 매트릭스 화면 지우기
void clearLEDMatrix();

// OctaFlip 보드를 LED 매트릭스에 표시
void drawBoardOnLED(const GameBoard *board);

// 기본 색상 정의
extern const LEDColor COLOR_RED;     // 빨간색 (Red 플레이어)
extern const LEDColor COLOR_BLUE;    // 파란색 (Blue 플레이어)
extern const LEDColor COLOR_BLACK;   // 검은색 (배경)
extern const LEDColor COLOR_WHITE;   // 흰색 (그리드선)
extern const LEDColor COLOR_GREEN;   // 녹색 (빈 셀)

#endif /* LED_MATRIX_H */