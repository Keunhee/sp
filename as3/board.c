#include <stdint.h>
#include "board.h"
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
    options.disable_hardware_pulsing = 1;

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

#ifdef __aarch64__
inline int count_player_pieces_asm(
    const char *board,
    unsigned char target,
    int rows,
    int cols,
    size_t stride
) {
    int total = 0;

    for (int r = 0; r < rows; r++) {
        const char *row_ptr = &board[r * stride];
        int count;

        asm volatile (
            "ld1    {v0.8b}, [%[row]]         \n"
            "dup    v1.8b, %w[target]         \n"
            "cmeq   v2.8b, v0.8b, v1.8b        \n"
            "ushr   v2.8b, v2.8b, #7           \n"
            "uaddlv h3, v2.8b                  \n"
            "umov   %w[result], v3.h[0]        \n"
            : [result] "=r" (count)
            : [row] "r" (row_ptr),
              [target] "r" ((unsigned int)target)
            : "v0", "v1", "v2", "v3", "memory"
        );

        total += count;
    }

    return total;
}
#else
int count_player_pieces_asm(
    const char *board,
    unsigned char target,
    int rows,
    int cols,
    size_t stride
) {
    int count = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (board[r * stride + c] == (char)target) {
                count++;
            }
        }
    }
    return count;
}
#endif

int dRow[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
int dCol[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

int absVal(int x) {
    return (x < 0) ? -x : x;
}

void initializeBoard(GameBoard *board) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            board->cells[i][j] = EMPTY_CELL;
        }
        board->cells[i][BOARD_SIZE] = '\0';
    }
    board->cells[0][0] = RED_PLAYER;
    board->cells[7][7] = RED_PLAYER;
    board->cells[0][7] = BLUE_PLAYER;
    board->cells[7][0] = BLUE_PLAYER;
    board->currentPlayer = RED_PLAYER;
    board->consecutivePasses = 0;
    countPieces(board);
}

void printBoard(const GameBoard *board) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        printf("%.*s\n", BOARD_SIZE, board->cells[i]);
    }
}

int countEmpty(const GameBoard *board) {
    int cnt = 0;
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            if(board->cells[i][j] == EMPTY_CELL) cnt++;
        }
    }
    return cnt;
}

void countPieces(GameBoard *board) {
#ifdef __aarch64__
  
    board->redCount = count_player_pieces_asm(
        (const char *)board->cells, 
        (unsigned char)RED_PLAYER, 
        BOARD_SIZE, 
        BOARD_SIZE, 
        sizeof(board->cells[0])  // BOARD_SIZE + 1
    );
    
    board->blueCount = count_player_pieces_asm(
        (const char *)board->cells, 
        (unsigned char)BLUE_PLAYER, 
        BOARD_SIZE, 
        BOARD_SIZE, 
        sizeof(board->cells[0])  // BOARD_SIZE + 1
    );
    
    board->emptyCount = countEmpty(board);

#else
    board->redCount = 0;
    board->blueCount = 0;
    board->emptyCount = 0;
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            char c = board->cells[i][j];
            if(c == RED_PLAYER) board->redCount++;
            else if(c == BLUE_PLAYER) board->blueCount++;
            else if(c == EMPTY_CELL) board->emptyCount++;
        }
    }
#endif
}

