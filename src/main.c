/* ============================================================================
 * main.c - Punto di ingresso: finestra, ciclo principale, chiusura pulita.
 *
 *   ./frostmark              -> gioca il mondo cotto in assets/world/
 *   ./frostmark --valida     -> controlla dati e mondo, elenca i problemi, esce
 *
 * Il seme non e' piu' un argomento: si passa a tools/baker, una volta, per
 * cuocere il mondo (make mondo). Vedi docs/05, fase 3.
 * ========================================================================== */
/* setenv()/access(): con -std=c99 vanno chieste esplicitamente. */
#define _POSIX_C_SOURCE 200809L

#include "raylib.h"
#include "game.h"
#include "world.h"
#include "worldfmt.h"
#include "dataparse.h"
#include "gamedata.h"
#include <stdlib.h>
#include <stdio.h>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <string.h>

/* Sotto WSL Mesa sceglie llvmpipe, cioe' rendering software: misurato, il
 * mondo gira a 23 fps con 2,6 passi di simulazione per fotogramma, e il gioco
 * diventa ingovernabile. Il driver d3d12, che passa per la GPU di Windows, c'e'
 * ma va chiesto: con quello siamo a 58 fps e un passo per fotogramma. Si
 * imposta prima di InitWindow - Mesa legge la variabile quando crea il
 * contesto - e solo se c'e' davvero il dispositivo e l'utente non ha gia'
 * scelto da se'. */
static void PreferGPUOnWSL(void)
{
#if !defined(__linux__)
    return;             /* la variabile riguarda solo Mesa su Linux */
#else
    if (getenv("WSL_DISTRO_NAME") == NULL && getenv("WSL_INTEROP") == NULL) return;
    if (getenv("GALLIUM_DRIVER") != NULL || getenv("LIBGL_ALWAYS_SOFTWARE") != NULL) return;
    if (access("/dev/dxg", F_OK) != 0) return;   /* niente GPU condivisa */
    setenv("GALLIUM_DRIVER", "d3d12", 0);
#endif
}

int main(int argc, char **argv)
{
    /* --valida carica dati e mondo, elenca i problemi ed esce: si usa
     * dall'editor e in integrazione continua, e non apre una finestra. */
    if (argc > 1 && strcmp(argv[1], "--valida") == 0) {
        bool ok = GameDataLoad();
        if (!WorldValidate(WORLD_DIR)) ok = false;
        if (ok) printf("dati e mondo validi.\n");
        else    printf("%d problemi.\n", DataProblemCount());
        return ok ? 0 : 1;
    }
    if (argc > 1) {
        fprintf(stderr, "uso: %s [--valida]\n"
                        "Il seme non e' piu' un argomento del gioco: il mondo si\n"
                        "cuoce una volta con 'make mondo'.\n", argv[0]);
        return 2;
    }

    /* Dati e mondo si controllano PRIMA di aprire la finestra: un mondo rotto
     * non deve far comparire una finestra per poi chiuderla. Costa il doppio
     * caricamento dei 12,6 MB di quote e biomi, cioe' qualche millisecondo di
     * cache del sistema. */
    if (!GameDataLoad()) {
        fprintf(stderr,
                "\nAvvio interrotto: %d problemi nei dati in %s.\n"
                "I dati di gioco non hanno valori di ripiego nel codice.\n"
                "Correggi i file segnalati qui sopra, oppure controllali con:\n"
                "  ./frostmark --valida\n",
                DataProblemCount(), DATA_DIR);
        return 1;
    }
    if (!WorldValidate(WORLD_DIR)) {
        fprintf(stderr,
                "\nAvvio interrotto: %d problemi nel mondo in %s.\n"
                "Il mondo non ha un ripiego procedurale: va cotto una volta con\n"
                "  make mondo\n",
                DataProblemCount(), WORLD_DIR);
        return 1;
    }

    PreferGPUOnWSL();

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, GAME_NAME " " GAME_VERSION);
    SetExitKey(KEY_NULL);          /* ESC serve al menu, non per uscire */
    SetTargetFPS(60);

    static Game game;              /* statico: la struttura e' grande        */
    if (!GameInit(&game)) {
        fprintf(stderr, "Avvio interrotto: %d problemi nel mondo.\n",
                DataProblemCount());
        CloseWindow();
        return 1;
    }

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

        GameUpdateCamera(&game);

        BeginDrawing();
            ClearBackground(BLACK);
            GameDraw(&game);
        EndDrawing();
    }

    GameShutdown(&game);
    CloseWindow();
    return 0;
}
