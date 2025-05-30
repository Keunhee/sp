#include "octaflip.h"
#include "ai_engine.h"
#include <stdio.h>
#include <time.h>

void test_game_scenario(const char* name, GameBoard *board, char player) {
    printf("\n=== %s ===\n", name);
    printf("현재 보드:\n");
    printBoard(board);
    printf("R=%d, B=%d, Empty=%d\n\n", board->redCount, board->blueCount, board->emptyCount);
    
    clock_t start = clock();
    Move move = generateWinningMove(board, player);
    clock_t end = clock();
    
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (move.sourceRow == 0 && move.sourceCol == 0 && 
        move.targetRow == 0 && move.targetCol == 0) {
        printf("AI 결정: 패스\n");
    } else {
        printf("AI 결정: (%d,%d) -> (%d,%d)\n", 
               move.sourceRow, move.sourceCol, 
               move.targetRow, move.targetCol);
        
        applyMove(board, &move);
        
        printf("\n이동 후 보드:\n");
        printBoard(board);
        printf("R=%d, B=%d, Empty=%d\n", board->redCount, board->blueCount, board->emptyCount);
    }
    
    printf("계산 시간: %.2f초\n", elapsed);
}

int main() {
    printf("=== OctaFlip AI 종합 테스트 ===\n");
    
    // 테스트 1: 초기 상태
    GameBoard board1;
    initializeBoard(&board1);
    test_game_scenario("초기 상태 테스트", &board1, RED_PLAYER);
    
    // 테스트 2: 중반 상황 시뮬레이션
    GameBoard board2;
    initializeBoard(&board2);
    // 몇 수 진행
    strcpy(board2.cells[0], "R..RR..B");
    strcpy(board2.cells[1], ".RR.....");
    strcpy(board2.cells[2], "........");
    strcpy(board2.cells[3], "........");
    strcpy(board2.cells[4], "........");
    strcpy(board2.cells[5], "........");
    strcpy(board2.cells[6], ".....BB.");
    strcpy(board2.cells[7], "B..BB..R");
    countPieces(&board2);
    test_game_scenario("중반 복잡한 상황", &board2, RED_PLAYER);
    
    // 테스트 3: 종반 상황 (빈 칸 적음)
    GameBoard board3;
    initializeBoard(&board3);
    // 종반 시뮬레이션
    strcpy(board3.cells[0], "RRRRRRRB");
    strcpy(board3.cells[1], "RRRRRRRB");
    strcpy(board3.cells[2], "RRRRRRRB");
    strcpy(board3.cells[3], "RRRRRRRB");
    strcpy(board3.cells[4], "RRRRRRRB");
    strcpy(board3.cells[5], "RRRRRRRB");
    strcpy(board3.cells[6], "RRRRR..B");
    strcpy(board3.cells[7], "BBBBBBBR");
    countPieces(&board3);
    test_game_scenario("종반 상황 (완전 계산)", &board3, RED_PLAYER);
    
    printf("\n=== 모든 테스트 완료 ===\n");
    return 0;
}