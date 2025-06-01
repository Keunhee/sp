#include "winning_strategy.h"
#include "ai_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 초반 정석 데이터베이스 (강력한 초반 이동들)
OpeningMove opening_book[] = {
    // 초기 위치에서 대각선 이동 (1칸 - 말 복제)
    {
        {
            "R......B",
            "........",
            "........", 
            "........",
            "........",
            "........",
            "........",
            "B......R"
        },
        {RED_PLAYER, 0, 0, 1, 1}, 95
    },
    // 더 공격적인 이동
    {
        {
            "R......B",
            "........",
            "........",
            "........", 
            "........",
            "........",
            "........",
            "B......R"
        },
        {RED_PLAYER, 7, 7, 6, 6}, 90
    },
    // 상대 코너 견제
    {
        {
            "R......B",
            "........",
            "........",
            "........",
            "........", 
            "........",
            "........",
            "B......R"
        },
        {RED_PLAYER, 0, 0, 0, 2}, 85
    }
};

int opening_book_size = sizeof(opening_book) / sizeof(OpeningMove);

// 초반 정석 확인
Move checkOpeningBook(const GameBoard *board, char player) {
    Move invalid_move = {player, 0, 0, 0, 0};
    
    // 초반이 아니면 정석 사용 안함
    if (!isOpeningPhase(board)) {
        return invalid_move;
    }
    
    printf("정석 데이터베이스 검색 중... (크기: %d)\n", opening_book_size);
    
    for (int i = 0; i < opening_book_size; i++) {
        OpeningMove *opening = &opening_book[i];
        
        // 현재 보드 상태 출력 (디버깅용)
        printf("현재 보드:\n");
        for (int r = 0; r < BOARD_SIZE; r++) {
            printf("'%s'\n", board->cells[r]);
        }
        
        printf("정석 %d:\n", i);
        for (int r = 0; r < BOARD_SIZE; r++) {
            printf("'%s'\n", opening->initial_board[r]);
        }
        
        // 보드 상태가 정석과 일치하는지 확인
        int match = 1;
        for (int r = 0; r < BOARD_SIZE && match; r++) {
            if (strcmp(board->cells[r], opening->initial_board[r]) != 0) {
                match = 0;
                printf("행 %d에서 불일치: '%s' vs '%s'\n", r, board->cells[r], opening->initial_board[r]);
            }
        }
        
        if (match && opening->best_move.player == player) {
            printf("정석 이동 발견! 신뢰도: %d%%, 이동: (%d,%d)->(%d,%d)\n", 
                   opening->confidence,
                   opening->best_move.sourceRow, opening->best_move.sourceCol,
                   opening->best_move.targetRow, opening->best_move.targetCol);
            return opening->best_move;
        }
    }
    
    printf("정석에서 매칭되는 이동 없음\n");
    return invalid_move;
}

// 초반 단계 확인
int isOpeningPhase(const GameBoard *board) {
    // 빈 칸이 50개 이상이면 초반
    return board->emptyCount >= 50;
}

// 종반 단계 확인  
int isEndgamePhase(const GameBoard *board) {
    return board->emptyCount <= ENDGAME_THRESHOLD;
}

// 완전 계산 종반 해결
Move solveEndgame(const GameBoard *board, char player) {
    printf("종반 완전 계산 시작 (빈 칸: %d)\n", board->emptyCount);
    
    // 최대 깊이로 완전 계산
    AIEngine *engine = createAIEngine();
    if (!engine) {
        return generateMove(board);
    }
    
    // 종반에서는 더 깊게 탐색
    engine->start_time = clock();
    engine->time_limit_exceeded = 0;
    
    Move best_move;
    best_move.player = player;
    best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
    
    int best_value = NEG_INFINITY_VAL;
    
    // 모든 가능한 이동을 완전 계산
    Move moves[256];
    int move_count;
    getAllValidMoves(board, player, moves, &move_count);
    
    if (move_count == 0) {
        destroyAIEngine(engine);
        return best_move;  // 패스
    }
    
    for (int i = 0; i < move_count; i++) {
        GameBoard temp_board;
        memcpy(&temp_board, board, sizeof(GameBoard));
        applyMove(&temp_board, &moves[i]);
        
        char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
        int value = minimax(engine, &temp_board, board->emptyCount, 
                          NEG_INFINITY_VAL, INFINITY_VAL, opponent, player);
        
        if (value > best_value) {
            best_value = value;
            best_move = moves[i];
        }
        
        printf("이동 (%d,%d)->(%d,%d): 점수 %d\n", 
               moves[i].sourceRow, moves[i].sourceCol,
               moves[i].targetRow, moves[i].targetCol, value);
    }
    
    printf("종반 최적 이동: (%d,%d)->(%d,%d), 점수: %d\n",
           best_move.sourceRow, best_move.sourceCol,
           best_move.targetRow, best_move.targetCol, best_value);
    
    destroyAIEngine(engine);
    return best_move;
}

