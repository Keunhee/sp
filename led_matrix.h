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

// 과제 요구사항에 맞는 색상 정의 (RGB 값 명시)
extern const LEDColor COLOR_RED;       // 빨간색 (255,0,0) - R 플레이어
extern const LEDColor COLOR_BLUE;      // 파란색 (0,0,255) - B 플레이어  
extern const LEDColor COLOR_EMPTY;     // 어두운 회색 (17,17,17) - 빈 공간 (.)
extern const LEDColor COLOR_OBSTACLE;  // 노란색 (255,255,0) - 장애물 (#)
extern const LEDColor COLOR_GRID;      // 회색 (51,51,51) - 그리드 라인
extern const LEDColor COLOR_BLACK;     // 검은색 (0,0,0) - 배경

// 기존 코드와의 호환성을 위한 별칭
extern const LEDColor COLOR_WHITE;     // 그리드 라인과 동일 (호환성)
extern const LEDColor COLOR_GREEN;     // 빈 셀과 동일 (호환성)

#endif /* LED_MATRIX_H */
