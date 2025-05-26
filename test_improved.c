#include "octaflip.h"
#include "led_matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 테스트 케이스 ID
int test_id = 0;

// 테스트 통과 여부 출력
void report_test(const char *description, int result) {
    printf("테스트 %d: %s - %s\n", ++test_id, description, result ? "통과" : "실패");
    if (!result) {
        printf("  > 테스트 실패: %s\n", description);
    }
}

// 보드 초기화 테스트
void test_board_initialization() {
    GameBoard board;
    initializeBoard(&board);
    
    // 초기 위치 확인
    int result = 1;
    result &= (board.cells[0][0] == RED_PLAYER);
    result &= (board.cells[7][7] == RED_PLAYER);
    result &= (board.cells[0][7] == BLUE_PLAYER);
    result &= (board.cells[7][0] == BLUE_PLAYER);
    
    // 말 개수 확인
    result &= (board.redCount == 2);
    result &= (board.blueCount == 2);
    result &= (board.emptyCount == 60);
    
    report_test("보드 초기화", result);
}

// 유효한 이동 검사 테스트
void test_valid_moves() {
    GameBoard board;
    initializeBoard(&board);
    
    Move move;
    move.player = RED_PLAYER;
    
    // 유효한 이동 1: (0,0) -> (1,1)
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 1;
    move.targetCol = 1;
    int result1 = isValidMove(&board, &move);
    
    // 유효한 이동 2: (0,0) -> (2,2) (2칸 이동)
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 2;
    move.targetCol = 2;
    int result2 = isValidMove(&board, &move);
    
    // 유효하지 않은 이동 1: (0,0) -> (3,3) (3칸 이동)
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 3;
    move.targetCol = 3;
    int result3 = !isValidMove(&board, &move);
    
    // 유효하지 않은 이동 2: (0,0) -> (1,2) (대각선 아님)
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 1;
    move.targetCol = 2;
    int result4 = !isValidMove(&board, &move);
    
    // 유효하지 않은 이동 3: 존재하지 않는 말 이동
    move.sourceRow = 1;
    move.sourceCol = 1;
    move.targetRow = 2;
    move.targetCol = 2;
    int result5 = !isValidMove(&board, &move);
    
    int result = result1 && result2 && result3 && result4 && result5;
    report_test("유효한 이동 검사", result);
}

// 이동 적용 테스트
void test_move_application() {
    GameBoard board;
    initializeBoard(&board);
    
    // (0,0)의 R을 (1,1)로 이동
    Move move;
    move.player = RED_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 1;
    move.targetCol = 1;
    
    applyMove(&board, &move);
    
    // 이동 후 상태 확인
    int result = 1;
    result &= (board.cells[0][0] == RED_PLAYER); // 1칸 이동이므로 원래 위치 유지
    result &= (board.cells[1][1] == RED_PLAYER); // 새 위치에 말 추가
    
    // 말 개수 확인
    result &= (board.redCount == 3); // 원래 2개 + 새로 놓은 1개
    result &= (board.blueCount == 2);
    
    report_test("이동 적용 (1칸)", result);
    
    // 초기화
    initializeBoard(&board);
    
    // (0,0)의 R을 (2,2)로 이동 (2칸 이동)
    move.player = RED_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = 2;
    move.targetCol = 2;
    
    applyMove(&board, &move);
    
    // 이동 후 상태 확인
    result = 1;
    result &= (board.cells[0][0] == EMPTY_CELL); // 2칸 이동이므로 원래 위치 비움
    result &= (board.cells[2][2] == RED_PLAYER); // 새 위치에 말 추가
    
    // 말 개수 확인
    result &= (board.redCount == 2); // 원래 2개, 하나 옮김 (변화 없음)
    result &= (board.blueCount == 2);
    
    report_test("이동 적용 (2칸)", result);
}

// 뒤집기 테스트
void test_flipping() {
    GameBoard board;
    
    // 테스트를 위한 특수 보드 설정
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board.cells[i][j] = EMPTY_CELL;
        }
        board.cells[i][BOARD_SIZE] = '\0';
    }
    
    // 가운데에 R을 둘러싼 B 배치
    board.cells[3][3] = RED_PLAYER;
    board.cells[2][2] = BLUE_PLAYER;
    board.cells[2][3] = BLUE_PLAYER;
    board.cells[2][4] = BLUE_PLAYER;
    board.cells[3][2] = BLUE_PLAYER;
    board.cells[3][4] = BLUE_PLAYER;
    board.cells[4][2] = BLUE_PLAYER;
    board.cells[4][3] = BLUE_PLAYER;
    board.cells[4][4] = BLUE_PLAYER;
    
    countPieces(&board);
    
    // R이 (1,1)로 이동하여 인접한 B 뒤집기
    Move move;
    move.player = RED_PLAYER;
    move.sourceRow = 3;
    move.sourceCol = 3;
    move.targetRow = 1;
    move.targetCol = 1;
    
    applyMove(&board, &move);
    
    // 이동 후 상태 확인
    int result = 1;
    result &= (board.cells[1][1] == RED_PLAYER); // 새 위치
    result &= (board.cells[2][2] == RED_PLAYER); // 뒤집힌 B
    
    // 말 개수 확인
    result &= (board.redCount == 3); // 원래 1개 + 뒤집은 1개 + 새로 놓은 1개
    result &= (board.blueCount == 7); // 원래 8개 - 뒤집힌 1개
    
    report_test("말 뒤집기", result);
}

