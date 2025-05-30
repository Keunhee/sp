#include "octaflip.h"
#include "ai_engine.h"
#include <stdio.h>

int main() {
    printf("=== AI 엔진 테스트 시작 ===\n");
    
    // 게임 보드 초기화
    GameBoard board;
    initializeBoard(&board);
    
    printf("초기 보드 상태:\n");
    printBoard(&board);
    printf("R=%d, B=%d, Empty=%d\n\n", board.redCount, board.blueCount, board.emptyCount);
    
    // 빨간색 플레이어가 먼저 이동
    printf("빨간색 플레이어(AI) 턴:\n");
    Move red_move = generateWinningMove(&board, RED_PLAYER);
    
    if (red_move.sourceRow == 0 && red_move.sourceCol == 0 && 
        red_move.targetRow == 0 && red_move.targetCol == 0) {
        printf("AI가 패스를 선택했습니다.\n");
    } else {
        printf("AI 선택: (%d,%d) -> (%d,%d)\n", 
               red_move.sourceRow, red_move.sourceCol, 
               red_move.targetRow, red_move.targetCol);
        
        // 이동 적용
        applyMove(&board, &red_move);
        
        printf("\n이동 후 보드 상태:\n");
        printBoard(&board);
        printf("R=%d, B=%d, Empty=%d\n", board.redCount, board.blueCount, board.emptyCount);
    }
    
    printf("\n=== AI 엔진 테스트 완료 ===\n");
    return 0;
}