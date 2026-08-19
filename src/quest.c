#include "quest.h"
#include "dataparse.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

QuestDef QUESTS[MAX_QUESTS];
static int gQuestCount = 0;

int QuestCount(void) { return gQuestCount; }

static const char *const STATE_NAMES[] = { "bloccata", "offerta" };
static const char *const SI_NO[]       = { "no", "si" };

bool QuestsLoad(const char *path)
{
    DataReader r;
    memset(QUESTS, 0, sizeof(QUESTS));
    gQuestCount = 0;

    if (!DataOpen(&r, path)) return false;
    int before = DataProblemCount();

    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "quest") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        if (r.id[0] == '\0') {
            DataProblem(&r, "sezione [quest] senza identificatore");
            DataSkipSection(&r);
            continue;
        }
        if (QuestFind(r.id) >= 0) {
            DataProblem(&r, "quest \"%s\" definita due volte", r.id);
            DataSkipSection(&r);
            continue;
        }
        if (gQuestCount >= MAX_QUESTS) {
            DataProblem(&r, "troppe quest: il massimo e' %d", MAX_QUESTS);
            DataSkipSection(&r);
            continue;
        }

        QuestDef *q = &QUESTS[gQuestCount];
        memset(q, 0, sizeof(*q));
        TextCopy(q->id, r.id);
        q->target = -1;
        q->rewardGold = q->rewardXp = -1;
        q->initialState = (QuestState)-1;
        int at = r.kindLine;

        char *key, *val;
        int tmp;
        while (DataNextField(&r, &key, &val)) {
            if      (strcmp(key, "titolo") == 0)      DataAsText(&r, key, val, q->title, sizeof(q->title));
            else if (strcmp(key, "committente") == 0) DataAsText(&r, key, val, q->giver, sizeof(q->giver));
            else if (strcmp(key, "descrizione") == 0) DataAsText(&r, key, val, q->desc, sizeof(q->desc));
            else if (strcmp(key, "obiettivo") == 0)   DataAsText(&r, key, val, q->objective, sizeof(q->objective));
            else if (strcmp(key, "assegnata_da") == 0)DataAsText(&r, key, val, q->giverType, sizeof(q->giverType));
            else if (strcmp(key, "sblocca") == 0)     DataAsText(&r, key, val, q->unlocks, sizeof(q->unlocks));
            else if (strcmp(key, "ricompensa") == 0)  DataAsText(&r, key, val, q->rewardItem, sizeof(q->rewardItem));
            else if (strcmp(key, "bersaglio") == 0)   DataAsInt(&r, key, val, 1, 10000, &q->target);
            else if (strcmp(key, "oro") == 0)         DataAsInt(&r, key, val, 0, 1000000, &q->rewardGold);
            else if (strcmp(key, "esperienza") == 0)  DataAsInt(&r, key, val, 0, 1000000, &q->rewardXp);
            else if (strcmp(key, "consuma") == 0) {
                if (DataAsEnum(&r, key, val, SI_NO, 2, &tmp)) q->consumesItems = (tmp != 0);
            }
            else if (strcmp(key, "stato_iniziale") == 0) {
                if (DataAsEnum(&r, key, val, STATE_NAMES, 2, &tmp))
                    q->initialState = (tmp == 0) ? Q_LOCKED : Q_OFFERED;
            }
            else if (strcmp(key, "avanza_con") == 0) {
                /* "uccisione:wolf" oppure "raccolta:herb" */
                const char *colon = strchr(val, ':');
                if (colon == NULL) {
                    DataProblem(&r, "avanza_con: atteso \"uccisione:<entita>\" o "
                                    "\"raccolta:<oggetto>\", trovato \"%s\"", val);
                } else {
                    char kind[32];
                    int n = (int)(colon - val);
                    if (n >= (int)sizeof(kind)) n = (int)sizeof(kind) - 1;
                    memcpy(kind, val, (size_t)n);
                    kind[n] = '\0';
                    if      (strcmp(kind, "uccisione") == 0) q->event = QEV_KILL;
                    else if (strcmp(kind, "raccolta") == 0)  q->event = QEV_COLLECT;
                    else DataProblem(&r, "avanza_con: \"%s\" non ammesso; "
                                         "valori possibili: uccisione, raccolta", kind);
                    DataAsText(&r, key, colon + 1, q->eventTarget, sizeof(q->eventTarget));
                }
            }
            else DataProblem(&r, "chiave \"%s\" sconosciuta per una quest", key);
        }

        if (q->title[0] == '\0')      DataProblemAt(&r, at, "%s: manca \"titolo\"", q->id);
        if (q->desc[0] == '\0')       DataProblemAt(&r, at, "%s: manca \"descrizione\"", q->id);
        if (q->objective[0] == '\0')  DataProblemAt(&r, at, "%s: manca \"obiettivo\"", q->id);
        if (q->target < 0)            DataProblemAt(&r, at, "%s: manca \"bersaglio\"", q->id);
        if (q->rewardGold < 0)        DataProblemAt(&r, at, "%s: manca \"oro\"", q->id);
        if (q->rewardXp < 0)          DataProblemAt(&r, at, "%s: manca \"esperienza\"", q->id);
        if (q->event == QEV_NONE)     DataProblemAt(&r, at, "%s: manca \"avanza_con\"", q->id);
        if ((int)q->initialState < 0) DataProblemAt(&r, at, "%s: manca \"stato_iniziale\"", q->id);
        if (q->giverType[0] == '\0')  DataProblemAt(&r, at, "%s: manca \"assegnata_da\"", q->id);

        if (q->rewardItem[0] != '\0' && ItemFind(q->rewardItem) <= ITEM_NONE)
            DataProblemAt(&r, at, "%s: ricompensa \"%s\" non definita in items.txt",
                          q->id, q->rewardItem);
        if (q->event == QEV_COLLECT && ItemFind(q->eventTarget) <= ITEM_NONE)
            DataProblemAt(&r, at, "%s: raccolta di \"%s\", non definito in items.txt",
                          q->id, q->eventTarget);
        /* I riferimenti alle entita' vanno verificati qui: un committente
         * inesistente non darebbe errore, la quest non comparirebbe mai. */
        if (q->event == QEV_KILL && EntityFind(q->eventTarget) == ENT_NONE)
            DataProblemAt(&r, at, "%s: uccisione di \"%s\", tipo non definito in "
                          "entities.txt", q->id, q->eventTarget);
        if (q->giverType[0] != '\0' && EntityFind(q->giverType) == ENT_NONE)
            DataProblemAt(&r, at, "%s: assegnata_da \"%s\", tipo non definito in "
                          "entities.txt", q->id, q->giverType);

        gQuestCount++;
    }
    DataClose(&r);
    if (gQuestCount == 0) DataProblem(NULL, "%s: nessuna quest definita", path);

    /* I riferimenti fra quest si risolvono a caricamento finito: l'ordine delle
     * sezioni non deve contare. */
    for (int i = 0; i < gQuestCount; i++)
        if (QUESTS[i].unlocks[0] != '\0' && QuestFind(QUESTS[i].unlocks) < 0)
            DataProblem(NULL, "%s: la quest \"%s\" da sbloccare non esiste",
                        QUESTS[i].id, QUESTS[i].unlocks);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d quest da %s", gQuestCount, path);
    return ok;
}


