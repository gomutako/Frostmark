/* ============================================================================
 * tumulo.c - La cripta non e' un oggetto ma una RICETTA: un anello di massi
 * con un varco. Le posizioni le produce CryptRing() dalla posizione del prop, e
 * le leggono in tre - disegno, spinta del giocatore, taglio della camera.
 *
 * Se i tre calcolassero anelli diversi si sbatterebbe contro un masso che non
 * c'e', o si attraverserebbe pietra che si vede. Questa prova fissa le
 * proprieta' che nessuno dei tre puo' violare da solo.
 *
 * Niente OpenGL: l'anello e' geometria pura, in coordinate locali. La quota la
 * mette chi disegna, prendendola dal terreno.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "prova.h"

/* Il giocatore ha raggio 0,35: per passare gli serve almeno il suo diametro. */
#define PASSAGGIO   0.70f

static Prop CriptaA(float x, float z)
{
    Prop p = { 0 };
    p.pos    = (Vector3){ x, 0.0f, z };
    p.scale  = 1.0f;
    p.rot    = 0.0f;
    p.radius = 5.0f;              /* il raggio cotto nel mondo */
    p.type   = PROP_CRYPT;
    return p;
}

/* Distanza libera fra due massi: da centro a centro, meno la pietra dei due. */
static float Luce(const CryptStone *a, const CryptStone *b)
{
    float dx = a->dx - b->dx, dz = a->dz - b->dz;
    return sqrtf(dx * dx + dz * dz) - CryptStoneRadius(a) - CryptStoneRadius(b);
}

