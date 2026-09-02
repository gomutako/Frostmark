#include "balance.h"
#include "config.h"
#include "dataparse.h"
#include "raylib.h"
#include <string.h>

Balance BAL;

/* Ogni campo con il suo nome nel file e l'intervallo ammesso. Tenerli in una
 * tabella evita una catena di confronti e fa si' che un campo dimenticato
 * venga segnalato senza scrivere due volte lo stesso controllo. */
typedef struct {
    const char *key;
    float      *slot;      /* uno dei due e' NULL */
    int        *islot;
    float       lo, hi;
} Field;

static const Field FIELDS[] = {
    { "camminata",          &BAL.walk,              NULL, 0.1f, 100.0f },
    { "corsa",              &BAL.run,               NULL, 0.1f, 100.0f },
    { "salto",              &BAL.jump,              NULL, 0.0f, 100.0f },
    { "gravita",            &BAL.gravity,           NULL, 0.1f, 200.0f },
    { "altezza_occhi",      &BAL.eyeHeight,         NULL, 0.2f,  10.0f },
    { "altezza_corpo",      &BAL.bodyHeight,        NULL, 0.2f,  10.0f },
    { "raggio",             &BAL.radius,            NULL, 0.05f,  5.0f },
    { "parata_velocita",    &BAL.blockSpeedMul,     NULL, 0.0f,   1.0f },
    { "parata_danno",       &BAL.blockDamageMul,    NULL, 0.0f,   1.0f },
    { "parata_vigore",      &BAL.blockStaminaDrain, NULL, 0.0f, 100.0f },
    { "vigore_corsa",       &BAL.staminaSprint,     NULL, 0.0f, 100.0f },
    { "vigore_recupero",    &BAL.staminaRegen,      NULL, 0.0f, 100.0f },
    { "vigore_salto",       &BAL.staminaJump,       NULL, 0.0f, 100.0f },
    { "magia_recupero",     &BAL.manaRegen,         NULL, 0.0f, 100.0f },
    { "vita_recupero",      &BAL.healthRegen,       NULL, 0.0f, 100.0f },
    { "mouse_sensibilita",  &BAL.mouseSens,         NULL, 0.0001f, 0.05f },
    { "caduta_soglia",      &BAL.fallThreshold,     NULL, 0.0f, 200.0f },
    { "caduta_fattore",     &BAL.fallFactor,        NULL, 0.0f, 100.0f },
    { "vita_livello",       &BAL.hpPerLevel,        NULL, 0.0f, 1000.0f },
    { "vigore_livello",     &BAL.staPerLevel,       NULL, 0.0f, 1000.0f },
    { "magia_livello",      &BAL.mpPerLevel,        NULL, 0.0f, 1000.0f },
    { "giorno_secondi",     &BAL.daySeconds,        NULL, 1.0f, 100000.0f },
    { "esperienza_base",    NULL, &BAL.xpBase,            1.0f, 1000000.0f },
    { "esperienza_livello", NULL, &BAL.xpPerLevel,        0.0f, 1000000.0f },
};
#define FIELD_COUNT ((int)(sizeof(FIELDS) / sizeof(FIELDS[0])))

bool BalanceLoad(const char *path)
{
    memset(&BAL, 0, sizeof(BAL));
    bool seen[FIELD_COUNT];
    memset(seen, 0, sizeof(seen));

    DataReader r;
    if (!DataOpen(&r, path)) return false;
    int before = DataProblemCount();

    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "bilanciamento") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        char *key, *val;
        while (DataNextField(&r, &key, &val)) {
            int f = -1;
            for (int i = 0; i < FIELD_COUNT; i++)
                if (strcmp(FIELDS[i].key, key) == 0) { f = i; break; }
            if (f < 0) {
                DataProblem(&r, "chiave \"%s\" sconosciuta nel bilanciamento", key);
                continue;
            }
            if (seen[f]) { DataProblem(&r, "\"%s\" indicato due volte", key); continue; }

            bool ok;
            if (FIELDS[f].slot)
                ok = DataAsFloat(&r, key, val, FIELDS[f].lo, FIELDS[f].hi, FIELDS[f].slot);
            else
                ok = DataAsInt(&r, key, val, (int)FIELDS[f].lo, (int)FIELDS[f].hi,
                               FIELDS[f].islot);
            if (ok) seen[f] = true;
        }
    }
    DataClose(&r);

    /* Nessun valore di ripiego: ogni campo va indicato. */
    for (int i = 0; i < FIELD_COUNT; i++)
        if (!seen[i]) DataProblem(NULL, "%s: manca \"%s\"", path, FIELDS[i].key);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d valori di bilanciamento da %s", FIELD_COUNT, path);
    return ok;
}
