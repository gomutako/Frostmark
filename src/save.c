#include "save.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC   0x4D525346u   /* "FSRM" */
#define SAVE_VERSION 1

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int seed;

    float px, py, pz, yaw, pitch;
    float hp, maxHp, sta, maxSta, mp, maxMp;
    int   level, xp, xpNext, gold, skillMelee, skillMagic;
    int   weapon, armor;
    int   wolvesKilled, herbsPicked;
    int   bossKilled;

    InvSlot inv[MAX_INVENTORY];

    int   questState[MAX_QUESTS];
    int   questProgress[MAX_QUESTS];

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
    s.weapon = p->weapon; s.armor = p->armor;
    s.wolvesKilled = p->wolvesKilled; s.herbsPicked = p->herbsPicked;
    s.bossKilled = p->bossKilled ? 1 : 0;
    memcpy(s.inv, p->inv, sizeof(s.inv));

    for (int i = 0; i < MAX_QUESTS; i++) {
        s.questState[i]    = (int)g->quests[i].state;
        s.questProgress[i] = g->quests[i].progress;
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
    if (s.magic != SAVE_MAGIC || s.version != SAVE_VERSION) return false;

    /* Rigenera il mondo dal seme salvato, poi ripristina il giocatore. */
    GameNewWorld(g, s.seed);

    Player *p = &g->player;
    p->pos = (Vector3){ s.px, s.py, s.pz };
    p->yaw = s.yaw; p->pitch = s.pitch;
    p->hp = s.hp; p->maxHp = s.maxHp;
    p->sta = s.sta; p->maxSta = s.maxSta;
    p->mp = s.mp; p->maxMp = s.maxMp;
    p->level = s.level; p->xp = s.xp; p->xpNext = s.xpNext; p->gold = s.gold;
    p->skillMelee = s.skillMelee; p->skillMagic = s.skillMagic;
    p->weapon = s.weapon; p->armor = s.armor;
    p->wolvesKilled = s.wolvesKilled; p->herbsPicked = s.herbsPicked;
    p->bossKilled = (s.bossKilled != 0);
    memcpy(p->inv, s.inv, sizeof(p->inv));

    for (int i = 0; i < MAX_QUESTS; i++) {
        g->quests[i].state    = (QuestState)s.questState[i];
        g->quests[i].progress = s.questProgress[i];
    }
    g->timeOfDay = s.timeOfDay;
    g->playTime  = s.playTime;
    return true;
}
