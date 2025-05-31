#include "ai_engine.h"
#include "winning_strategy.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#if defined(__aarch64__)
// ------------------------------
// AArch64(ARMv8) NEON 최적화 버전
// ------------------------------
bool isCorner(int r, int c) {
    bool result;
    register int board_max asm("w3") = BOARD_SIZE - 1;
    asm volatile (
        "cmp    %w[r], wzr\n"
        "cset   %w[t1], eq\n"
        "cmp    %w[r], %w[max]\n"
        "cset   %w[t2], eq\n"
        "orr    %w[t1], %w[t1], %w[t2]\n"
        "cmp    %w[c], wzr\n"
        "cset   %w[t3], eq\n"
        "cmp    %w[c], %w[max]\n"
        "cset   %w[t4], eq\n"
        "orr    %w[t3], %w[t3], %w[t4]\n"
        "and    %w[out], %w[t1], %w[t3]\n"
        : [out] "=r" (result)
        : [r] "r" (r), [c] "r" (c), [max] "r" (board_max),
          [t1] "r" (0), [t2] "r" (0), [t3] "r" (0), [t4] "r" (0)
        : "cc"
    );
    return result;
}

int evaluateBoard(const GameBoard *board, char player) {
    int player_score = 0;
    int opponent_score = 0;
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;

    for (int r = 0; r < BOARD_SIZE; r++) {
        const char *board_row_ptr = (const char *)&board->cells[r][0];
        const short *weights_row_ptr = (const short *)&POSITION_WEIGHTS[r][0];

        asm volatile (
            "ld1    {v0.8b}, [%[board_ptr]]\n"         // 보드 행에서 8 바이트 로드
            "ld1    {v1.8h}, [%[weights_ptr]]\n"       // 가중치에서 8 쇼트(short) 로드
            "dup    v10.8b, %w[p_id_asm]\n"            // player_id (바이트)를 v10으로 복제
            "dup    v11.8b, %w[o_id_asm]\n"            // opponent_id (바이트)를 v11으로 복제
            "cmeq   v2.8b, v0.8b, v10.8b\n"            // v2[i] = (board[i] == player_id) ? 0xFF : 0x00
            "cmeq   v3.8b, v0.8b, v11.8b\n"            // v3[i] = (board[i] == opponent_id) ? 0xFF : 0x00
            "sxtl   v4.8h, v2.8b\n"                    // v2 (바이트)를 v4 (쇼트)로 부호 확장
            "sxtl   v7.8h, v3.8b\n"                    // v3 (바이트)를 v7 (쇼트)로 부호 확장
            "and    v6.16b, v1.16b, v4.16b\n"           // v4[i]가 0xFFFF이면 v6[i]=v1[i], 아니면 0
            "and    v8.16b, v1.16b, v7.16b\n"           // v7[i]가 0xFFFF이면 v8[i]=v1[i], 아니면 0
            "uaddlv s12, v6.8h\n"                     // v6의 요소 합산 → s12
            "uaddlv s13, v8.8h\n"                     // v8의 요소 합산 → s13
            "fmov   w10, s12\n"                        // NEON 스칼라 s12를 GPR w10으로 이동
            "fmov   w11, s13\n"                        // NEON 스칼라 s13를 GPR w11으로 이동
            "add    %w[p_score_asm], %w[p_score_asm], w10\n" // 플레이어 점수 누적
            "add    %w[o_score_asm], %w[o_score_asm], w11\n" // 상대방 점수 누적
            : [p_score_asm] "+r" (player_score),
              [o_score_asm] "+r" (opponent_score)
            : [board_ptr] "r" (board_row_ptr),
              [weights_ptr] "r" (weights_row_ptr),
              [p_id_asm] "r" (player),
              [o_id_asm] "r" (opponent)
            : "v0", "v1", "v2", "v3", "v4", "v6", "v7", "v8",
              "v10", "v11", "s12", "s13",
              "w10", "w11",
              "cc", "memory"
        );
    }
    return player_score - opponent_score;
}

#elif defined(__x86_64__)
// ------------------------------
// x86_64 환경: 순수 C(또는 필요 시 SSE/AVX) 버전
// ------------------------------
#include <stdint.h>

// 코너 판별 (이미 AArch64 이외 환경에서도 정의되어 있음)
bool isCorner(int r, int c) {
    return (r == 0 || r == BOARD_SIZE - 1) && (c == 0 || c == BOARD_SIZE - 1);
}

