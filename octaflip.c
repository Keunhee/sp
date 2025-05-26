#include "octaflip.h"

/**
 * 8방향 이동 벡터 (상하좌우 및 대각선)
 * 
 * 이 배열은 8방향의 이동을 나타내는 상대 좌표를 정의합니다:
 * 0: 좌상(-1,-1)   1: 상(-1,0)    2: 우상(-1,1)
 * 3: 좌(0,-1)                     4: 우(0,1)
 * 5: 좌하(1,-1)    6: 하(1,0)     7: 우하(1,1)
 */
int dRow[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
int dCol[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

// 절대값 계산 함수
int absVal(int x) {
    return (x < 0) ? -x : x;
}

/**
 * 보드 초기화 함수
 * 
 * 게임 보드를 초기 상태로 설정합니다:
 * - 모든 셀을 빈 칸으로 초기화
 * - 초기 말 위치 설정 (코너에 각 플레이어 말 배치)
 * - 게임 상태 초기화 (현재 플레이어, 패스 횟수 등)
 * 
 * @param board 초기화할 게임 보드 구조체 포인터
 */
void initializeBoard(GameBoard *board) {
    // 모든 셀을 빈 공간으로 초기화
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->cells[i][j] = EMPTY_CELL;
        }
        board->cells[i][BOARD_SIZE] = '\0'; // 문자열 종료 (JSON 변환용)
    }
    
    // 초기 위치에 말 배치 (각 코너에 배치)
    board->cells[0][0] = RED_PLAYER;  // 왼쪽 상단
    board->cells[7][7] = RED_PLAYER;  // 오른쪽 하단
    board->cells[0][7] = BLUE_PLAYER; // 오른쪽 상단
    board->cells[7][0] = BLUE_PLAYER; // 왼쪽 하단
    
    // 초기 상태 설정
    board->currentPlayer = RED_PLAYER; // 빨간색 플레이어가 먼저 시작
    board->consecutivePasses = 0;      // 연속 패스 횟수 초기화
    
    // 말 개수 계산
    countPieces(board);
}

// 보드 상태 출력 함수
void printBoard(const GameBoard *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%.*s\n", BOARD_SIZE, board->cells[i]);
    }
}

// 보드에서 말의 개수 계산
void countPieces(GameBoard *board) {
    board->redCount = 0;
    board->blueCount = 0;
    board->emptyCount = 0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board->cells[i][j] == RED_PLAYER) {
                board->redCount++;
            } else if (board->cells[i][j] == BLUE_PLAYER) {
                board->blueCount++;
            } else if (board->cells[i][j] == EMPTY_CELL) {
                board->emptyCount++;
            }
        }
    }
}

// 빈 칸의 개수 계산
int countEmpty(const GameBoard *board) {
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board->cells[i][j] == EMPTY_CELL) {
                count++;
            }
        }
    }
    return count;
}

// 유효한 이동이 있는지 확인
int hasValidMove(const GameBoard *board, char player) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->cells[r][c] != player) continue;
            
            for (int d = 0; d < 8; d++) {
                for (int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    
                    // 보드 범위 검사
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    
                    // 2칸 이동 시 중간 칸 검사
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || board->cells[mr][mc] == BLUE_PLAYER) continue;
                    }
                    
                    // 목적지가 빈 칸인지 검사
                    if (board->cells[nr][nc] == EMPTY_CELL) {
                        return 1; // 유효한 이동이 있음
                    }
                }
            }
        }
    }
    return 0; // 유효한 이동이 없음
}

/**
 * 유효한 이동인지 확인하는 함수
 * 
 * 주어진 이동이 게임 규칙에 따라 유효한지 검사합니다:
 * - 패스 검사: (0,0)->(0,0) 이동은 패스로 간주하며, 유효한 이동이 없을 때만 가능
 * - 보드 범위 검사: 시작점과 도착점이 보드 내에 있어야 함
 * - 소유권 검사: 시작점에 현재 플레이어의 말이 있어야 함
 * - 목적지 검사: 도착점이 빈 칸이어야 함
 * - 이동 방향 및 거리 검사: 상하좌우 또는 대각선으로 1칸 또는 2칸만 이동 가능
 * - 중간 칸 검사: 2칸 이동 시 중간 칸이 비어있어야 함
 * 
 * @param board 현재 게임 보드
 * @param move 검사할 이동 정보
 * @return 유효한 이동이면 1, 그렇지 않으면 0
 */
