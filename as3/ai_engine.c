#include "ai_engine.h"
#include "winning_strategy.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#if defined(__aarch64__)
// WARNING: The aarch64 assembly version of evaluateBoard below uses a simpler
// evaluation based only on positional weights. It does NOT correctly handle
// BLOCKED_CELLs (it may misinterpret them depending on its logic focused only on
// player/opponent pieces vs empty for its positional calculation) and does not
// include piece count, full mobility (blocked cell aware), stability (blocked cell
// aware), or game phase specific evaluation. For improved AI, this should
// ideally be updated to match the logic in the C versions, or a C fallback
// should be used.
// ------------------------------
// 취급주의 어셈블리 건들이지말것
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

int evaluateBoard(const GameBoard *board, char player, int game_phase) {
    int player_score = 0;
    int opponent_score = 0;
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;

    for (int r = 0; r < BOARD_SIZE; r++) {
        const char *board_row_ptr = (const char *)&board->cells[r][0];
        const short *weights_row_ptr = (const short *)&POSITION_WEIGHTS[r][0];

        asm volatile (
            "ld1    {v0.8b}, [%[board_ptr]]\n"        
            "ld1    {v1.8h}, [%[weights_ptr]]\n"       
            "dup    v10.8b, %w[p_id_asm]\n"          
            "dup    v11.8b, %w[o_id_asm]\n"
            "cmeq   v2.8b, v0.8b, v10.8b\n"
            "cmeq   v3.8b, v0.8b, v11.8b\n"
            "sxtl   v4.8h, v2.8b\n"
            "sxtl   v7.8h, v3.8b\n"
            "and    v6.16b, v1.16b, v4.16b\n"
            "and    v8.16b, v1.16b, v7.16b\n"
            "uaddlv s12, v6.8h\n"
            "uaddlv s13, v8.8h\n"
            "fmov   w10, s12\n"
            "fmov   w11, s13\n"
            "add    %w[p_score_asm], %w[p_score_asm], w10\n"
            "add    %w[o_score_asm], %w[o_score_asm], w11\n"
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
// 진짜경고 의도와 다르거나 버그나면 작성자 호출바람
// ------------------------------
#include <stdint.h>


bool isCorner(int r, int c) {
    return (r == 0 || r == BOARD_SIZE - 1) && (c == 0 || c == BOARD_SIZE - 1);
}


int evaluateBoard(const GameBoard *board, char player, int game_phase) {
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    int positional_player_score = 0;
    int positional_opponent_score = 0;

    int current_piece_weight;
    int current_mobility_weight;
    int current_stability_weight;
    int current_positional_factor;

    if (game_phase == PHASE_EARLY) {
        current_piece_weight = PIECE_COUNT_WEIGHT_EARLY;
        current_mobility_weight = MOBILITY_WEIGHT_EARLY;
        current_stability_weight = STABILITY_WEIGHT_EARLY;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_EARLY;
    } else if (game_phase == PHASE_MID) {
        current_piece_weight = PIECE_COUNT_WEIGHT_MID;
        current_mobility_weight = MOBILITY_WEIGHT_MID;
        current_stability_weight = STABILITY_WEIGHT_MID;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_MID;
    } else { // PHASE_LATE
        current_piece_weight = PIECE_COUNT_WEIGHT_LATE;
        current_mobility_weight = MOBILITY_WEIGHT_LATE;
        current_stability_weight = STABILITY_WEIGHT_LATE;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_LATE;
    }

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char cell = board->cells[r][c];
            short weight = POSITION_WEIGHTS[r][c];
            if (cell == player) {
                positional_player_score += weight;
            } else if (cell == opponent) {
                positional_opponent_score += weight;
            }
        }
    }
    int positional_value = (positional_player_score - positional_opponent_score) * current_positional_factor;

    int my_pieces = (player == RED_PLAYER) ? board->redCount : board->blueCount;
    int opp_pieces = (player == RED_PLAYER) ? board->blueCount : board->redCount;
    int piece_diff_score = (my_pieces - opp_pieces) * current_piece_weight;

    int my_mobility = getMobility(board, player);
    int opponent_mobility = getMobility(board, opponent);
    int mobility_score = (my_mobility - opponent_mobility) * current_mobility_weight;

    int stability_score = getStability(board, player) * current_stability_weight; // Only player's stability considered for now

    return positional_value + piece_diff_score + mobility_score + stability_score;
}

#else
// ------------------------------
// 기타 플랫폼 (순수 C 버전)
// ------------------------------
bool isCorner(int r, int c) {
    return (r == 0 || r == BOARD_SIZE - 1) && (c == 0 || c == BOARD_SIZE - 1);
}

int evaluateBoard(const GameBoard *board, char player, int game_phase) {
    char opponent = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER;
    int positional_player_score = 0;
    int positional_opponent_score = 0;

    int current_piece_weight;
    int current_mobility_weight;
    int current_stability_weight;
    int current_positional_factor;

    if (game_phase == PHASE_EARLY) {
        current_piece_weight = PIECE_COUNT_WEIGHT_EARLY;
        current_mobility_weight = MOBILITY_WEIGHT_EARLY;
        current_stability_weight = STABILITY_WEIGHT_EARLY;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_EARLY;
    } else if (game_phase == PHASE_MID) {
        current_piece_weight = PIECE_COUNT_WEIGHT_MID;
        current_mobility_weight = MOBILITY_WEIGHT_MID;
        current_stability_weight = STABILITY_WEIGHT_MID;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_MID;
    } else { // PHASE_LATE
        current_piece_weight = PIECE_COUNT_WEIGHT_LATE;
        current_mobility_weight = MOBILITY_WEIGHT_LATE;
        current_stability_weight = STABILITY_WEIGHT_LATE;
        current_positional_factor = POSITIONAL_WEIGHT_FACTOR_LATE;
    }

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char cell = board->cells[r][c];
            short weight = POSITION_WEIGHTS[r][c];
            if (cell == player) {
                positional_player_score += weight;
            } else if (cell == opponent) {
                positional_opponent_score += weight;
            }
        }
    }
    int positional_value = (positional_player_score - positional_opponent_score) * current_positional_factor;

    int my_pieces = (player == RED_PLAYER) ? board->redCount : board->blueCount;
    int opp_pieces = (player == RED_PLAYER) ? board->blueCount : board->redCount;
    int piece_diff_score = (my_pieces - opp_pieces) * current_piece_weight;

    int my_mobility = getMobility(board, player);
    int opponent_mobility = getMobility(board, opponent);
    int mobility_score = (my_mobility - opponent_mobility) * current_mobility_weight;

    int stability_score = getStability(board, player) * current_stability_weight; // Only player's stability considered for now

    return positional_value + piece_diff_score + mobility_score + stability_score;
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