// 물질적 우위 계산
int calculateMaterial(const GameBoard *board, char player) {
    int my_pieces = (player == RED_PLAYER) ? board->redCount : board->blueCount;
    int opp_pieces = (player == RED_PLAYER) ? board->blueCount : board->redCount;
    return my_pieces - opp_pieces;
}

// 위협 수준 계산
int calculateThreatLevel(const GameBoard *board, char player) {
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    
    int threat_level = 0;
    
    // 상대가 다음 턴에 만들 수 있는 말 수 계산
    Move opp_moves[256];
    int opp_count;
    getAllValidMoves(board, opponent, opp_moves, &opp_count);
    
    for (int i = 0; i < opp_count; i++) {
        GameBoard temp_board;
        memcpy(&temp_board, board, sizeof(GameBoard));
        applyMove(&temp_board, &opp_moves[i]);
        
        int my_pieces_after = (player == RED_PLAYER) ? temp_board.redCount : temp_board.blueCount;
        int my_pieces_before = (player == RED_PLAYER) ? board->redCount : board->blueCount;
        
        int pieces_lost = my_pieces_before - my_pieces_after;
        threat_level += pieces_lost;
    }
    
    return threat_level;
}

// 승부수 찾기 (상대를 크게 압도할 수 있는 이동)
Move findKillerMove(const GameBoard *board, char player) {
    Move killer_move = {player, 0, 0, 0, 0};
    int max_damage = 0;
    
    Move moves[256];
    int move_count;
    getAllValidMoves(board, player, moves, &move_count);
    
    for (int i = 0; i < move_count; i++) {
        GameBoard temp_board;
        memcpy(&temp_board, board, sizeof(GameBoard));
        applyMove(&temp_board, &moves[i]);
        
        // 이동으로 인한 상대 말 감소량 계산
        char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
        int opp_before = (opponent == RED_PLAYER) ? board->redCount : board->blueCount;
        int opp_after = (opponent == RED_PLAYER) ? temp_board.redCount : temp_board.blueCount;
        int damage = opp_before - opp_after;
        
        // 코너 점유 보너스
        if (isCorner(moves[i].targetRow, moves[i].targetCol)) {
            damage += 50;
        }
        
        if (damage > max_damage) {
            max_damage = damage;
            killer_move = moves[i];
        }
    }
    
    if (max_damage >= 3) {  // 상당한 피해를 줄 수 있는 경우
        printf("승부수 발견! 피해량: %d\n", max_damage);
        return killer_move;
    }
    
    return (Move){player, 0, 0, 0, 0};  // 승부수 없음
}

// 승리 확정 위치 확인
int isWinningPosition(const GameBoard *board, char player) {
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    
    // 상대 말이 없으면 승리
    int opp_pieces = (opponent == RED_PLAYER) ? board->redCount : board->blueCount;
    if (opp_pieces == 0) return 1;
    
    // 상대가 이동할 수 없고 내가 더 많은 말을 가지고 있으면 승리
    if (!hasValidMove(board, opponent)) {
        int my_pieces = (player == RED_PLAYER) ? board->redCount : board->blueCount;
        return my_pieces > opp_pieces;
    }
    
    // 보드가 가득 찬 상태에서 더 많은 말을 가지고 있으면 승리
    if (board->emptyCount == 0) {
        int my_pieces = (player == RED_PLAYER) ? board->redCount : board->blueCount;
        return my_pieces > opp_pieces;
    }
    
    return 0;
}