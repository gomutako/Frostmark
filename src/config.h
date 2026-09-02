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
#define MAX_ITEMS         64          /* capienza del catalogo caricato da file */
#define MAX_SHOP_STOCK    32
#define MAX_ENTITY_TYPES  32          /* tipi caricati da entities.txt */
#define MAX_RUMORS        32
#define MAX_WORLD_NPCS   128          /* NPC previsti da assets/world/spawns.txt */
#define MAX_QUESTS        16          /* caricate da quests.txt */

/* --- Giocatore ---------------------------------------------------------
 * Velocita', gravita', altezze e parata sono dati: stanno in
 * assets/data/balance.txt e si leggono da BAL (balance.h). */

/* Pendenza massima a cui il giocatore resta incollato scendendo: oltre, e'
 * una caduta. Il valore e' un rapporto altezza/distanza, non un angolo. */
#define STEP_DOWN_SLOPE 2.0f

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
#define CAM_DIST_INDOOR 2.2f            /* dentro un edificio si sta stretti */
#define CAM_BUILD_MARGIN 0.28f          /* stacco della camera dai muri */
/* Dissolvenza del personaggio quando la camera gli finisce addosso. */
#define CAM_FADE_OUT   0.45f            /* sotto questa distanza e' invisibile */
#define CAM_FADE_IN    1.25f            /* sopra e' pieno */
#define CAM_RETURN_SPEED 3.5f           /* m/s con cui la camera si riallontana */

/* --- Modelli animati dei personaggi (opzionali) -------------------------
 * Se il file esiste sostituisce le primitive. La scala non si indovina: viene
 * ricavata misurando il modello e portandolo all'altezza voluta.
 * Vedi docs/03-asset-pubblici.md. */
#define PLAYER_MODEL_FILE  "assets/models/player.glb"
#define PLAYER_MODEL_YAW   0.0f         /* rotazione extra se il modello guarda -Z */
/* Mesh da agganciare alle ossa (arma, scudo, elmo): raylib non le anima da se'.
 * L'elenco sta in un file "<modello>.attach" generato da
 * tools/glb_attachments.py, ed e' modificabile a mano. */
#define MAX_ATTACHMENTS    8
/* Oltre questa distanza gli NPC tornano alle primitive: ogni personaggio
 * animato costa una rideformazione e un caricamento del buffer per frame
 * (~0.06 ms), e a 140 m un passo non si distingue piu'. */
#define NPC_MODEL_DIST     140.0f


/* --- Dati esterni -------------------------------------------------------
 * Nessun dato di gioco vive nel codice: se questi file mancano o contengono
 * errori il gioco non parte. Vedi docs/05-piano-dati-esterni-e-motore.md. */
#define DATA_DIR        "assets/data"
#define DATA_ITEMS      DATA_DIR "/items.txt"
#define DATA_ENTITIES   DATA_DIR "/entities.txt"
#define DATA_QUESTS     DATA_DIR "/quests.txt"
#define DATA_BALANCE    DATA_DIR "/balance.txt"
#define DATA_SHOP       DATA_DIR "/shop.txt"
#define DATA_RUMORS     DATA_DIR "/rumors.txt"

/* --- Simulazione a passo fisso ------------------------------------------
 * La simulazione avanza a scatti di SIM_STEP: la fisica (fase 4 del piano in
 * docs/05) con un dt variabile cambia comportamento, e a passo fisso il gioco
 * diventa riproducibile. SIM_MAX_STEPS evita la spirale in cui recuperare il
 * tempo perso costa piu' tempo di quello recuperato. */
#define SIM_STEP       (1.0 / 60.0)
#define SIM_MAX_FRAME  0.25          /* frame piu' lunghi vengono troncati */
#define SIM_MAX_STEPS  6             /* al massimo 6 passi per fotogramma  */

/* --- Tempo -------------------------------------------------------------
 * La durata del giorno e' un dato: BAL.daySeconds. */

#endif /* CONFIG_H */