static unsigned long long zobrist_table[BOARD_SIZE][BOARD_SIZE][3];
static int zobrist_initialized = 0;

void initZobrist(void) {
    if (zobrist_initialized) goto END_INIT;
    srand(12345);
    {
        int i = 0;
I_CHECK:
        if (i >= BOARD_SIZE) goto DONE_I;
        {
            int j = 0;
J_CHECK:
            if (j >= BOARD_SIZE) goto NEXT_I;
            {
                int k = 0;
K_CHECK:
                if (k >= 3) goto NEXT_J;
                zobrist_table[i][j][k] = (((unsigned long long)rand() << 32) | rand());
                k++;
                goto K_CHECK;
NEXT_J:
                ;
            }
            j++;
            goto J_CHECK;
NEXT_I:
            ;
        }
        i++;
        goto I_CHECK;
DONE_I:
        zobrist_initialized = 1;
    }
END_INIT:
    return;
}

AIEngine *createAIEngine(void) {
    AIEngine *engine = NULL;
    goto ALLOC_ENGINE;

ALLOC_ENGINE:
    engine = (AIEngine *)malloc(sizeof(AIEngine));
    if (!engine) goto FAIL;
    engine->transposition_table = NULL;
    engine->nodes_searched = 0;
    engine->time_limit_exceeded = 0;
    goto ALLOC_TT;

ALLOC_TT:
    engine->transposition_table = (TTEntry *)calloc(TRANSPOSITION_TABLE_SIZE, sizeof(TTEntry));
    if (!engine->transposition_table) goto FREE_ENGINE;
    initZobrist();
    goto SUCCESS;

FREE_ENGINE:
    free(engine);
    engine = NULL;
    goto FAIL;

SUCCESS:
    return engine;

FAIL:
    return NULL;
}

void destroyAIEngine(AIEngine *engine) {
    if (!engine) goto END_DESTROY;

    if (engine->transposition_table) free(engine->transposition_table);
    goto FREE_ENGINE;

FREE_ENGINE:
    free(engine);
END_DESTROY:
    return;
}

int isTimeUp(AIEngine *engine) {
    if (engine->time_limit_exceeded) goto TIMEUP;
    {
        clock_t now = clock();
        double elapsed = ((double)(now - engine->start_time)) / CLOCKS_PER_SEC;
        if (elapsed >= TIME_LIMIT) {
            engine->time_limit_exceeded = 1;
            goto TIMEUP;
        }
    }
    return 0;
TIMEUP:
    return 1;
}

