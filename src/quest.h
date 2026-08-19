/* ============================================================================
 * quest.h - Sistema di quest a obiettivi contatori + dialoghi generati.
 *
 * Le quest sono descritte da una tabella statica (QUESTS) e lo stato del
 * giocatore e' un semplice array di QuestInst. I dialoghi non sono un albero
 * di dati ma vengono COSTRUITI a runtime in base al tipo di NPC e allo stato
 * delle quest: molto piu' compatto e facile da estendere.
 * ========================================================================== */
#ifndef QUEST_H
#define QUEST_H

#include <stdbool.h>
#include "config.h"
#include "dataid.h"

struct Game;
struct Entity;

enum { QUEST_WOLVES = 0, QUEST_HERBS = 1, QUEST_BOSS = 2 };

typedef enum { Q_LOCKED, Q_OFFERED, Q_ACTIVE, Q_READY, Q_DONE } QuestState;

typedef struct {
    const char *id;         /* identificatore stabile, es. "wolves" */
    const char *title;
    const char *giver;      /* etichetta descrittiva del committente */
    const char *desc;
    const char *objective;
    int         target;
    int         rewardGold, rewardXp;
    const char *rewardItem;   /* identificatore, "" per nessuna ricompensa */
} QuestDef;

typedef struct { QuestState state; int progress; } QuestInst;

extern const QuestDef QUESTS[MAX_QUESTS];

/* Indice della quest dato il suo identificatore, -1 se non esiste. */
int QuestFind(const char *id);
unsigned int QuestStableId(int quest);
int QuestFromStableId(unsigned int stable);   /* -1 se sconosciuta */

/* Dicerie, caricate da assets/data/rumors.txt. */
bool RumorsLoad(const char *path);
int  RumorCount(void);
const char *RumorText(int index);

void QuestInitAll(QuestInst *q);
void QuestProgress(struct Game *g, int questId, int amount);
void QuestAccept(struct Game *g, int questId);
void QuestTurnIn(struct Game *g, int questId);
const char *QuestStateLabel(QuestState s);

/* ---- Dialoghi ---------------------------------------------------------- */

typedef enum {
    DACT_CLOSE, DACT_ACCEPT_QUEST, DACT_TURNIN_QUEST,
    DACT_SHOP, DACT_RUMOR, DACT_HEAL
} DlgAction;

typedef struct { char text[110]; DlgAction action; int param; } DlgOption;

typedef struct {
    char      speaker[32];
    char      text[420];
    DlgOption opts[4];
    int       optCount;
} Dialogue;

void DialogueBuild(struct Game *g, struct Entity *e, Dialogue *out);
void DialogueChoose(struct Game *g, struct Entity *e, int optionIndex);

#endif /* QUEST_H */
