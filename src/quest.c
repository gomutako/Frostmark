#include "quest.h"
#include "dataparse.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

const QuestDef QUESTS[MAX_QUESTS] = {
    {
        "wolves",
        "La minaccia dei lupi",
        "Guardia del villaggio",
        "I branchi sono scesi a valle e le greggi non sono piu' al sicuro. "
        "La guardia ti chiede di ridurne il numero.",
        "Uccidi i lupi", 5,
        120, 150, "iron_sword"
    },
    {
        "herbs",
        "Erbe per l'infermeria",
        "Anziano del villaggio",
        "L'infermeria e' a corto di scorte. Raccogli erbe curative nei prati "
        "e nei boschi (premi E vicino alle piante luminose).",
        "Raccogli erbe curative", 5,
        90, 110, "leather_armor"
    },
    {
        "boss",
        "Il Sepolto",
        "Anziano del villaggio",
        "Sotto la cripta a nord-est dorme Vald, un antico signore della guerra. "
        "Qualcosa lo ha risvegliato. Raggiungi la cripta e mettilo a tacere.",
        "Sconfiggi Vald il Sepolto", 1,
        600, 500, "ancient_blade"
    },
};

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
    for (int i = 0; i < MAX_QUESTS; i++)
        if (strcmp(QUESTS[i].id, id) == 0) return i;
    return -1;
}

unsigned int QuestStableId(int quest)
{
    if (quest < 0 || quest >= MAX_QUESTS) return 0u;
    return DataId(QUESTS[quest].id);
}

int QuestFromStableId(unsigned int stable)
{
    if (stable == 0u) return -1;
    for (int i = 0; i < MAX_QUESTS; i++)
        if (DataId(QUESTS[i].id) == stable) return i;
    return -1;
}

void QuestInitAll(QuestInst *q)
{
    for (int i = 0; i < MAX_QUESTS; i++) { q[i].state = Q_OFFERED; q[i].progress = 0; }
    q[QUEST_BOSS].state = Q_LOCKED;   /* si sblocca dopo la prima quest */
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

    /* La quest delle erbe consuma il materiale raccolto. */
    if (questId == QUEST_HERBS)
        InvRemove(g->player.inv, ItemFind("herb"), QUESTS[QUEST_HERBS].target);

    /* Sblocco della quest principale. */
    if (questId == QUEST_WOLVES && g->quests[QUEST_BOSS].state == Q_LOCKED)
        g->quests[QUEST_BOSS].state = Q_OFFERED;

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
static int QuestForNPC(Game *g, EntityType t)
{
    if (t == ENT_GUARD) {
        if (g->quests[QUEST_WOLVES].state == Q_OFFERED ||
            g->quests[QUEST_WOLVES].state == Q_READY) return QUEST_WOLVES;
    }
    if (t == ENT_ELDER) {
        if (g->quests[QUEST_BOSS].state == Q_OFFERED ||
            g->quests[QUEST_BOSS].state == Q_READY) return QUEST_BOSS;
        if (g->quests[QUEST_HERBS].state == Q_OFFERED ||
            g->quests[QUEST_HERBS].state == Q_READY) return QUEST_HERBS;
    }
    return -1;
}

void DialogueBuild(Game *g, Entity *e, Dialogue *out)
{
    memset(out, 0, sizeof(Dialogue));
    if (e == NULL) return;

    const char *townName = "queste terre";
    if (e->townIndex >= 0 && e->townIndex < g->world.townCount)
        townName = g->world.towns[e->townIndex].name;

    TextCopy(out->speaker, e->name);

    int q = QuestForNPC(g, e->type);

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

    switch (e->type) {
        case ENT_MERCHANT:
            snprintf(out->text, sizeof(out->text),
                     "Merce onesta a prezzi quasi onesti. Cosa ti serve?");
            AddOpt(out, "Mostrami la merce.", DACT_SHOP, 0);
            AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
            AddOpt(out, "Nulla, grazie.", DACT_CLOSE, 0);
            break;

        case ENT_GUARD:
            snprintf(out->text, sizeof(out->text),
                     "Tieni la lama nel fodero dentro le mura di %s e non avremo problemi.",
                     townName);
            AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
            AddOpt(out, "Buona guardia.", DACT_CLOSE, 0);
            break;

        case ENT_ELDER:
            snprintf(out->text, sizeof(out->text),
                     "Le pietre di %s ricordano piu' cose di quante ne dimentichino gli uomini.",
                     townName);
            AddOpt(out, "Puoi curarmi?", DACT_HEAL, 0);
            AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
            AddOpt(out, "Ti lascio ai tuoi pensieri.", DACT_CLOSE, 0);
            break;

        default:
            snprintf(out->text, sizeof(out->text),
                     "Giornata dura, forestiero. Le strade fuori da %s non sono sicure.",
                     townName);
            AddOpt(out, "Che si dice in giro?", DACT_RUMOR, 0);
            AddOpt(out, "Addio.", DACT_CLOSE, 0);
            break;
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
