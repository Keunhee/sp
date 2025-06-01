#include "board.h"
#include <string.h>
#include <stdio.h>

GameBoard game_board;

void initBoard() {
    initializeBoard(&game_board);
}

void updateBoard(const GameBoard *new_board) {
    memcpy(&game_board, new_board, sizeof(GameBoard));
}

GameBoard *getBoard() {
    return &game_board;
}

void displayBoard() {
    printBoard(&game_board);
}
