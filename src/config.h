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

/* Metri di terreno coperti da una ripetizione della texture del terreno:
 * abbassarlo se si innesta una texture fotografica (assets/textures/grass.png)
 * e il risultato appare sfocato. */
#define TERRAIN_UV_TILE 8.0f

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
#define PLAYER_HEIGHT  1.85f            /* altezza del corpo in terza persona */
#define GRAVITY       22.0f

/* --- Camera -------------------------------------------------------------
 * In terza persona la camera orbita dietro la testa. Sotto CAM_DIST_MIN si
 * rientra in prima persona: e' il comportamento della rotellina in Skyrim. */
#define CAM_DIST_MIN   1.8f
#define CAM_DIST_MAX   6.5f
#define CAM_DIST_DEF   3.4f
#define CAM_ZOOM_STEP  0.45f
#define CAM_CLEARANCE  0.45f            /* stacco minimo dal terreno */
#define CAM_SHOULDER   0.45f            /* scostamento a destra: libera il mirino */
#define CAM_RISE       0.35f            /* la camera guarda da sopra la spalla */

/* --- Modello animato del giocatore (opzionale) --------------------------
 * Se il file esiste, in terza persona sostituisce le primitive. La scala
 * porta l'altezza del modello a PLAYER_HEIGHT: il cavaliere KayKit misura
 * 2.31 unita', da cui 1.85 / 2.31 = 0.80. Con un altro pacchetto va rifatta.
 * Vedi docs/03-asset-pubblici.md. */
#define PLAYER_MODEL_FILE  "assets/models/player.glb"
/* Mesh da agganciare alle ossa (arma, scudo, elmo): raylib non le anima da se'.
 * Il file lo genera tools/glb_attachments.py ed e' modificabile a mano. */
#define PLAYER_ATTACH_FILE "assets/models/player.attach"
#define MAX_ATTACHMENTS    8
#define PLAYER_MODEL_SCALE 0.80f
#define PLAYER_MODEL_YAW   0.0f         /* rotazione extra se il modello guarda -Z */

/* --- Parata ------------------------------------------------------------- */
#define BLOCK_SPEED_MUL 0.45f           /* quanto rallenta */
#define BLOCK_DMG_MUL   0.45f           /* danno che passa comunque */
#define BLOCK_STA_DRAIN 5.0f            /* vigore al secondo tenendo la guardia */

/* --- Tempo -------------------------------------------------------------- */
#define DAY_LENGTH_SECONDS 720.0f  /* un giorno di gioco = 12 minuti reali   */

#endif /* CONFIG_H */
