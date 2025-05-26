#include "octaflip.h"
#include "led_matrix.h"
#include <stdio.h>
#include <stdlib.h>

// 옥타플립 게임 핵심 로직 테스트
int main() {
    GameBoard board;
    Move move;
    
    // 보드 초기화
    printf("보드 초기화 중...\n");
    initializeBoard(&board);
    
    // 초기 보드 출력
    printf("\n초기 보드 상태:\n");
    printBoard(&board);
    
    // 말 개수 확인
    printf("\n말 개수 - Red: %d, Blue: %d, 빈칸: %d\n", 
           board.redCount, board.blueCount, board.emptyCount);
    
    // 유효한 이동 테스트
    printf("\n유효한 이동 테스트:\n");
    
    // Red 플레이어의 첫 번째 이동 (0,0) -> (1,1)
    move.player = RED_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 1;
    move.targetCol = 1;
    
    // 이동 유효성 검사
    if (isValidMove(&board, &move)) {
        printf("유효한 이동: (%d,%d) -> (%d,%d)\n",
               move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
        
        // 이동 적용
        applyMove(&board, &move);
        
        // 업데이트된 보드 출력
        printf("\n이동 후 보드 상태:\n");
        printBoard(&board);
        
        // 말 개수 확인
        printf("\n말 개수 - Red: %d, Blue: %d, 빈칸: %d\n", 
               board.redCount, board.blueCount, board.emptyCount);
    } else {
        printf("유효하지 않은 이동: (%d,%d) -> (%d,%d)\n",
               move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
    }
    
    // Blue 플레이어의 이동 (0,7) -> (1,6)
    move.player = BLUE_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 7;
    move.targetRow = 1;
    move.targetCol = 6;
    
    // 이동 유효성 검사
    if (isValidMove(&board, &move)) {
        printf("\n유효한 이동: (%d,%d) -> (%d,%d)\n",
               move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
        
        // 이동 적용
        applyMove(&board, &move);
        
        // 업데이트된 보드 출력
        printf("\n이동 후 보드 상태:\n");
        printBoard(&board);
        
        // 말 개수 확인
        printf("\n말 개수 - Red: %d, Blue: %d, 빈칸: %d\n", 
               board.redCount, board.blueCount, board.emptyCount);
    } else {
        printf("\n유효하지 않은 이동: (%d,%d) -> (%d,%d)\n",
               move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
    }
    
    // LED 매트릭스 시뮬레이션 테스트
    printf("\nLED 매트릭스 시뮬레이션 테스트:\n");
    ledMatrixInit();
    drawBoardOnLED(&board);
    ledMatrixClose();
    
    // 자동 이동 생성 테스트
    printf("\n자동 이동 생성 테스트:\n");
    board.currentPlayer = RED_PLAYER;
    move = generateMove(&board);
    printf("생성된 이동: (%d,%d) -> (%d,%d)\n",
           move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);
    
    // 보드 상태 확인
    printf("\n현재 보드 상태 검증:\n");
    int redMoves = 0, blueMoves = 0;
    
    // 각 플레이어의 가능한 이동 계산
    if (hasValidMove(&board, RED_PLAYER)) {
        redMoves = 1;
        printf("Red 플레이어는 유효한 이동이 있습니다.\n");
    } else {
        printf("Red 플레이어는 유효한 이동이 없습니다.\n");
    }
    
    if (hasValidMove(&board, BLUE_PLAYER)) {
        blueMoves = 1;
        printf("Blue 플레이어는 유효한 이동이 있습니다.\n");
    } else {
        printf("Blue 플레이어는 유효한 이동이 없습니다.\n");
    }
    
    printf("게임 진행 가능 여부: %s\n", (redMoves || blueMoves) ? "가능" : "불가능");
    
    // 유효한 이동이 없으면 패스 (0,0,0,0)
    if (redMoves == 0) {
        printf("\nRed 플레이어의 패스 테스트:\n");
        move.player = RED_PLAYER;
        move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
        
        if (isValidMove(&board, &move)) {
            printf("패스 유효: Red 플레이어는 유효한 이동이 없습니다.\n");
        } else {
            printf("패스 유효하지 않음: Red 플레이어는 유효한 이동이 있습니다.\n");
        }
    }
    
    // 게임 종료 조건 테스트
    if (hasGameEnded(&board)) {
        printf("\n게임이 종료되었습니다.\n");
        printResult(&board);
    } else {
        printf("\n게임이 계속 진행 중입니다.\n");
    }
    
    return 0;
}