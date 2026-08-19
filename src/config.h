/* ============================================================================
 * config.h - Costanti globali del gioco.
 *
 * Tutti i "numeri magici" del progetto vivono qui: cambiando questi valori si
 * modifica il bilanciamento e la scala del mondo senza toccare la logica.
 * ========================================================================== */
#ifndef CONFIG_H
#define CONFIG_H

#define GAME_NAME     "Frostmark"
#define GAME_VERSION  "1.0.0"
#define SAVE_FILE     "frostmark.sav"

/* --- Finestra --------------------------------------------------------- */
#define SCREEN_W 1280
#define SCREEN_H 720

/* --- Geometria del mondo ----------------------------------------------
 * Il mondo e' una griglia di chunk. Ogni chunk e' una mesh di terreno di
 * CHUNK_QUADS x CHUNK_QUADS quadrati, grande CHUNK_SIZE unita' per lato.
 * 1 unita' ~ 1 metro. */
#define CHUNK_QUADS   32
#define CHUNK_VERTS   (CHUNK_QUADS + 1)
#define CHUNK_SIZE    64.0f
#define VERT_STEP     (CHUNK_SIZE / (float)CHUNK_QUADS)

#define WORLD_CHUNKS  64                                   /* 64x64 chunk    */
#define WORLD_SIZE    (WORLD_CHUNKS * CHUNK_SIZE)          /* 4096 x 4096 m  */

#define VIEW_CHUNKS   5                                    /* raggio visivo  */
#define MAX_LOADED_CHUNKS ((2*VIEW_CHUNKS+1)*(2*VIEW_CHUNKS+1))
#define CHUNK_BUILDS_PER_FRAME 3                           /* anti-stutter   */

/* --- Terreno ----------------------------------------------------------- */
#define SEA_LEVEL       14.0f
#define SNOW_LEVEL      78.0f
#define MOUNTAIN_LEVEL  55.0f

/* --- Popolamento -------------------------------------------------------- */
#define MAX_PROPS_PER_CHUNK 160
#define MAX_TOWNS            5
#define TOWN_RADIUS         70.0f

#define MAX_ENTITIES     192
#define MAX_PROJECTILES   64
#define MAX_INVENTORY     40
#define MAX_QUESTS         3

/* --- Giocatore ---------------------------------------------------------- */
#define PLAYER_EYE     1.72f
#define PLAYER_RADIUS  0.45f
#define PLAYER_WALK    5.0f
#define PLAYER_RUN     9.0f
#define PLAYER_JUMP    7.5f
#define GRAVITY       22.0f

/* --- Tempo -------------------------------------------------------------- */
#define DAY_LENGTH_SECONDS 720.0f  /* un giorno di gioco = 12 minuti reali   */

#endif /* CONFIG_H */