const char *QuestStateLabel(QuestState s)
{
    switch (s) {
        case Q_LOCKED:  return "non disponibile";
        case Q_OFFERED: return "proposta";
        case Q_ACTIVE:  return "in corso";
        case Q_READY:   return "da consegnare";
        case Q_DONE:    return "completata";
        default:        return "?";
    }
}

int QuestFind(const char *id)
{
    if (id == NULL) return -1;
    for (int i = 0; i < gQuestCount; i++)
        if (strcmp(QUESTS[i].id, id) == 0) return i;
    return -1;
}

unsigned int QuestStableId(int quest)
{
    if (quest < 0 || quest >= gQuestCount) return 0u;
    return DataId(QUESTS[quest].id);
}

int QuestFromStableId(unsigned int stable)
{
    if (stable == 0u) return -1;
    for (int i = 0; i < gQuestCount; i++)
        if (DataId(QUESTS[i].id) == stable) return i;
    return -1;
}

void QuestInitAll(QuestInst *q)
{
    for (int i = 0; i < MAX_QUESTS; i++) { q[i].state = Q_LOCKED; q[i].progress = 0; }
    for (int i = 0; i < gQuestCount; i++) q[i].state = QUESTS[i].initialState;
}

void QuestAccept(Game *g, int questId)
{
    if (questId < 0 || questId >= MAX_QUESTS) return;
    if (g->quests[questId].state != Q_OFFERED) return;
    g->quests[questId].state = Q_ACTIVE;
    GameToast(g, "Nuovo incarico: %s", QUESTS[questId].title);
}