unsigned long long calculateHash(const GameBoard *board) {
    unsigned long long hash = 0ULL;
    int r = 0;
R_CHECK:
    if (r >= BOARD_SIZE) goto HASH_DONE;
    {
        int c = 0;
C_CHECK:
        if (c >= BOARD_SIZE) goto NEXT_R;
        {
            int piece_type = 0;
            if (board->cells[r][c] == RED_PLAYER) piece_type = 1;
            else if (board->cells[r][c] == BLUE_PLAYER) piece_type = 2;
            hash ^= zobrist_table[r][c][piece_type];
        }
        c++;
        goto C_CHECK;
NEXT_R:
        ;
    }
    r++;
    goto R_CHECK;
HASH_DONE:
    return hash;
}

void storeInTT(AIEngine *engine, unsigned long long hash, int depth, int value, Move move, char flag) {
    int index = hash % TRANSPOSITION_TABLE_SIZE;
    TTEntry *entry = &engine->transposition_table[index];
    if (entry->hash == 0ULL) goto WRITE;
    if (entry->depth > depth) goto END_STORE;
WRITE:
    entry->hash = hash;
    entry->depth = depth;
    entry->value = value;
    entry->best_move = move;
    entry->flag = flag;
END_STORE:
    return;
}

TTEntry *lookupTT(AIEngine *engine, unsigned long long hash) {
    TTEntry *entry = &engine->transposition_table[hash % TRANSPOSITION_TABLE_SIZE];
    if (entry->hash == hash) goto FOUND;
    return NULL;
FOUND:
    return entry;
}

int isEdge(int row, int col) {
    return (row == 0 || row == BOARD_SIZE - 1 || col == 0 || col == BOARD_SIZE - 1);
}

int getMobility(const GameBoard *board, char player) {
    int mobility = 0;
    int r = 0;
R_CHECK_MOB:
    if (r >= BOARD_SIZE) goto MOB_DONE;
    {
        int c = 0;
C_CHECK_MOB:
        if (c >= BOARD_SIZE) goto NEXT_R_MOB;
        if (board->cells[r][c] != player) goto NEXT_C_MOB;
        {
            int d = 0;
D_CHECK_MOB:
            if (d >= 8) goto END_D_MOB;
            {
                int s = 1;
S_CHECK_MOB:
                if (s > 2) goto NEXT_D_MOB;
                {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) goto INC_S_MOB;
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || board->cells[mr][mc] == BLUE_PLAYER || board->cells[mr][mc] == BLOCKED_CELL) goto INC_S_MOB;
                    }
                    if (board->cells[nr][nc] == EMPTY_CELL) mobility++;
                }
INC_S_MOB:
                s++;
                goto S_CHECK_MOB;
NEXT_D_MOB:
                ;
            }
            d++;
            goto D_CHECK_MOB;
END_D_MOB:
            ;
        }
NEXT_C_MOB:
        c++;
        goto C_CHECK_MOB;
NEXT_R_MOB:
        ;
    }
    r++;
    goto R_CHECK_MOB;
MOB_DONE:
    return mobility;
}

int getStability(const GameBoard *board, char player) {
    int stability = 0;
    int r = 0;
R_CHECK_STAB:
    if (r >= BOARD_SIZE) goto STAB_DONE;
    {
        int c = 0;
C_CHECK_STAB:
        if (c >= BOARD_SIZE) goto NEXT_R_STAB;
        if (board->cells[r][c] != player) goto NEXT_C_STAB;
        if (isCorner(r, c)) { stability += 50; goto NEXT_C_STAB; }
        if (isEdge(r, c)) { stability += 20; goto NEXT_C_STAB; }
        {
            int surrounded = 1;
            int d = 0;
D_CHECK_STAB:
            if (d >= 8) goto AFTER_SURROUND;
            {
                int nr = r + dRow[d];
                int nc = c + dCol[d];
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                    if (board->cells[nr][nc] == EMPTY_CELL) { surrounded = 0; goto AFTER_SURROUND; }
                }
            }
            d++;
            goto D_CHECK_STAB;
AFTER_SURROUND:
            if (surrounded) stability += 10;
        }
NEXT_C_STAB:
        c++;
        goto C_CHECK_STAB;
NEXT_R_STAB:
        ;
    }
    r++;
    goto R_CHECK_STAB;
STAB_DONE:
    return stability;
}