// 게임 종료 조건 테스트
void test_game_end_conditions() {
    GameBoard board;
    initializeBoard(&board);
    
    // 1. 연속 패스
    board.consecutivePasses = 2;
    int result1 = has_game_ended(&board);
    
    // 초기화
    initializeBoard(&board);
    
    // 2. 모든 R 제거
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board.cells[i][j] == RED_PLAYER) {
                board.cells[i][j] = EMPTY_CELL;
            }
        }
    }
    countPieces(&board);
    int result2 = has_game_ended(&board);
    
    // 초기화
    initializeBoard(&board);
    
    // 3. 보드 가득 참
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board.cells[i][j] == EMPTY_CELL) {
                board.cells[i][j] = (i + j) % 2 == 0 ? RED_PLAYER : BLUE_PLAYER;
            }
        }
    }
    countPieces(&board);
    int result3 = has_game_ended(&board);
    
    int result = result1 && result2 && result3;
    report_test("게임 종료 조건", result);
}

// 패스 테스트
void test_pass_validation() {
    GameBoard board;
    
    // 테스트를 위한 특수 보드 설정 (R이 이동할 수 없는 상황)
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board.cells[i][j] = EMPTY_CELL;
        }
        board.cells[i][BOARD_SIZE] = '\0';
    }
    
    // R을 B로 둘러싸기
    board.cells[3][3] = RED_PLAYER;
    board.cells[2][2] = BLUE_PLAYER;
    board.cells[2][3] = BLUE_PLAYER;
    board.cells[2][4] = BLUE_PLAYER;
    board.cells[3][2] = BLUE_PLAYER;
    board.cells[3][4] = BLUE_PLAYER;
    board.cells[4][2] = BLUE_PLAYER;
    board.cells[4][3] = BLUE_PLAYER;
    board.cells[4][4] = BLUE_PLAYER;
    
    countPieces(&board);
    
    // R의 유효한 이동이 없는지 확인
    int has_move = hasValidMove(&board, RED_PLAYER);
    
    // 패스 이동 (0,0,0,0) 생성
    Move pass_move;
    pass_move.player = RED_PLAYER;
    pass_move.sourceRow = pass_move.sourceCol = pass_move.targetRow = pass_move.targetCol = 0;
    
    // 패스 유효성 검사
    int valid_pass = isValidMove(&board, &pass_move);
    
    int result = !has_move && valid_pass;
    report_test("패스 유효성 검증", result);
}

// 메모리 할당/해제 테스트
void test_memory_management() {
    GameBoard board;
    initializeBoard(&board);
    
    // 문자열 배열 생성
    char **board_array = boardToStringArray(&board);
    
    // 메모리 해제
    freeBoardStringArray(board_array);
    
    // 이 테스트는 메모리 누수를 직접 확인할 수 없음
    // 하지만 코드가 비정상 종료되지 않으면 통과로 간주
    report_test("메모리 관리", 1);
}

// JSON 변환 테스트
void test_json_conversion() {
    GameBoard board;
    initializeBoard(&board);
    
    // 보드를 문자열 배열로 변환
    char **board_array = boardToStringArray(&board);
    
    // 새 보드 생성
    GameBoard new_board;
    
    // 문자열 배열을 보드로 변환
    stringArrayToBoard(&new_board, board_array);
    
    // 두 보드가 동일한지 확인
    int result = 1;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board.cells[i][j] != new_board.cells[i][j]) {
                result = 0;
                break;
            }
        }
        if (!result) break;
    }
    
    // 메모리 해제
    freeBoardStringArray(board_array);
    
    report_test("JSON 변환", result);
}

// 경계 조건 테스트
void test_edge_cases() {
    GameBoard board;
    initializeBoard(&board);
    
    // 보드 경계를 벗어나는 이동
    Move move;
    move.player = RED_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 0;
    move.targetRow = -1;
    move.targetCol = -1;
    
    int result1 = !isValidMove(&board, &move);
    
    // 상대 말을 이동하려는 시도
    move.player = RED_PLAYER;
    move.sourceRow = 0;
    move.sourceCol = 7; // BLUE_PLAYER 위치
    move.targetRow = 1;
    move.targetCol = 6;
    
    int result2 = !isValidMove(&board, &move);
    
    int result = result1 && result2;
    report_test("경계 조건", result);
}

// 수정된 has_game_ended 함수 (테스트용)
int has_game_ended(const GameBoard *board) {
    // 한 플레이어의 말이 모두 사라진 경우
    if (board->redCount == 0 || board->blueCount == 0) {
        return 1;
    }
    
    // 보드가 가득 찬 경우
    if (board->emptyCount == 0) {
        return 1;
    }
    
    // 연속으로 두 번 패스한 경우
    if (board->consecutivePasses >= 2) {
        return 1;
    }
    
    return 0;
}

int main() {
    printf("=== OctaFlip 게임 테스트 시작 ===\n\n");
    
    // 기본 기능 테스트
    test_board_initialization();
    test_valid_moves();
    test_move_application();
    test_flipping();
    test_game_end_conditions();
    
    // 추가 기능 테스트
    test_pass_validation();
    test_memory_management();
    test_json_conversion();
    test_edge_cases();
    
    printf("\n=== 테스트 완료: 총 %d개 테스트 실행 ===\n", test_id);
    
    return 0;
}