void QuestProgress(Game *g, int questId, int amount)
{
    if (questId < 0 || questId >= MAX_QUESTS) return;
    QuestInst *q = &g->quests[questId];
    if (q->state != Q_ACTIVE) return;

    q->progress += amount;
    if (q->progress >= QUESTS[questId].target) {
        q->progress = QUESTS[questId].target;
        q->state = Q_READY;
        GameToast(g, "Obiettivo completato: %s - torna dal committente",
                  QUESTS[questId].title);
    } else {
        GameToast(g, "%s: %d/%d", QUESTS[questId].objective,
                  q->progress, QUESTS[questId].target);
    }
}

void QuestTurnIn(Game *g, int questId)
{
    if (questId < 0 || questId >= MAX_QUESTS) return;
    QuestInst *q = &g->quests[questId];
    if (q->state != Q_READY) return;

    q->state = Q_DONE;
    g->player.gold += QUESTS[questId].rewardGold;
    PlayerAddXP(&g->player, QUESTS[questId].rewardXp);
    int reward = ItemFind(QUESTS[questId].rewardItem);
    if (reward > ITEM_NONE) InvAdd(g->player.inv, reward, 1);

    /* Alcune quest consumano il materiale raccolto. */
    if (QUESTS[questId].consumesItems && QUESTS[questId].event == QEV_COLLECT) {
        int item = ItemFind(QUESTS[questId].eventTarget);
        if (item > ITEM_NONE)
            InvRemove(g->player.inv, item, QUESTS[questId].target);
    }

    /* Sblocco a catena, dichiarato nei dati. */
    if (QUESTS[questId].unlocks[0] != '\0') {
        int next = QuestFind(QUESTS[questId].unlocks);
        if (next >= 0 && g->quests[next].state == Q_LOCKED)
            g->quests[next].state = Q_OFFERED;
    }

    GameToast(g, "Ricompensa: %d oro, %d PE, %s",
              QUESTS[questId].rewardGold, QUESTS[questId].rewardXp,
              ITEMS[reward > ITEM_NONE ? reward : ITEM_NONE].name);
}

/* ------------------------------------------------------------------------ */
/*  DIALOGHI                                                                */
/* ------------------------------------------------------------------------ */

static void AddOpt(Dialogue *d, const char *text, DlgAction a, int param)
{
    if (d->optCount >= 4) return;
    TextCopy(d->opts[d->optCount].text, text);
    d->opts[d->optCount].action = a;
    d->opts[d->optCount].param  = param;
    d->optCount++;
}

/* Sceglie quale quest questo NPC puo' gestire in questo momento. */
/* Le quest da consegnare hanno la precedenza su quelle da offrire, e fra pari
 * vince l'ultima definita: cosi' l'ordine nel file esprime la priorita'. */
int QuestForGiver(Game *g, int entityType)
{
    int offered = -1, ready = -1;
    for (int i = 0; i < gQuestCount; i++) {
        if (EntityFind(QUESTS[i].giverType) != entityType) continue;
        if (g->quests[i].state == Q_READY)   ready = i;
        if (g->quests[i].state == Q_OFFERED) offered = i;
    }
    return (ready >= 0) ? ready : offered;
}

/* ---- Eventi ------------------------------------------------------------ */

static void QuestFireEvent(Game *g, QuestEvent kind, const char *target)
{
    for (int i = 0; i < gQuestCount; i++) {
        if (QUESTS[i].event != kind) continue;
        if (strcmp(QUESTS[i].eventTarget, target) != 0) continue;
        if (g->quests[i].state != Q_ACTIVE) continue;
        QuestProgress(g, i, 1);
    }
}

void QuestOnKill(Game *g, int entityType)
{
    if (entityType < 0 || entityType >= EntityTypeCount()) return;
    QuestFireEvent(g, QEV_KILL, ENTITY_TYPES[entityType].id);
}

void QuestOnCollect(Game *g, int itemId)
{
    if (itemId <= ITEM_NONE || itemId >= ItemCount()) return;
    QuestFireEvent(g, QEV_COLLECT, ITEMS[itemId].id);
}