int hasValidMove(const GameBoard *board, char player) {
    for(int r = 0; r < BOARD_SIZE; r++) {
        for(int c = 0; c < BOARD_SIZE; c++) {
            if(board->cells[r][c] != player) continue;
            for(int d = 0; d < 8; d++) {
                for(int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    if(nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                        continue;
                    if(s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if(board->cells[mr][mc] == RED_PLAYER ||
                           board->cells[mr][mc] == BLUE_PLAYER)
                            continue;
                    }
                    if(board->cells[nr][nc] == EMPTY_CELL)
                        return 1;
                }
            }
        }
    }
    return 0;
}

int isValidMove(const GameBoard *board, Move *move) {
    int r1 = move->sourceRow, c1 = move->sourceCol;
    int r2 = move->targetRow, c2 = move->targetCol;
    char current = move->player;
    if(r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0)
        return !hasValidMove(board, current);
    if(r1 < 0 || r1 >= BOARD_SIZE || c1 < 0 || c1 >= BOARD_SIZE ||
       r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE)
        return 0;
    if(board->cells[r1][c1] != current) return 0;
    if(board->cells[r2][c2] != EMPTY_CELL) return 0;
    int dr = r2 - r1, dc = c2 - c1;
    int absDr = absVal(dr), absDc = absVal(dc);
    int maxD = (absDr > absDc) ? absDr : absDc;
    if(maxD != 1 && maxD != 2) return 0;
    if(absDr != 0 && absDc != 0 && absDr != absDc) return 0;
    if(maxD == 2) {
        int mr = r1 + dr/2, mc = c1 + dc/2;
        if(board->cells[mr][mc] == RED_PLAYER ||
           board->cells[mr][mc] == BLUE_PLAYER)
            return 0;
    }
    return 1;
}

void applyMove(GameBoard *board, Move *move) {
    int r1 = move->sourceRow, c1 = move->sourceCol;
    int r2 = move->targetRow, c2 = move->targetCol;
    char current = move->player;
    char opponent = (current == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    int dr = r2 - r1, dc = c2 - c1;
    int absDr = absVal(dr), absDc = absVal(dc);
    int maxD = (absDr > absDc) ? absDr : absDc;
    if(maxD == 2) board->cells[r1][c1] = EMPTY_CELL;
    board->cells[r2][c2] = current;
    for(int d = 0; d < 8; d++) {
        int nr = r2 + dRow[d], nc = c2 + dCol[d];
        if(nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
            continue;
        if(board->cells[nr][nc] == opponent)
            board->cells[nr][nc] = current;
    }
    countPieces(board);
}

void printResult(const GameBoard *board) {
    if(board->redCount > board->blueCount) printf("Red\n");
    else if(board->blueCount > board->redCount) printf("Blue\n");
    else printf("Draw\n");
}

char** boardToStringArray(const GameBoard *board) {
    char **boardArray = malloc(BOARD_SIZE * sizeof(char*));
    if(!boardArray) exit(1);
    for(int i = 0; i < BOARD_SIZE; i++) {
        boardArray[i] = malloc((BOARD_SIZE + 1) * sizeof(char));
        if(!boardArray[i]) {
            for(int j = 0; j < i; j++) free(boardArray[j]);
            free(boardArray);
            exit(1);
        }
        strncpy(boardArray[i], board->cells[i], BOARD_SIZE);
        boardArray[i][BOARD_SIZE] = '\0';
    }
    return boardArray;
}

void freeBoardStringArray(char **boardArray) {
    if(!boardArray) return;
    for(int i = 0; i < BOARD_SIZE; i++) {
        free(boardArray[i]);
    }
    free(boardArray);
}

void stringArrayToBoard(GameBoard *board, char **boardArray) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        strncpy(board->cells[i], boardArray[i], BOARD_SIZE + 1);
    }
    countPieces(board);
}

int hasGameEnded(const GameBoard *board) {
    if(board->redCount == 0 || board->blueCount == 0) return 1;
    if(countEmpty(board) == 0) return 1;
    if(board->consecutivePasses >= 2) return 1;
    return 0;
}

Move generateMove(const GameBoard *board) {
    Move move;
    move.player = board->currentPlayer;
    for(int r = 0; r < BOARD_SIZE; r++) {
        for(int c = 0; c < BOARD_SIZE; c++) {
            if(board->cells[r][c] != move.player) continue;
            for(int d = 0; d < 8; d++) {
                for(int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    if(nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                        continue;
                    if(s == 2) {
                        int mr = r + dRow[d], mc = c + dCol[d];
                        if(board->cells[mr][mc] == RED_PLAYER ||
                           board->cells[mr][mc] == BLUE_PLAYER)
                            continue;
                    }
                    if(board->cells[nr][nc] == EMPTY_CELL) {
                        move.sourceRow = r;
                        move.sourceCol = c;
                        move.targetRow = nr;
                        move.targetCol = nc;
                        return move;
                    }
                }
            }
        }
    }
    move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
    return move;
}

#ifdef BOARD_STANDALONE
int main() {
    GameBoard board;
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            board.cells[i][j] = EMPTY_CELL;
        }
        board.cells[i][BOARD_SIZE] = '\0';
    }
    for(int i = 0; i < 8; i++) {
        if (scanf("%8s", board.cells[i]) != 1) {
            printf("Invalid input\n");
            return 0;
        }
        int count = 0;
        for (int i2 = 0; i2<8; i2++){
            if (board.cells[i][i2] == 'R' || board.cells[i][i2] == 'B' ||board.cells[i][i2] == '.' || board.cells[i][i2] == '#')
            {
                count++;
            }
        }
        if(count != 8){
            printf("Board input error\n");
            exit(0);
        }   
    }
    ledMatrixInit();
    drawBoardOnLED(&board);
    sleep 3;
    ledMatrixClose();
    return 0;
}
#endif
