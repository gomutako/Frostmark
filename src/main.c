/* ============================================================================
 * main.c - Punto di ingresso: finestra, ciclo principale, chiusura pulita.
 *
 *   ./frostmark              -> mondo con il seme predefinito
 *   ./frostmark 12345        -> mondo generato dal seme 12345
 * ========================================================================== */
#include "raylib.h"
#include "game.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    unsigned int seed = 20260819u;
    if (argc > 1) seed = (unsigned int)strtoul(argv[1], NULL, 10);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, GAME_NAME " " GAME_VERSION);
    SetExitKey(KEY_NULL);          /* ESC serve al menu, non per uscire */
    SetTargetFPS(60);

    static Game game;              /* statico: la struttura e' grande        */
    GameInit(&game, seed);

    /* Passo fisso: la simulazione avanza sempre di SIM_STEP, indipendentemente
     * dal frame rate. Serve alla fisica, che con un dt variabile cambia
     * comportamento, e rende il gioco riproducibile. L'input si legge una volta
     * per fotogramma, la simulazione puo' girare piu' volte per recuperare. */
    double accumulator = 0.0;

    while (!WindowShouldClose() && game.running) {
        double frame = GetFrameTime();
        if (frame > SIM_MAX_FRAME) frame = SIM_MAX_FRAME;  /* dopo un freeze,
                                                              non si recupera
                                                              all'infinito */
        accumulator += frame;

        GameInput(&game);

        int steps = 0;
        while (accumulator >= SIM_STEP && steps < SIM_MAX_STEPS) {
            GameSimulate(&game, (float)SIM_STEP);
            accumulator -= SIM_STEP;
            steps++;
        }
        if (steps == SIM_MAX_STEPS) accumulator = 0.0;      /* rinuncia al resto */

        BeginDrawing();
            ClearBackground(BLACK);
            GameDraw(&game);
        EndDrawing();
    }

    GameShutdown(&game);
    CloseWindow();
    return 0;
}