void DialogueBuild(Game *g, Entity *e, Dialogue *out)
{
    memset(out, 0, sizeof(Dialogue));
    if (e == NULL) return;

    const char *townName = "queste terre";
    if (e->townIndex >= 0 && e->townIndex < g->world.townCount)
        townName = g->world.towns[e->townIndex].name;

    TextCopy(out->speaker, e->name);

    int q = QuestForGiver(g, e->type);

    if (q >= 0 && g->quests[q].state == Q_READY) {
        snprintf(out->text, sizeof(out->text),
                 "Hai fatto quello che chiedevo. %s ha un debito con te, forestiero.",
                 townName);
        AddOpt(out, TextFormat("Consegna: %s", QUESTS[q].title), DACT_TURNIN_QUEST, q);
        AddOpt(out, "Ci vediamo.", DACT_CLOSE, 0);
        return;
    }

    if (q >= 0 && g->quests[q].state == Q_OFFERED) {
        snprintf(out->text, sizeof(out->text), "%s", QUESTS[q].desc);
        AddOpt(out, "Accetto l'incarico.", DACT_ACCEPT_QUEST, q);
        AddOpt(out, "Ci pensero'.", DACT_CLOSE, 0);
        AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
        return;
    }

    /* I tipi si confrontano per identificatore: il testo dei dialoghi e' l'ultimo
     * dato ancora nel codice, e migrera' con la grammatica di condizioni
     * (fase 2 del piano in docs/05). */
    if (e->type == EntityFind("merchant")) {
        snprintf(out->text, sizeof(out->text),
                 "Merce onesta a prezzi quasi onesti. Cosa ti serve?");
        AddOpt(out, "Mostrami la merce.", DACT_SHOP, 0);
        AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
        AddOpt(out, "Nulla, grazie.", DACT_CLOSE, 0);
    } else if (e->type == EntityFind("guard")) {
        snprintf(out->text, sizeof(out->text),
                 "Tieni la lama nel fodero dentro le mura di %s e non avremo problemi.",
                 townName);
        AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
        AddOpt(out, "Buona guardia.", DACT_CLOSE, 0);
    } else if (e->type == EntityFind("elder")) {
        snprintf(out->text, sizeof(out->text),
                 "Le pietre di %s ricordano piu' cose di quante ne dimentichino gli uomini.",
                 townName);
        AddOpt(out, "Puoi curarmi?", DACT_HEAL, 0);
        AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
        AddOpt(out, "Ti lascio ai tuoi pensieri.", DACT_CLOSE, 0);
    } else {
        snprintf(out->text, sizeof(out->text),
                 "Giornata dura, forestiero. Le strade fuori da %s non sono sicure.",
                 townName);
        AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
        AddOpt(out, "Addio.", DACT_CLOSE, 0);
    }
}

/* Dicerie caricate da file: array di testi, con la lunghezza dichiarata. */
static char gRumors[MAX_RUMORS][160];
static int  gRumorCount = 0;

int RumorCount(void) { return gRumorCount; }

const char *RumorText(int index)
{
    if (index < 0 || index >= gRumorCount) return "";
    return gRumors[index];
}

bool RumorsLoad(const char *path)
{
    DataReader r;
    gRumorCount = 0;
    if (!DataOpen(&r, path)) return false;

    int before = DataProblemCount();
    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "dicerie") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        char *key, *val;
        while (DataNextField(&r, &key, &val)) {
            if (strcmp(key, "testo") != 0) {
                DataProblem(&r, "chiave \"%s\" sconosciuta per le dicerie", key);
                continue;
            }
            if (gRumorCount >= MAX_RUMORS) {
                DataProblem(&r, "troppe dicerie: il massimo e' %d", MAX_RUMORS);
                continue;
            }
            if (DataAsText(&r, key, val, gRumors[gRumorCount], 160)) gRumorCount++;
        }
    }
    DataClose(&r);
    if (gRumorCount == 0) DataProblem(NULL, "%s: nessuna diceria definita", path);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d dicerie da %s", gRumorCount, path);
    return ok;
}


void DialogueChoose(Game *g, Entity *e, int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= g->dlg.optCount) return;
    DlgOption *o = &g->dlg.opts[optionIndex];

    switch (o->action) {
        case DACT_ACCEPT_QUEST:
            QuestAccept(g, o->param);
            g->state = GS_PLAY;
            break;

        case DACT_TURNIN_QUEST:
            QuestTurnIn(g, o->param);
            g->state = GS_PLAY;
            break;

        case DACT_SHOP:
            g->shopCursor = 0;
            g->state = GS_SHOP;
            break;

        case DACT_HEAL:
            if (g->player.gold >= 20) {
                g->player.gold -= 20;
                g->player.hp = g->player.maxHp;
                GameToast(g, "L'anziano ti rimette in sesto. (-20 oro)");
            } else {
                GameToast(g, "Servono 20 monete d'oro.");
            }
            g->state = GS_PLAY;
            break;

        case DACT_RUMOR: {
            int n = RumorCount();
            int idx = (int)(g->playTime * 3.0f) % n;
            snprintf(g->dlg.text, sizeof(g->dlg.text), "%s", RumorText(idx));
            g->dlg.optCount = 0;
            AddOpt(&g->dlg, "Interessante. Addio.", DACT_CLOSE, 0);
            g->dialogueOpt = 0;
        } break;

        case DACT_CLOSE:
        default:
            g->state = GS_PLAY;
            break;
    }
    (void)e;
}