// evaluateBoard_fallback: 순수 C 로직
int evaluateBoard(const GameBoard *board, char player) {
    int player_score = 0;
    int opponent_score = 0;
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char cell = board->cells[r][c];
            short weight = POSITION_WEIGHTS[r][c];
            if (cell == player) {
                player_score += weight;
            } else if (cell == opponent) {
                opponent_score += weight;
            }
        }
    }
    return player_score - opponent_score;
}

#else
// ------------------------------
// 기타 플랫폼 (순수 C 버전)
// ------------------------------
bool isCorner(int r, int c) {
    return (r == 0 || r == BOARD_SIZE - 1) && (c == 0 || c == BOARD_SIZE - 1);
}

int evaluateBoard(const GameBoard *board, char player) {
    int player_score = 0;
    int opponent_score = 0;
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char cell = board->cells[r][c];
            short weight = POSITION_WEIGHTS[r][c];
            if (cell == player) {
                player_score += weight;
            } else if (cell == opponent) {
                opponent_score += weight;
            }
        }
    }
    return player_score - opponent_score;
}
#endif

short POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    {100, -20, 20, 5, 5, 20, -20, 100},
    {-20, -40, -5, -5, -5, -5, -40, -20},
    {20, -5, 15, 3, 3, 15, -5, 20},
    {5, -5, 3, 3, 3, 3, -5, 5},
    {5, -5, 3, 3, 3, 3, -5, 5},
    {20, -5, 15, 3, 3, 15, -5, 20},
    {-20, -40, -5, -5, -5, -5, -40, -20},
    {100, -20, 20, 5, 5, 20, -20, 100}
};
// Zobrist 해시용 랜덤 테이블
static unsigned long long zobrist_table[BOARD_SIZE][BOARD_SIZE][3];
static int zobrist_initialized = 0;

// Zobrist 해시 테이블 초기화
void initZobrist() {
    if (zobrist_initialized) return;
    
    srand(12345); // 고정 시드로 일관성 보장
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            for (int k = 0; k < 3; k++) {
                zobrist_table[i][j][k] = 
                    ((unsigned long long)rand() << 32) | rand();
            }
        }
    }
    zobrist_initialized = 1;
}

// AI 엔진 생성
AIEngine* createAIEngine() {
    AIEngine *engine = malloc(sizeof(AIEngine));
    if (!engine) return NULL;
    
    engine->transposition_table = calloc(TRANSPOSITION_TABLE_SIZE, sizeof(TTEntry));
    if (!engine->transposition_table) {
        free(engine);
        return NULL;
    }
    
    engine->nodes_searched = 0;
    engine->time_limit_exceeded = 0;
    
    initZobrist();
    
    return engine;
}

// AI 엔진 해제
void destroyAIEngine(AIEngine *engine) {
    if (engine) {
        if (engine->transposition_table) {
            free(engine->transposition_table);
        }
        free(engine);
    }
}

// 시간 초과 확인
int isTimeUp(AIEngine *engine) {
    if (engine->time_limit_exceeded) return 1;
    
    clock_t current = clock();
    double elapsed = ((double)(current - engine->start_time)) / CLOCKS_PER_SEC;
    
    if (elapsed >= TIME_LIMIT) {
        engine->time_limit_exceeded = 1;
        return 1;
    }
    return 0;
}

// 보드 해시 계산
unsigned long long calculateHash(const GameBoard *board) {
    unsigned long long hash = 0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int piece_type = 0;
            if (board->cells[i][j] == RED_PLAYER) piece_type = 1;
            else if (board->cells[i][j] == BLUE_PLAYER) piece_type = 2;
            
            hash ^= zobrist_table[i][j][piece_type];
        }
    }
    
    return hash;
}

// Transposition Table에 저장
void storeInTT(AIEngine *engine, unsigned long long hash, int depth, int value, 
               Move move, char flag) {
    int index = hash % TRANSPOSITION_TABLE_SIZE;
    TTEntry *entry = &engine->transposition_table[index];
    
    // 더 깊은 탐색이거나 새로운 엔트리일 때만 저장
    if (entry->hash == 0 || entry->depth <= depth) {
        entry->hash = hash;
        entry->depth = depth;
        entry->value = value;
        entry->best_move = move;
        entry->flag = flag;
    }
}

// Transposition Table에서 조회
TTEntry* lookupTT(AIEngine *engine, unsigned long long hash) {
    int index = hash % TRANSPOSITION_TABLE_SIZE;
    TTEntry *entry = &engine->transposition_table[index];
    
    if (entry->hash == hash) {
        return entry;
    }
    return NULL;
}

