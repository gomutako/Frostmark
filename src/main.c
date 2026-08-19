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

    while (!WindowShouldClose() && game.running) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;       /* evita salti dopo un freeze */

        GameUpdate(&game, dt);

        BeginDrawing();
            ClearBackground(BLACK);
            GameDraw(&game);
        EndDrawing();
    }

    GameShutdown(&game);
    CloseWindow();
    return 0;
}
