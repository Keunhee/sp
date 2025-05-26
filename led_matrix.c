#include "led_matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

// 기본 색상 정의
const LEDColor COLOR_RED   = {255, 0, 0};      // 빨간색 (Red 플레이어)
const LEDColor COLOR_BLUE  = {0, 0, 255};      // 파란색 (Blue 플레이어)
const LEDColor COLOR_BLACK = {0, 0, 0};        // 검은색 (배경)
const LEDColor COLOR_WHITE = {255, 255, 255};  // 흰색 (그리드선)
const LEDColor COLOR_GREEN = {0, 150, 0};      // 녹색 (빈 셀)

// LED 매트릭스 크기 정의 (8x8 게임 보드)
#define LED_MATRIX_WIDTH 64
#define LED_MATRIX_HEIGHT 64
#define CELL_SIZE 8  // 각 셀의 크기 (픽셀)

// 가상 LED 매트릭스 버퍼
static LEDColor ledBuffer[LED_MATRIX_HEIGHT][LED_MATRIX_WIDTH];

// GPIO 관련 정의 (하드웨어 제어용)
#define BCM2708_PERI_BASE 0x3F000000  // Raspberry Pi 3 기준
#define GPIO_BASE         (BCM2708_PERI_BASE + 0x200000)
#define BLOCK_SIZE        (4*1024)

// GPIO 액세스를 위한 변수
static volatile unsigned *gpio = NULL;
static int mem_fd = -1;

// GPIO 핀 정의 (하드웨어 제어용)
#define OE  4   // Output Enable
#define CLK 17  // Clock
#define LAT 21  // Latch
#define A   22  // Address A
#define B   23  // Address B
#define C   24  // Address C
#define D   25  // Address D
#define R1  5   // Red 1
#define G1  13  // Green 1
#define B1  6   // Blue 1
#define R2  12  // Red 2
#define G2  16  // Green 2
#define B2  26  // Blue 2

// GPIO 제어 매크로 (하드웨어 제어용)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define GPIO_SET *(gpio+7)
#define GPIO_CLR *(gpio+10)
#define GPIO_READ(g) (*(gpio+13) & (1<<g))

// 시뮬레이션 모드 플래그
static int simulation_mode = 1;

// 시뮬레이션 모드에서 보드 출력 함수
static void printBufferAsBoard() {
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            // 셀의 중앙 픽셀 색상 확인
            int centerX = x * CELL_SIZE + CELL_SIZE / 2;
            int centerY = y * CELL_SIZE + CELL_SIZE / 2;
            LEDColor pixelColor = ledBuffer[centerY][centerX];
            
            // 색상에 따라 문자 출력
            if (pixelColor.r > 200 && pixelColor.g < 50 && pixelColor.b < 50) {
                printf("R ");  // 빨간색 (Red 플레이어)
            } else if (pixelColor.r < 50 && pixelColor.g < 50 && pixelColor.b > 200) {
                printf("B ");  // 파란색 (Blue 플레이어)
            } else if (pixelColor.r < 50 && pixelColor.g > 100 && pixelColor.b < 50) {
                printf(". ");  // 녹색 (빈 셀)
            } else {
                printf("  ");  // 기타 색상
            }
        }
        printf("\n");
    }
}

// LED 매트릭스 초기화
int ledMatrixInit() {
    // 버퍼 초기화
    clearLEDMatrix();
    
    // 하드웨어 LED 매트릭스 초기화 시도
    mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0) {
        printf("LED Matrix: 시뮬레이션 모드로 실행합니다.\n");
        simulation_mode = 1;
        return 0;
    }
    
    // GPIO 메모리 매핑
    gpio = (volatile unsigned *)mmap(
        NULL,
        BLOCK_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO_BASE
    );
    
    if (gpio == MAP_FAILED) {
        close(mem_fd);
        printf("LED Matrix: GPIO 매핑 실패, 시뮬레이션 모드로 실행합니다.\n");
        simulation_mode = 1;
        return 0;
    }
    
    // GPIO 핀 설정
    INP_GPIO(OE);  OUT_GPIO(OE);
    INP_GPIO(CLK); OUT_GPIO(CLK);
    INP_GPIO(LAT); OUT_GPIO(LAT);
    INP_GPIO(A);   OUT_GPIO(A);
    INP_GPIO(B);   OUT_GPIO(B);
    INP_GPIO(C);   OUT_GPIO(C);
    INP_GPIO(D);   OUT_GPIO(D);
    INP_GPIO(R1);  OUT_GPIO(R1);
    INP_GPIO(G1);  OUT_GPIO(G1);
    INP_GPIO(B1);  OUT_GPIO(B1);
    INP_GPIO(R2);  OUT_GPIO(R2);
    INP_GPIO(G2);  OUT_GPIO(G2);
    INP_GPIO(B2);  OUT_GPIO(B2);
    
    // 초기 상태 설정
    GPIO_SET = 1 << OE;  // 출력 비활성화
    GPIO_CLR = 1 << CLK; // 클럭 로우
    GPIO_CLR = 1 << LAT; // 래치 로우
    
    simulation_mode = 0;
    printf("LED Matrix: 하드웨어 초기화 완료\n");
    return 0;
}

// LED 매트릭스 종료
void ledMatrixClose() {
    if (!simulation_mode && gpio != NULL && gpio != MAP_FAILED) {
        // 모든 LED 끄기
        clearLEDMatrix();
        updateLEDMatrix();
        
        // GPIO 매핑 해제
        munmap((void*)gpio, BLOCK_SIZE);
        gpio = NULL;
        
        // 파일 디스크립터 닫기
        if (mem_fd >= 0) {
            close(mem_fd);
            mem_fd = -1;
        }
    }
}