int isValidMove(const GameBoard *board, Move *move) {
    int r1 = move->sourceRow;
    int c1 = move->sourceCol;
    int r2 = move->targetRow;
    int c2 = move->targetCol;
    char current = move->player;
    
    // 패스 검사 (모든 좌표가 0인 경우)
    if (r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0) {
        // 유효한 이동이 있는지 확인 - 있으면 패스 불가
        return !hasValidMove(board, current);
    }
    
    // 범위 검사
    if (r1 < 0 || r1 >= BOARD_SIZE || c1 < 0 || c1 >= BOARD_SIZE ||
        r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE)
        return 0;
    
    // 출발 위치에 현재 플레이어의 말이 있는지 검사
    if (board->cells[r1][c1] != current)
        return 0;
    
    // 목적지가 빈 칸인지 검사
    if (board->cells[r2][c2] != EMPTY_CELL)
        return 0;
    
    // 이동 방향 및 거리 계산
    int dr = r2 - r1;
    int dc = c2 - c1;
    int absDr = absVal(dr);
    int absDc = absVal(dc);
    int maxD = (absDr > absDc) ? absDr : absDc;
    
    // 이동 거리 검사 (1칸 또는 2칸만 허용)
    if (maxD != 1 && maxD != 2)
        return 0;
    
    // 대각선 이동 검사 (대각선 이동시 행과 열의 이동 거리가 같아야 함)
    if (absDr != 0 && absDc != 0 && absDr != absDc)
        return 0;
    
    // 2칸 이동 시 중간 칸 검사 (중간 칸에 말이 없어야 함)
    if (maxD == 2) {
        int mr = r1 + dr / 2;
        int mc = c1 + dc / 2;
        if (board->cells[mr][mc] == RED_PLAYER || board->cells[mr][mc] == BLUE_PLAYER)
            return 0;
    }
    
    return 1; // 모든 조건을 만족하면 유효한 이동
}

/**
 * 이동 적용 함수
 * 
 * 유효한 이동을 게임 보드에 적용합니다:
 * - 말 이동 처리: 1칸 이동은 말을 남기고, 2칸 이동은 말을 제거
 * - 말 뒤집기: 이동 후 인접한 상대 말을 현재 플레이어의 말로 변경
 * - 말 개수 업데이트: 보드 상태를 반영하여 각 플레이어 말 개수 갱신
 * 
 * @param board 이동을 적용할 게임 보드
 * @param move 적용할 이동 정보
 */
void applyMove(GameBoard *board, Move *move) {
    int r1 = move->sourceRow;
    int c1 = move->sourceCol;
    int r2 = move->targetRow;
    int c2 = move->targetCol;
    char current = move->player;
    char opponent = (current == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    
    // 이동 방향 및 거리 계산
    int dr = r2 - r1;
    int dc = c2 - c1;
    int absDr = absVal(dr);
    int absDc = absVal(dc);
    int maxD = (absDr > absDc) ? absDr : absDc;
    
    // 2칸 이동인 경우 출발 위치의 말 제거 (말이 뛰어넘는 경우)
    if (maxD == 2) {
        board->cells[r1][c1] = EMPTY_CELL;
    }
    // 1칸 이동인 경우 출발 위치의 말은 남겨둠 (말이 복제되는 효과)
    
    // 목적지에 말 배치
    board->cells[r2][c2] = current;
    
    // 인접한 상대 말 뒤집기 (8방향 확인)
    for (int d = 0; d < 8; d++) {
        int nr = r2 + dRow[d];
        int nc = c2 + dCol[d];
        
        // 보드 범위 검사
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
            continue;
        
        // 인접한 상대 말을 현재 플레이어의 말로 변경 (뒤집기)
        if (board->cells[nr][nc] == opponent) {
            board->cells[nr][nc] = current;
        }
    }
    
    // 말 개수 업데이트
    countPieces(board);
}

// 게임 결과 출력 함수
void printResult(const GameBoard *board) {
    if (board->redCount > board->blueCount) {
        printf("Red\n");
    } else if (board->blueCount > board->redCount) {
        printf("Blue\n");
    } else {
        printf("Draw\n");
    }
}

// 보드 문자열 변환 함수 (JSON용)
char** boardToStringArray(const GameBoard *board) {
    char **boardArray = (char**)malloc(BOARD_SIZE * sizeof(char*));
    if (boardArray == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        boardArray[i] = (char*)malloc((BOARD_SIZE + 1) * sizeof(char));
        if (boardArray[i] == NULL) {
            // 이전에 할당된 메모리 해제
            for (int j = 0; j < i; j++) {
                free(boardArray[j]);
            }
            free(boardArray);
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        // 문자열 복사 (항상 null 종료 보장)
        strncpy(boardArray[i], board->cells[i], BOARD_SIZE);
        boardArray[i][BOARD_SIZE] = '\0';
    }
    
    return boardArray;
}

// 보드 문자열 배열 메모리 해제 함수
void freeBoardStringArray(char **boardArray) {
    if (boardArray == NULL) return;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (boardArray[i] != NULL) {
            free(boardArray[i]);
        }
    }
    free(boardArray);
}

// 문자열 배열에서 보드로 변환 (JSON용)
void stringArrayToBoard(GameBoard *board, char **boardArray) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        strncpy(board->cells[i], boardArray[i], BOARD_SIZE + 1);
    }
    
    // 말 개수 업데이트
    countPieces(board);
}

// 게임 종료 여부 확인
int hasGameEnded(const GameBoard *board) {
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

// 자동 이동 생성 함수 (간단한 구현)
Move generateMove(const GameBoard *board) {
    Move move;
    move.player = board->currentPlayer;
    
    // 유효한 이동을 찾아 반환
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->cells[r][c] != move.player) continue;
            
            for (int d = 0; d < 8; d++) {
                for (int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    
                    // 보드 범위 검사
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    
                    // 2칸 이동 시 중간 칸 검사
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || board->cells[mr][mc] == BLUE_PLAYER) continue;
                    }
                    
                    // 목적지가 빈 칸인지 검사
                    if (board->cells[nr][nc] == EMPTY_CELL) {
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
    
    // 유효한 이동이 없으면 패스 (0,0,0,0)
    move.sourceRow = move.sourceCol = move.targetRow = move.targetCol = 0;
    return move;
}