// 코너 위치 확인


// 가장자리 위치 확인
int isEdge(int row, int col) {
    return row == 0 || row == BOARD_SIZE-1 || col == 0 || col == BOARD_SIZE-1;
}

// 이동성(mobility) 계산 - 가능한 이동 수
int getMobility(const GameBoard *board, char player) {
    int mobility = 0;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->cells[r][c] != player) continue;
            
            for (int d = 0; d < 8; d++) {
                for (int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || 
                            board->cells[mr][mc] == BLUE_PLAYER) continue;
                    }
                    
                    if (board->cells[nr][nc] == EMPTY_CELL) {
                        mobility++;
                    }
                }
            }
        }
    }
    
    return mobility;
}

// 안정성(stability) 계산 - 뒤집히기 어려운 말들
int getStability(const GameBoard *board, char player) {
    int stability = 0;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->cells[r][c] != player) continue;
            
            // 코너 말은 절대 안전
            if (isCorner(r, c)) {
                stability += 50;
                continue;
            }
            
            // 가장자리 말은 상대적으로 안전
            if (isEdge(r, c)) {
                stability += 20;
                continue;
            }
            
            // 주변이 모두 채워져 있으면 안전
            int surrounded = 1;
            for (int d = 0; d < 8; d++) {
                int nr = r + dRow[d];
                int nc = c + dCol[d];
                
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                    if (board->cells[nr][nc] == EMPTY_CELL) {
                        surrounded = 0;
                        break;
                    }
                }
            }
            
            if (surrounded) {
                stability += 10;
            }
        }
    }
    
    return stability;
}



// 모든 유효한 이동 생성
void getAllValidMoves(const GameBoard *board, char player, Move *moves, int *count) {
    *count = 0;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->cells[r][c] != player) continue;
            
            for (int d = 0; d < 8; d++) {
                for (int s = 1; s <= 2; s++) {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || 
                            board->cells[mr][mc] == BLUE_PLAYER) continue;
                    }
                    
                    if (board->cells[nr][nc] == EMPTY_CELL) {
                        moves[*count].player = player;
                        moves[*count].sourceRow = r;
                        moves[*count].sourceCol = c;
                        moves[*count].targetRow = nr;
                        moves[*count].targetCol = nc;
                        (*count)++;
                    }
                }
            }
        }
    }
}