// LED 매트릭스 픽셀 설정
void setLEDPixel(int x, int y, LEDColor color) {
    // 범위 검사
    if (x < 0 || x >= LED_MATRIX_WIDTH || y < 0 || y >= LED_MATRIX_HEIGHT)
        return;
    
    // 버퍼에 픽셀 설정
    ledBuffer[y][x] = color;
}

// LED 매트릭스 화면 업데이트 (하드웨어)
static void updateLEDMatrixHardware() {
    if (simulation_mode || gpio == NULL || gpio == MAP_FAILED)
        return;
    
    // 각 라인 스캔
    for (int line = 0; line < 32; line++) {
        // 라인 주소 설정 (A, B, C, D)
        if (line & 1) GPIO_SET = 1 << A; else GPIO_CLR = 1 << A;
        if (line & 2) GPIO_SET = 1 << B; else GPIO_CLR = 1 << B;
        if (line & 4) GPIO_SET = 1 << C; else GPIO_CLR = 1 << C;
        if (line & 8) GPIO_SET = 1 << D; else GPIO_CLR = 1 << D;
        
        // 픽셀 데이터 설정
        for (int x = 0; x < 64; x++) {
            // 상단 절반
            if (ledBuffer[line][x].r > 128) GPIO_SET = 1 << R1; else GPIO_CLR = 1 << R1;
            if (ledBuffer[line][x].g > 128) GPIO_SET = 1 << G1; else GPIO_CLR = 1 << G1;
            if (ledBuffer[line][x].b > 128) GPIO_SET = 1 << B1; else GPIO_CLR = 1 << B1;
            
            // 하단 절반
            if (ledBuffer[line + 32][x].r > 128) GPIO_SET = 1 << R2; else GPIO_CLR = 1 << R2;
            if (ledBuffer[line + 32][x].g > 128) GPIO_SET = 1 << G2; else GPIO_CLR = 1 << G2;
            if (ledBuffer[line + 32][x].b > 128) GPIO_SET = 1 << B2; else GPIO_CLR = 1 << B2;
            
            // 클럭 펄스
            GPIO_SET = 1 << CLK;
            GPIO_CLR = 1 << CLK;
        }
        
        // 래치 펄스
        GPIO_SET = 1 << LAT;
        GPIO_CLR = 1 << LAT;
        
        // 출력 활성화
        GPIO_CLR = 1 << OE;
        
        // 지연 (밝기 제어)
        struct timespec ts = {0, 100000}; // 100 마이크로초
        nanosleep(&ts, NULL);
        
        // 출력 비활성화
        GPIO_SET = 1 << OE;
    }
}

// LED 매트릭스 화면 업데이트
void updateLEDMatrix() {
    if (simulation_mode) {
        // 시뮬레이션 모드에서는 콘솔에 출력
        printf("\nLED 매트릭스 화면 (시뮬레이션):\n");
        printBufferAsBoard();
    } else {
        // 하드웨어 모드에서는 실제 LED 매트릭스 업데이트
        updateLEDMatrixHardware();
    }
}

// LED 매트릭스 화면 지우기
void clearLEDMatrix() {
    // 모든 픽셀을 검은색으로 설정
    for (int y = 0; y < LED_MATRIX_HEIGHT; y++) {
        for (int x = 0; x < LED_MATRIX_WIDTH; x++) {
            ledBuffer[y][x] = COLOR_BLACK;
        }
    }
}

// 셀 그리기 (내부 함수)
static void drawCell(int boardX, int boardY, LEDColor color) {
    int startX = boardX * CELL_SIZE;
    int startY = boardY * CELL_SIZE;
    
    // 셀 내부 채우기
    for (int y = 0; y < CELL_SIZE - 1; y++) {
        for (int x = 0; x < CELL_SIZE - 1; x++) {
            setLEDPixel(startX + x, startY + y, color);
        }
    }
    
    // 셀 테두리 그리기 (흰색)
    for (int i = 0; i < CELL_SIZE; i++) {
        setLEDPixel(startX + i, startY, COLOR_WHITE);               // 상단 가로선
        setLEDPixel(startX + i, startY + CELL_SIZE - 1, COLOR_WHITE); // 하단 가로선
        setLEDPixel(startX, startY + i, COLOR_WHITE);               // 좌측 세로선
        setLEDPixel(startX + CELL_SIZE - 1, startY + i, COLOR_WHITE); // 우측 세로선
    }
}

// OctaFlip 보드를 LED 매트릭스에 표시
void drawBoardOnLED(const GameBoard *board) {
    // 버퍼 초기화
    clearLEDMatrix();
    
    // 각 셀 그리기
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            LEDColor cellColor;
            
            // 셀 상태에 따른 색상 선택
            switch (board->cells[y][x]) {
                case RED_PLAYER:
                    cellColor = COLOR_RED;
                    break;
                case BLUE_PLAYER:
                    cellColor = COLOR_BLUE;
                    break;
                case EMPTY_CELL:
                    cellColor = COLOR_GREEN;
                    break;
                default:
                    cellColor = COLOR_BLACK;
                    break;
            }
            
            // 셀 그리기
            drawCell(x, y, cellColor);
        }
    }
    
    // 화면 업데이트
    updateLEDMatrix();
}