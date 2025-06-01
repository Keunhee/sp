#ifndef BOARD_H
#define BOARD_H

#include "octaflip.h"

extern GameBoard game_board;

void initBoard();
void updateBoard(const GameBoard *new_board);
GameBoard *getBoard();
void displayBoard();

#endif /* BOARD_H */