int main(void)
{
    CryptStone anello[CRYPT_MAX_MASSI];
    Prop cripta = CriptaA(700.0f, 540.0f);
    int n = CryptRing(&cripta, anello, CRYPT_MAX_MASSI);

    Ok("l'anello ha 17 posti meno i 2 del varco", n == CRYPT_POSTI - CRYPT_VUOTI);

    /* --- Deterministico, e funzione della POSIZIONE --------------------- */
    CryptStone bis[CRYPT_MAX_MASSI];
    int n2 = CryptRing(&cripta, bis, CRYPT_MAX_MASSI);
    int uguali = (n == n2);
    for (int i = 0; i < n && uguali; i++)
        if (anello[i].dx != bis[i].dx || anello[i].dz != bis[i].dz ||
            anello[i].variante != bis[i].variante ||
            anello[i].scala != bis[i].scala || anello[i].yaw != bis[i].yaw ||
            anello[i].affonda != bis[i].affonda) uguali = 0;
    Ok("la stessa cripta da' sempre lo stesso anello", uguali);

    /* Due cripte diverse non devono avere lo stesso ingresso, o il mondo
     * sembrerebbe stampato con lo stampino. */
    CryptStone altra[CRYPT_MAX_MASSI];
    Prop cripta2 = CriptaA(1200.0f, 300.0f);
    int n3 = CryptRing(&cripta2, altra, CRYPT_MAX_MASSI);
    int diverso = (n3 != n);
    for (int i = 0; i < n && i < n3 && !diverso; i++)
        if (anello[i].dx != altra[i].dx || anello[i].variante != altra[i].variante)
            diverso = 1;
    Ok("due posizioni diverse danno anelli diversi", diverso);

    /* --- Il centro e' libero -------------------------------------------- *
     * Vale piu' di tutti gli altri: il boss nasce ESATTAMENTE a cryptPos, e le
     * entita' usano la stessa spinta del giocatore, che a distanza zero non ha
     * una direzione in cui agire. Un masso al centro vuol dire un boss nella
     * roccia, e non lo si scopre finche' non si arriva in fondo alla quest. */
    float piuVicino = 1e9f;
    for (int i = 0; i < n; i++) {
        float d = sqrtf(anello[i].dx * anello[i].dx + anello[i].dz * anello[i].dz);
        float libero = d - CryptStoneRadius(&anello[i]);
        if (libero < piuVicino) piuVicino = libero;
    }
    Ok("nessun masso copre il centro: il boss non nasce nella pietra",
       piuVicino > 0.0f);
    Ok("il vano interno tiene il boss (raggio 0,75) e chi lo combatte",
       piuVicino > 0.75f + 0.35f);

    /* --- Un varco, e uno solo ------------------------------------------- *
     * I massi escono in ordine di posto, quindi i vicini nell'array sono vicini
     * nell'anello, e l'ultimo torna sul primo. */
    int aperture = 0, dovE = -1;
    float varco = 0.0f;
    for (int i = 0; i < n; i++) {
        float l = Luce(&anello[i], &anello[(i + 1) % n]);
        if (l > PASSAGGIO) { aperture++; if (l > varco) { varco = l; dovE = i; } }
    }
    Ok("c'e' esattamente un'apertura attraversabile", aperture == 1);
    Ok("il varco e' largo come una porta doppia", varco > 2.5f && varco < 3.5f);

    /* E l'anello e' CHIUSO altrove: non solo "senza altre aperture larghe", ma
     * senza NESSUNA fessura - i massi devono sovrapporsi. Senza questo controllo
     * un anello tutto buchi passerebbe quello di sopra ogni volta che il buco
     * piu' largo e' uno solo.
     *
     * Si confronta l'indice, non la larghezza: due fessure identiche renderebbero
     * il confronto fra float una moneta. */
    int tocca = 1;
    for (int i = 0; i < n; i++) {
        if (i == dovE) continue;
        if (Luce(&anello[i], &anello[(i + 1) % n]) > 0.0f) tocca = 0;
    }
    Ok("fuori dal varco i massi si sovrappongono", tocca);

    /* --- Le sei forme --------------------------------------------------- *
     * Su una cripta sola il caso puo' non pescarle tutte; su duecento deve. */
    int visto[6] = { 0 };
    int totale = 0;
    for (int k = 0; k < 200; k++) {
        Prop c = CriptaA(100.0f + (float)k * 7.0f, 200.0f + (float)k * 11.0f);
        int m = CryptRing(&c, anello, CRYPT_MAX_MASSI);
        for (int i = 0; i < m; i++) {
            if (anello[i].variante < 0 || anello[i].variante >= 6) { totale = -1; break; }
            visto[anello[i].variante]++;
            totale++;
        }
        if (totale < 0) break;
    }
    Ok("la variante resta dentro l'array", totale > 0);
    int tutte = (totale > 0);
    for (int v = 0; v < 6; v++) if (visto[v] == 0) tutte = 0;
    Ok("su duecento cripte compaiono tutte e sei le forme", tutte);

    int nessunaDomina = (totale > 0);
    for (int v = 0; v < 6; v++) if (visto[v] > totale / 2) nessunaDomina = 0;
    Ok("nessuna forma si prende meta' dei massi", nessunaDomina);

    /* --- La spinta vera ------------------------------------------------- *
     * WorldResolveCollision gira sui chunk ma non tocca la GPU: un chunk si
     * costruisce a mano, e la prova resta senza contesto grafico. */
    static World mondo;
    Prop cr = CriptaA(100.0f, 100.0f);

    mondo.hasTumulo           = true;
    mondo.chunks[0].active    = true;
    mondo.chunks[0].cx        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].cz        = (int)(100.0f / CHUNK_SIZE);
    mondo.chunks[0].propCount = 1;
    mondo.chunks[0].props[0]  = cr;

    /* Dentro il vano non si viene spinti. Il punto e' preso FUORI dal centro
     * esatto, a 1,5 m: sul centro la prova passerebbe anche senza aver scritto
     * niente, perche' il cerchio cotto da 5,0 m a distanza zero non ha una
     * direzione in cui spingere - passerebbe per il motivo sbagliato. A 1,5 m
     * il cerchio cotto spinge fino a 5,35, e il controllo cade davvero finche'
     * il tumulo non e' scritto. */
    Vector3 dentro = { 101.5f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &dentro, 0.35f);
    Ok("dentro il vano non si viene spinti",
       fabsf(dentro.x - 101.5f) < 0.01f && fabsf(dentro.z - 100.0f) < 0.01f);

    /* E sul centro esatto, che e' dove nasce il boss. */
    Vector3 centro = { 100.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &centro, 0.35f);
    Ok("sul centro esatto, dove nasce il boss, non si viene spinti",
       fabsf(centro.x - 100.0f) < 0.01f && fabsf(centro.z - 100.0f) < 0.01f);

    /* Contro un masso si viene spinti fuori DA QUEL MASSO. */
    CryptStone anelloC[CRYPT_MAX_MASSI];
    int nc = CryptRing(&cr, anelloC, CRYPT_MAX_MASSI);
    Ok("l'anello ha prodotto dei massi", nc > 0);

    float mx = 100.0f + anelloC[0].dx, mz = 100.0f + anelloC[0].dz;
    Vector3 addosso = { mx, 0.0f, mz };
    WorldResolveCollision(&mondo, &addosso, 0.35f);
    float dm = sqrtf((addosso.x - mx) * (addosso.x - mx) +
                     (addosso.z - mz) * (addosso.z - mz));
    Ok("dentro un masso si viene spinti fuori da QUEL masso",
       dm > CryptStoneRadius(&anelloC[0]) + 0.35f - 0.01f);

    /* Senza tumulo si torna al raggio cotto nel mondo: 5,0 + 0,35. E' la
     * promessa del ripiego, e va provata o non e' una promessa. */
    mondo.hasTumulo = false;
    Vector3 kit = { 102.0f, 0.0f, 100.0f };
    WorldResolveCollision(&mondo, &kit, 0.35f);
    float dk = sqrtf((kit.x - 100.0f) * (kit.x - 100.0f) +
                     (kit.z - 100.0f) * (kit.z - 100.0f));
    Ok("senza tumulo si torna al raggio cotto",
       fabsf(dk - (5.0f + 0.35f)) < 0.01f);

    return ProveEsito();
}
