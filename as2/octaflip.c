#include "octaflip.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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