// led_matrix.c
#include "led_matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "led-matrix-c.h"
// LED 색상 정의
const LEDColor COLOR_RED      = {255, 0, 0};     // R
const LEDColor COLOR_BLUE     = {0, 0, 255};     // B
const LEDColor COLOR_EMPTY    = {17, 17, 17};    // .
const LEDColor COLOR_OBSTACLE = {255, 255, 0};   // #
const LEDColor COLOR_GRID     = {51, 51, 51};    // 격자
const LEDColor COLOR_BLACK    = {0, 0, 0};       // 배경

// 호환성용 색상
const LEDColor COLOR_WHITE    = {51, 51, 51};    // 그리드와 동일
const LEDColor COLOR_GREEN    = {17, 17, 17};    // 빈 공간과 동일

// 매트릭스 객체
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static int width = 0, height = 0;

int ledMatrixInit() {
    struct RGBLedMatrixOptions options;
    memset(&options, 0, sizeof(options));

    // 기본 설정 (필요에 따라 조정 가능)
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";  // 혹은 "adafruit-hat", "adafruit-hat-pwm" 등

    matrix = led_matrix_create_from_options(&options, NULL, NULL);
    if (!matrix) {
        fprintf(stderr, "[LED] 매트릭스 초기화 실패. 시뮬레이션 모드 또는 오류.\n");
        return -1;
    }

    canvas = led_matrix_create_offscreen_canvas(matrix);
    led_canvas_get_size(canvas, &width, &height);

    printf("[LED] 매트릭스 초기화 완료 (%dx%d)\n", width, height);
    return 0;
}

void ledMatrixClose() {
    if (matrix) {
        led_matrix_delete(matrix);
        matrix = NULL;
        canvas = NULL;
    }
}

void setPixel(int x, int y, LEDColor color) {
    if (canvas && x >= 0 && x < width && y >= 0 && y < height) {
        led_canvas_set_pixel(canvas, x, y, color.r, color.g, color.b);
    }
}

void clearMatrix() {
    if (canvas) {
        led_canvas_clear(canvas);
    }
}

void refreshMatrix() {
    if (matrix && canvas) {
        canvas = led_matrix_swap_on_vsync(matrix, canvas);
    }
}
void drawBoardOnLED(const GameBoard *board) {
    clearMatrix();  // 먼저 전체 지우기

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            LEDColor color;
            char cell = board->cells[row][col];
            switch (cell) {
                case 'R': color = COLOR_RED; break;
                case 'B': color = COLOR_BLUE; break;
                case '#': color = COLOR_OBSTACLE; break;
                case '.': color = COLOR_EMPTY; break;
                default:  color = COLOR_BLACK; break;
            }

            // 각 셀을 6x6 블록으로 출력 (1픽셀 그리드 간격 포함)
            int base_x = col * 8 + 1;
            int base_y = row * 8 + 1;
            for (int dy = 0; dy < 6; dy++) {
                for (int dx = 0; dx < 6; dx++) {
                    setPixel(base_x + dx, base_y + dy, color);
                }
            }

            // 격자 라인 색상은 COLOR_GRID
            for (int i = 0; i < 64; i++) {
                setPixel(i, row * 8, COLOR_GRID); // 가로
                setPixel(i, row * 8 + 7, COLOR_GRID);
                setPixel(col * 8, i, COLOR_GRID); // 세로
                setPixel(col * 8 + 7, i, COLOR_GRID);
            }
        }
    }

    refreshMatrix();  // 최종적으로 버퍼 스왑
}
