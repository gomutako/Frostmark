/* ============================================================================
 * scale.c - Le scale dentro gli edifici sono un volume, non solo una quota.
 *
 * Finche' la rampa e' esistita solo come superficie calpestabile la si
 * attraversava: di fianco e dall'alto sta piu' in alto di un gradino, e
 * WorldSupportHeight() la scartava. Questa prova fissa il confine fra cio' che
 * deve respingere e cio' che deve restare aperto.
 *
 * Include world.c perche' ResolveHouse() e' static, e giustamente: e' un
 * dettaglio della collisione, non un'API.
 * ========================================================================== */
#include "../../src/world.c"
#include "prova.h"

static Prop CasaAlta(void)
{
    Prop p;
    memset(&p, 0, sizeof p);
    p.type = PROP_HOUSE;
    p.scale = 1.0f;
    p.rot = 0.0f;

    /* La forma e' una funzione della posizione: si cerca una posizione che
     * dia una casa a due piani, che e' l'unica ad avere una scala. */
    for (int i = 0; i < 4000; i++) {
        p.pos = (Vector3){ (float)i * 0.25f, 0.0f, 7.0f };
        if (WorldHouseFloors(&p) == 2) return p;
    }
    printf("nessuna casa a due piani trovata: la prova non ha soggetto\n");
    exit(1);
}

/* Mette il giocatore in un punto, in celle rispetto al centro della casa, e
 * guarda se la collisione lo sposta. */
static void Prova(const Prop *p, const char *cosa, float lx, float lz, float y,
                  int respintoAtteso)
{
    float cell = BUILD_CELL * p->scale;
    Vector3 pos = { p->pos.x + lx * cell, p->pos.y + y, p->pos.z + lz * cell };
    Vector3 prima = pos;

    ResolveHouse(p, &pos, 0.40f);

    float spostato = hypotf(pos.x - prima.x, pos.z - prima.z);
    int respinto = spostato > 1e-4f;
    printf("%-46s spostato %.3f m -> %-9s %s\n", cosa, (double)spostato,
           respinto ? "RESPINTO" : "passa",
           respinto == respintoAtteso ? "ok" : "FALLITO");
    if (respinto != respintoAtteso) gProveFallite++;
}

int main(void)
{
    Prop p = CasaAlta();
    HouseShape sh = HouseShapeOf(&p);
    float cx = (float)sh.stairX - (sh.nx - 1) / 2.0f;   /* centro della cella */
    float cz = (float)sh.stairZ - (sh.nz - 1) / 2.0f;   /* della scala, in celle */

    printf("casa %dx%d, %d piani, cella della scala al centro (%.2f, %.2f)\n\n",
           sh.nx, sh.nz, sh.floors, (double)cx, (double)cz);

    /* Al piano terra la scala deve essere solida da sopra e di fianco. */
    Prova(&p, "terra, entra dall'alto della rampa (+X)", cx + 0.35f, cz, 0.0f, 1);
    Prova(&p, "terra, entra di fianco (-Z)",             cx, cz - 0.45f, 0.0f, 1);
    Prova(&p, "terra, in mezzo alla rampa",              cx, cz,         0.0f, 1);

    /* Il piede della rampa resta aperto: e' da li' che si sale. */
    Prova(&p, "terra, piede della rampa (-X)",           cx - 0.45f, cz, 0.0f, 0);

    /* Chi ci sta gia' sopra ha i piedi alla quota giusta e non va toccato. */
    Prova(&p, "gia' sulla rampa a meta'",       cx, cz, 0.5f * BUILD_CELL, 0);

    /* E dal piano di sopra la tromba resta libera per scendere. */
    Prova(&p, "primo piano, sopra la tromba",   cx, cz, BUILD_CELL, 0);

    return ProveEsito();
}
