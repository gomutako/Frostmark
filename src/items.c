#include "items.h"
#include "dataparse.h"
#include "raylib.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/*  CATALOGO CARICATO DA FILE                                               */
/* ------------------------------------------------------------------------ */

ItemDef ITEMS[MAX_ITEMS];
static int gItemCount = 0;

int ItemCount(void) { return gItemCount; }

/* I nomi dei tipi come compaiono nei file, nell'ordine di ItemKind. */
static const char *const KIND_NAMES[] = {
    "nessuno", "arma", "armatura", "pozione", "cibo", "varie"
};

bool ItemsLoad(const char *path)
{
    DataReader r;
    memset(ITEMS, 0, sizeof(ITEMS));
    gItemCount = 0;

    /* L'indice 0 e' la sentinella "nessun oggetto": non sta nel file. */
    TextCopy(ITEMS[0].id, "none");
    TextCopy(ITEMS[0].name, "-");
    ITEMS[0].kind = IK_NONE;
    gItemCount = 1;

    if (!DataOpen(&r, path)) return false;

    int before = DataProblemCount();
    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "oggetto") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        if (r.id[0] == '\0') {
            DataProblem(&r, "sezione [oggetto] senza identificatore");
            DataSkipSection(&r);
            continue;
        }
        if (ItemFind(r.id) >= 0) {
            DataProblem(&r, "oggetto \"%s\" definito due volte", r.id);
            DataSkipSection(&r);
            continue;
        }
        if (gItemCount >= MAX_ITEMS) {
            DataProblem(&r, "troppi oggetti: il massimo e' %d", MAX_ITEMS - 1);
            DataSkipSection(&r);
            continue;
        }

        ItemDef *it = &ITEMS[gItemCount];
        memset(it, 0, sizeof(*it));
        if (!DataAsText(&r, "id", r.id, it->id, sizeof(it->id))) {
            DataSkipSection(&r);
            continue;
        }
        it->kind = IK_NONE;
        it->value = -1;
        it->power = -1.0f;

        char *key, *val;
        while (DataNextField(&r, &key, &val)) {
            if      (strcmp(key, "nome") == 0)
                DataAsText(&r, key, val, it->name, sizeof(it->name));
            else if (strcmp(key, "descrizione") == 0)
                DataAsText(&r, key, val, it->desc, sizeof(it->desc));
            else if (strcmp(key, "valore") == 0)
                DataAsInt(&r, key, val, 0, 100000, &it->value);
            else if (strcmp(key, "potenza") == 0)
                DataAsFloat(&r, key, val, 0.0f, 10000.0f, &it->power);
            else if (strcmp(key, "tipo") == 0) {
                int kind = 0;
                if (DataAsEnum(&r, key, val, KIND_NAMES,
                               (int)(sizeof(KIND_NAMES) / sizeof(KIND_NAMES[0])), &kind))
                    it->kind = (ItemKind)kind;
            }
            else DataProblem(&r, "chiave \"%s\" sconosciuta per un oggetto", key);
        }

        /* Nessun campo ha un valore di ripiego: se manca, e' un errore. */
        int at = r.kindLine;      /* la sezione, non la riga dove siamo ora */
        if (it->name[0] == '\0') DataProblemAt(&r, at, "%s: manca \"nome\"", it->id);
        if (it->kind == IK_NONE)  DataProblemAt(&r, at, "%s: manca \"tipo\"", it->id);
        if (it->value < 0)        DataProblemAt(&r, at, "%s: manca \"valore\"", it->id);
        if (it->power < 0.0f)     DataProblemAt(&r, at, "%s: manca \"potenza\"", it->id);

        gItemCount++;
    }
    DataClose(&r);

    if (gItemCount <= 1) DataProblem(NULL, "%s: nessun oggetto definito", path);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d oggetti da %s", gItemCount - 1, path);
    return ok;
}


bool InvAdd(InvSlot *inv, int itemId, int qty)
{
    if (itemId <= ITEM_NONE || itemId >= gItemCount || qty <= 0) return false;

    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == itemId) { inv[i].qty += qty; return true; }

    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == ITEM_NONE) { inv[i].id = itemId; inv[i].qty = qty; return true; }

    return false;   /* inventario pieno */
}

bool InvRemove(InvSlot *inv, int itemId, int qty)
{
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv[i].id == itemId) {
            if (inv[i].qty < qty) return false;
            inv[i].qty -= qty;
            if (inv[i].qty <= 0) { inv[i].id = ITEM_NONE; inv[i].qty = 0; }
            return true;
        }
    }
    return false;
}

int InvCount(const InvSlot *inv, int itemId)
{
    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv[i].id == itemId) return inv[i].qty;
    return 0;
}

int InvUsedSlots(const InvSlot *inv)
{
    int n = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) if (inv[i].id != ITEM_NONE) n++;
    return n;
}

float InvWeight(const InvSlot *inv)
{
    float wgt = 0.0f;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv[i].id == ITEM_NONE) continue;
        float unit;
        switch (ITEMS[inv[i].id].kind) {
            case IK_WEAPON: unit = 6.0f; break;
            case IK_ARMOR:  unit = 12.0f; break;
            case IK_POTION: unit = 0.5f; break;
            case IK_FOOD:   unit = 0.4f; break;
            default:        unit = 0.2f; break;
        }
        wgt += unit * (float)inv[i].qty;
    }
    return wgt;
}

/* ------------------------------------------------------------------------ */
/*  IDENTIFICATORI STABILI (vedi dataid.h)                                  */
/* ------------------------------------------------------------------------ */

int ItemFind(const char *id)
{
    if (id == NULL) return -1;
    for (int i = 0; i < gItemCount; i++)
        if (strcmp(ITEMS[i].id, id) == 0) return i;
    return -1;
}

unsigned int ItemStableId(int item)
{
    if (item <= ITEM_NONE || item >= gItemCount) return 0u;
    return DataId(ITEMS[item].id);
}

int ItemFromStableId(unsigned int stable)
{
    if (stable == 0u) return ITEM_NONE;
    for (int i = 1; i < gItemCount; i++)
        if (DataId(ITEMS[i].id) == stable) return i;
    return ITEM_NONE;
}
