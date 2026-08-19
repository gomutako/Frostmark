#include "save.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC   0x4D525346u   /* "FSRM" */
#define SAVE_VERSION 2

/* Versione 2: gli oggetti e le quest sono identificati dall'hash del loro nome
 * (vedi dataid.h) e non dalla posizione in una tabella. Aggiungere, togliere o
 * riordinare una definizione non cambia piu' il significato di una partita
 * salvata - cosa indispensabile quando le definizioni arriveranno da file
 * esterni. I conteggi sono scritti nel file, cosi' alzare MAX_INVENTORY o
 * MAX_QUESTS non invalida i salvataggi esistenti.
 *
 * La versione 1 non e' leggibile: era indicizzata per posizione, e indovinare a
 * quale oggetto corrispondesse un indice sarebbe stato peggio di rifiutarla. */

typedef struct { unsigned int item; int qty; } SaveSlot;
typedef struct { unsigned int quest; int state; int progress; } SaveQuest;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int seed;

    float px, py, pz, yaw, pitch;
    float hp, maxHp, sta, maxSta, mp, maxMp;
    int   level, xp, xpNext, gold, skillMelee, skillMagic;
    unsigned int weapon, armor;          /* identificatori stabili, 0 = nudo */
    int   wolvesKilled, herbsPicked;
    int   bossKilled;

    int       slotCount;
    SaveSlot  slots[MAX_INVENTORY];

    int       questCount;
    SaveQuest quests[MAX_QUESTS];

    float timeOfDay;
    float playTime;
} SaveData;

bool SaveGameToFile(Game *g, const char *path)
{
    SaveData s;
    memset(&s, 0, sizeof(s));

    s.magic   = SAVE_MAGIC;
    s.version = SAVE_VERSION;
    s.seed    = g->world.seed;

    Player *p = &g->player;
    s.px = p->pos.x; s.py = p->pos.y; s.pz = p->pos.z;
    s.yaw = p->yaw;  s.pitch = p->pitch;
    s.hp = p->hp; s.maxHp = p->maxHp;
    s.sta = p->sta; s.maxSta = p->maxSta;
    s.mp = p->mp; s.maxMp = p->maxMp;
    s.level = p->level; s.xp = p->xp; s.xpNext = p->xpNext; s.gold = p->gold;
    s.skillMelee = p->skillMelee; s.skillMagic = p->skillMagic;
    s.weapon = ItemStableId(p->weapon);
    s.armor  = ItemStableId(p->armor);
    s.wolvesKilled = p->wolvesKilled; s.herbsPicked = p->herbsPicked;
    s.bossKilled = p->bossKilled ? 1 : 0;

    /* Solo gli slot pieni, con l'identificatore dell'oggetto. */
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (p->inv[i].id == ITEM_NONE || p->inv[i].qty <= 0) continue;
        s.slots[s.slotCount].item = ItemStableId(p->inv[i].id);
        s.slots[s.slotCount].qty  = p->inv[i].qty;
        s.slotCount++;
    }

    for (int i = 0; i < MAX_QUESTS; i++) {
        s.quests[s.questCount].quest    = QuestStableId(i);
        s.quests[s.questCount].state    = (int)g->quests[i].state;
        s.quests[s.questCount].progress = g->quests[i].progress;
        s.questCount++;
    }

    s.timeOfDay = g->timeOfDay;
    s.playTime  = g->playTime;

    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t n = fwrite(&s, sizeof(s), 1, f);
    fclose(f);
    return n == 1;
}

bool LoadGameFromFile(Game *g, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    SaveData s;
    size_t n = fread(&s, sizeof(s), 1, f);
    fclose(f);
    if (n != 1) return false;
    if (s.magic != SAVE_MAGIC) return false;
    if (s.version != SAVE_VERSION) {
        TraceLog(LOG_WARNING, "SAVE: versione %u non supportata (serve la %d)",
                 s.version, SAVE_VERSION);
        return false;
    }
    if (s.slotCount  < 0 || s.slotCount  > MAX_INVENTORY) return false;
    if (s.questCount < 0 || s.questCount > MAX_QUESTS)    return false;

    /* La visuale non sta nel salvataggio: e' una preferenza di vista, non
     * stato di gioco. GameNewWorld() rifa' il giocatore da zero, quindi la si
     * mette da parte e la si rimette dopo. */
    CamMode camMode = g->player.camMode;
    float   camDist = g->player.camDist;

    /* Il mondo non si rigenera piu' dal seme: e' un dato, e ce n'e' uno solo.
     * Il seme salvato serve a riconoscerlo: una partita fatta in un altro mondo
     * ha coordinate che qui cadono altrove - in mezzo al mare o dentro una
     * montagna - e caricarla sarebbe peggio che rifiutarla. */
    if (s.seed != g->world.seed) {
        TraceLog(LOG_WARNING, "SAVE: la partita viene dal mondo %u, questo e' il "
                              "%u. Caricamento rifiutato.", s.seed, g->world.seed);
        return false;
    }

    /* Ricarica il mondo e ripristina il giocatore. */
    if (!GameNewWorld(g)) return false;

    Player *p = &g->player;
    p->camMode = camMode;
    p->camDist = camDist;
    p->pos = (Vector3){ s.px, s.py, s.pz };
    p->yaw = s.yaw; p->pitch = s.pitch;
    p->hp = s.hp; p->maxHp = s.maxHp;
    p->sta = s.sta; p->maxSta = s.maxSta;
    p->mp = s.mp; p->maxMp = s.maxMp;
    p->level = s.level; p->xp = s.xp; p->xpNext = s.xpNext; p->gold = s.gold;
    p->skillMelee = s.skillMelee; p->skillMagic = s.skillMagic;
    p->weapon = ItemFromStableId(s.weapon);
    p->armor  = ItemFromStableId(s.armor);
    p->wolvesKilled = s.wolvesKilled; p->herbsPicked = s.herbsPicked;
    p->bossKilled = (s.bossKilled != 0);

    /* L'inventario si ricostruisce risolvendo gli identificatori: un oggetto
     * che non esiste piu' viene scartato con un avviso, non fa fallire il
     * caricamento. */
    memset(p->inv, 0, sizeof(p->inv));
    for (int i = 0; i < s.slotCount; i++) {
        int id = ItemFromStableId(s.slots[i].item);
        if (id == ITEM_NONE) {
            TraceLog(LOG_WARNING, "SAVE: oggetto sconosciuto (%08x), scartato",
                     s.slots[i].item);
            continue;
        }
        InvAdd(p->inv, id, s.slots[i].qty);
    }

    for (int i = 0; i < s.questCount; i++) {
        int q = QuestFromStableId(s.quests[i].quest);
        if (q < 0) {
            TraceLog(LOG_WARNING, "SAVE: quest sconosciuta (%08x), ignorata",
                     s.quests[i].quest);
            continue;
        }
        g->quests[q].state    = (QuestState)s.quests[i].state;
        g->quests[q].progress = s.quests[i].progress;
    }

    g->timeOfDay = s.timeOfDay;
    g->playTime  = s.playTime;
    return true;
}
