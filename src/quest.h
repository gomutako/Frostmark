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

typedef enum { Q_LOCKED, Q_OFFERED, Q_ACTIVE, Q_READY, Q_DONE } QuestState;

/* Come avanza una quest. Il gioco emette gli eventi, le quest reagiscono:
 * nessun collegamento cablato fra un nemico e un incarico. */
typedef enum { QEV_NONE, QEV_KILL, QEV_COLLECT } QuestEvent;

typedef struct {
    char       id[32];        /* identificatore stabile, es. "wolves" */
    char       title[64];
    char       giver[48];     /* etichetta descrittiva del committente */
    char       desc[320];
    char       objective[64];
    int        target;
    char       giverType[32]; /* tipo di entita' che assegna e riceve */
    QuestEvent event;
    char       eventTarget[32];  /* entita' da uccidere od oggetto da raccogliere */
    bool       consumesItems;    /* alla consegna sottrae "target" oggetti */
    char       unlocks[32];      /* quest sbloccata alla consegna, "" se nessuna */
    QuestState initialState;
    int        rewardGold, rewardXp;
    char       rewardItem[32];   /* identificatore, "" per nessuna ricompensa */
} QuestDef;

typedef struct { QuestState state; int progress; } QuestInst;

extern QuestDef QUESTS[MAX_QUESTS];
int  QuestCount(void);
bool QuestsLoad(const char *path);

/* Eventi: il gioco li emette senza sapere quali quest ne dipendono. */
void QuestOnKill(struct Game *g, int entityType);
void QuestOnCollect(struct Game *g, int itemId);
/* Quest che questo tipo di entita' sta offrendo o attende, -1 se nessuna. */
int  QuestForGiver(struct Game *g, int entityType);

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
