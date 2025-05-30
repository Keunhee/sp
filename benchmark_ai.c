#include <stdio.h>
#include <time.h>
#include "octaflip.h"
#include "ai_engine.h"

int main() {
    GameBoard board;
    initializeBoard(&board);

    AIEngine *engine = createAIEngine();
    clock_t start = clock();
    Move move = generateWinningMove(&board, RED_PLAYER);
    clock_t end = clock();
    destroyAIEngine(engine);

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("실행 시간: %.6f초\n", elapsed);
    printf("Best move: (%d,%d) -> (%d,%d)\n",
           move.sourceRow, move.sourceCol, move.targetRow, move.targetCol);

    return 0;
}