void getAllValidMoves(const GameBoard *board, char player, Move *moves, int *count) {
    *count = 0;
    int r = 0;
R_CHECK_VM:
    if (r >= BOARD_SIZE) goto VM_DONE;
    {
        int c = 0;
C_CHECK_VM:
        if (c >= BOARD_SIZE) goto NEXT_R_VM;
        if (board->cells[r][c] != player) goto NEXT_C_VM;
        {
            int d = 0;
D_CHECK_VM:
            if (d >= 8) goto END_D_VM;
            {
                int s = 1;
S_CHECK_VM:
                if (s > 2) goto NEXT_D_VM;
                {
                    int nr = r + dRow[d] * s;
                    int nc = c + dCol[d] * s;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) goto INC_S_VM;
                    if (s == 2) {
                        int mr = r + dRow[d];
                        int mc = c + dCol[d];
                        if (board->cells[mr][mc] == RED_PLAYER || board->cells[mr][mc] == BLUE_PLAYER || board->cells[mr][mc] == BLOCKED_CELL) goto INC_S_VM;
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
INC_S_VM:
                s++;
                goto S_CHECK_VM;
NEXT_D_VM:
                ;
            }
            d++;
            goto D_CHECK_VM;
END_D_VM:
            ;
        }
NEXT_C_VM:
        c++;
        goto C_CHECK_VM;
NEXT_R_VM:
        ;
    }
    r++;
    goto R_CHECK_VM;
VM_DONE:
    return;
}

// Minimax with Alpha-Beta Pruning
int minimax(AIEngine *engine, GameBoard *board, int depth, int alpha, int beta, 
           char maximizing_player, char original_player, int game_phase) {
    
    engine->nodes_searched++;
    
    // 시간 초과 확인
    if (isTimeUp(engine)) {
        return evaluateBoard(board, original_player, game_phase);
    }
    
    // 터미널 노드 또는 최대 깊이 도달
    if (depth == 0 || hasGameEnded(board)) {
        return evaluateBoard(board, original_player, game_phase);
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
            return evaluateBoard(board, original_player, game_phase);
        }
        
        // 상대 턴으로 넘어감
        return minimax(engine, board, depth - 1, alpha, beta, opponent, original_player, game_phase);
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
            int eval = minimax(engine, &temp_board, depth - 1, alpha, beta, next_player, original_player, game_phase);
            
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
            int eval = minimax(engine, &temp_board, depth - 1, alpha, beta, next_player, original_player, game_phase);
            
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
        
        Move current_best = { 0 };  
        int current_best_value = NEG_INFINITY_VAL;
        
        int current_game_phase = get_game_phase(&temp_board);

        // Try to prioritize a killer move
        Move killer_m = findKillerMove(&temp_board, player); // findKillerMove is from winning_strategy.h
        if (isValidMove(&temp_board, &killer_m)) { // Check if a valid killer move was found
            // Search for the killer move in the general moves list and bring it to the front
            for (int k_idx = 0; k_idx < move_count; k_idx++) {
                if (moves[k_idx].sourceRow == killer_m.sourceRow &&
                    moves[k_idx].sourceCol == killer_m.sourceCol &&
                    moves[k_idx].targetRow == killer_m.targetRow &&
                    moves[k_idx].targetCol == killer_m.targetCol &&
                    moves[k_idx].player == killer_m.player) {
                    
                    // Swap killer_move to the front (moves[0])
                    if (k_idx > 0) {
                        Move temp_move_for_swap = moves[0];
                        moves[0] = moves[k_idx];
                        moves[k_idx] = temp_move_for_swap;
                        printf("Killer move prioritized: (%d,%d) to (%d,%d)\n", killer_m.sourceRow, killer_m.sourceCol, killer_m.targetRow, killer_m.targetCol);
                    }
                    break; // Found and swapped (or already at front)
                }
            }
        }
        
        for (int i = 0; i < move_count; i++) {
            if (isTimeUp(engine)) break;
            
            GameBoard move_board;
            memcpy(&move_board, &temp_board, sizeof(GameBoard));
            applyMove(&move_board, &moves[i]);
            
            char opponent_player = (player == RED_PLAYER) ? BLUE_PLAYER : RED_PLAYER; // Renamed to avoid conflict
            int value = minimax(engine, &move_board, depth - 1, NEG_INFINITY_VAL, INFINITY_VAL, 
                              opponent_player, player, current_game_phase);
            
            if (value > current_best_value) {
                current_best_value = value;
                current_best = moves[i];
            }
        }
        
        if (!isTimeUp(engine) && current_best_value > best_value) {
            best_value = current_best_value;
            best_move = current_best;
        }
    }
    return best_move;
}

// 승리 보장 이동 생성 (메인 함수)
Move generateWinningMove(const GameBoard *board, char player) {
    printf("=== 강력한 AI 엔진 시작 ===\n");

    // 오프닝 북 확인
    Move opening_move = checkOpeningBook(board, player);
    if (isValidMove(board, &opening_move)) {
        printf("오프닝 북 이동 사용!\n");
        return opening_move;
    }

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