// Minimax with Alpha-Beta Pruning
int minimax(AIEngine *engine, GameBoard *board, int depth, int alpha, int beta, 
           char maximizing_player, char original_player) {
    
    engine->nodes_searched++;
    
    // 시간 초과 확인
    if (isTimeUp(engine)) {
        return evaluateBoard(board, original_player);
    }
    
    // 터미널 노드 또는 최대 깊이 도달
    if (depth == 0 || hasGameEnded(board)) {
        return evaluateBoard(board, original_player);
    }
    
    // Transposition Table 조회
    unsigned long long hash = calculateHash(board);
    TTEntry *tt_entry = lookupTT(engine, hash);
    if (tt_entry && tt_entry->depth >= depth) {
        if (tt_entry->flag == 'E') {
            return tt_entry->value;
        } else if (tt_entry->flag == 'L' && tt_entry->value >= beta) {
            return tt_entry->value;
        } else if (tt_entry->flag == 'U' && tt_entry->value <= alpha) {
            return tt_entry->value;
        }
    }
    
    Move moves[256];
    int move_count;
    getAllValidMoves(board, maximizing_player, moves, &move_count);
    
    // 유효한 이동이 없으면 패스
    if (move_count == 0) {
        char opponent = (maximizing_player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
        
        // 상대도 이동할 수 없으면 게임 종료
        Move opp_moves[256];
        int opp_count;
        getAllValidMoves(board, opponent, opp_moves, &opp_count);
        if (opp_count == 0) {
            return evaluateBoard(board, original_player);
        }
        
        // 상대 턴으로 넘어감
        return minimax(engine, board, depth - 1, alpha, beta, opponent, original_player);
    }
    
    Move best_move;
    best_move.player = maximizing_player;
    best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
    char tt_flag = 'U';  // Upper bound
    
    if (maximizing_player == original_player) {
        // Maximizing player
        int max_eval = NEG_INFINITY_VAL;
        
        for (int i = 0; i < move_count; i++) {
            if (isTimeUp(engine)) break;
            
            GameBoard temp_board;
            memcpy(&temp_board, board, sizeof(GameBoard));
            applyMove(&temp_board, &moves[i]);
            
            char next_player = (maximizing_player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
            int eval = minimax(engine, &temp_board, depth - 1, alpha, beta, next_player, original_player);
            
            if (eval > max_eval) {
                max_eval = eval;
                best_move = moves[i];
            }
            
            alpha = (alpha > eval) ? alpha : eval;
            if (beta <= alpha) {
                tt_flag = 'L';  // Lower bound
                break;
            }
        }
        
        if (max_eval == alpha) tt_flag = 'E';  // Exact
        storeInTT(engine, hash, depth, max_eval, best_move, tt_flag);
        return max_eval;
        
    } else {
        // Minimizing player
        int min_eval = INFINITY_VAL;
        
        for (int i = 0; i < move_count; i++) {
            if (isTimeUp(engine)) break;
            
            GameBoard temp_board;
            memcpy(&temp_board, board, sizeof(GameBoard));
            applyMove(&temp_board, &moves[i]);
            
            char next_player = (maximizing_player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
            int eval = minimax(engine, &temp_board, depth - 1, alpha, beta, next_player, original_player);
            
            if (eval < min_eval) {
                min_eval = eval;
                best_move = moves[i];
            }
            
            beta = (beta < eval) ? beta : eval;
            if (beta <= alpha) {
                tt_flag = 'L';  // Lower bound
                break;
            }
        }
        
        if (min_eval == beta) tt_flag = 'E';  // Exact
        storeInTT(engine, hash, depth, min_eval, best_move, tt_flag);
        return min_eval;
    }
}

// 최고의 이동 찾기
Move findBestMove(AIEngine *engine, const GameBoard *board, char player) {
    engine->start_time = clock();
    engine->time_limit_exceeded = 0;
    engine->nodes_searched = 0;
    
    Move best_move;
    best_move.player = player;
    best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
    
    int best_value = NEG_INFINITY_VAL;
    
    // Iterative Deepening
    for (int depth = 1; depth <= MAX_DEPTH; depth++) {
        if (isTimeUp(engine)) break;
        
        GameBoard temp_board;
        memcpy(&temp_board, board, sizeof(GameBoard));
        
        Move moves[256];
        int move_count;
        getAllValidMoves(&temp_board, player, moves, &move_count);
        
        if (move_count == 0) {
            // 패스
            best_move.sourceRow = best_move.sourceCol = best_move.targetRow = best_move.targetCol = 0;
            break;
        }
        
        Move current_best;
        int current_best_value = NEG_INFINITY_VAL;
        
        for (int i = 0; i < move_count; i++) {
            if (isTimeUp(engine)) break;
            
            GameBoard move_board;
            memcpy(&move_board, &temp_board, sizeof(GameBoard));
            applyMove(&move_board, &moves[i]);
            
            char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
            int value = minimax(engine, &move_board, depth - 1, NEG_INFINITY_VAL, INFINITY_VAL, 
                              opponent, player);
            
            if (value > current_best_value) {
                current_best_value = value;
                current_best = moves[i];
            }
        }
        
        if (!isTimeUp(engine) && current_best_value > best_value) {
            best_value = current_best_value;
            best_move = current_best;
        }
        
        printf("Depth %d: Best value = %d, Nodes = %d\n", 
               depth, current_best_value, engine->nodes_searched);
    }
    
    printf("Final: Best value = %d, Total nodes = %d\n", 
           best_value, engine->nodes_searched);
    
    return best_move;
}

// 승리 보장 이동 생성 (메인 함수)
Move generateWinningMove(const GameBoard *board, char player) {
    printf("=== 강력한 AI 엔진 시작 ===\n");
    
    // 종반이면 완전 계산 사용
    if (isEndgamePhase(board)) {
        printf("종반 단계 - 완전 계산 시작...\n");
        Move endgame_move = solveEndgame(board, player);
        printf("=== 종반 완전 해결 ===\n");
        return endgame_move;
    }
    
    // 메인 AI 엔진 사용
    printf("고급 AI 엔진 구동 중...\n");
    AIEngine *engine = createAIEngine();
    if (!engine) {
        printf("AI 엔진 초기화 실패 - 기본 이동 사용\n");
        return generateMove(board);
    }
    
    Move move = findBestMove(engine, board, player);
    destroyAIEngine(engine);
    
    printf("=== AI 엔진 최적해 선택 ===\n");
    return move;
}
