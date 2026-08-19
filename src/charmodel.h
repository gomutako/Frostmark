/* ============================================================================
 * charmodel.h - Modello di personaggio animato, condiviso da giocatore e NPC.
 *
 * Incapsula le tre cose che raylib non fa da se' con i pacchetti di personaggi
 * CC0 (KayKit, Quaternius, ...):
 *
 *   1. trovare le animazioni per NOME e non per indice, cosi' pacchetti
 *      diversi funzionano senza toccare il codice;
 *   2. animare solo le mesh dotate di pesi - UpdateModelAnimation() di raylib
 *      5.5 crolla sui modelli che mescolano mesh skinnate e non;
 *   3. muovere le mesh senza pesi (arma, scudo, elmo) seguendo un osso.
 *
 * Un CharModel e' una risorsa condivisibile: la posa NON sta qui dentro, la
 * tiene chi disegna. Lo stesso modello serve quindi molti personaggi, ognuno
 * con la sua animazione, rideformandolo dentro il ciclo di disegno.
 * Vedi docs/03-asset-pubblici.md.
 * ========================================================================== */
#ifndef CHARMODEL_H
#define CHARMODEL_H

#include "raylib.h"
#include "config.h"
#include <stdbool.h>

/* Ruoli di animazione. Un pacchetto ne offre spesso decine (il cavaliere
 * KayKit ne ha 76): al gioco servono questi. */
typedef enum {
    CANIM_IDLE, CANIM_WALK, CANIM_RUN, CANIM_ATTACK, CANIM_BLOCK,
    CANIM_CAST, CANIM_JUMP, CANIM_HURT, CANIM_DEATH, CANIM_COUNT
} CharAnim;

typedef struct {
    bool   loaded;
    Model  model;
    /* Vista con le sole mesh dotate di scheletro: condivide i dati con
     * 'model', esiste solo per non far crollare UpdateModelAnimation(). */
    Model  skinnedView;
    bool   viewAllocated, anySkinned;

    ModelAnimation *anims;
    int    animCount;
    int    animOf[CANIM_COUNT];        /* indice in anims, -1 se assente */

    /* Mesh senza pesi da muovere a mano seguendo un osso. */
    struct { int mesh, bone; } attach[MAX_ATTACHMENTS];
    int    attachCount;

    float  scale;                      /* porta il modello all'altezza voluta */
} CharModel;

/* Carica il modello e il file degli agganci che gli sta accanto (stesso nome,
 * estensione .attach). 'heightMeters' e' l'altezza voluta del personaggio: la
 * scala viene ricavata misurando il modello, non va indovinata.
 * Ritorna false se il file non c'e' o non e' leggibile: in quel caso chi
 * chiama disegna le primitive di sempre. */
bool CharModelLoad(CharModel *c, const char *glbPath, float heightMeters);
void CharModelUnload(CharModel *c);

/* Numero di fotogrammi del ruolo, 0 se il ruolo manca nel pacchetto. */
int  CharModelFrames(const CharModel *c, CharAnim a);
/* Il ruolo piu' vicino disponibile: corsa -> camminata -> riposo. */
CharAnim CharModelResolve(const CharModel *c, CharAnim want);
/* Le clip di questi ruoli si ripetono, le altre restano sull'ultima posa. */
bool CharAnimLoops(CharAnim a);

/* Fa avanzare il fotogramma: ripete le clip ciclabili, ferma le altre
 * sull'ultima posa. 'speedMul' allunga o accorcia il passo (1.0 = 60 fps, la
 * frequenza a cui raylib ricampiona le animazioni glTF); 'oneShotSeconds' e' la
 * durata voluta per le clip non ciclabili, che spesso sono piu' lunghe
 * dell'azione di gioco a cui corrispondono. */
float CharModelAdvance(const CharModel *c, CharAnim a, float frame, float dt,
                       float speedMul, float oneShotSeconds);

/* Deforma il modello nella posa richiesta e lo disegna. Va chiamata una volta
 * per personaggio, dentro il ciclo di disegno: la deformazione vale fino alla
 * chiamata successiva. */
void CharModelDraw(const CharModel *c, Vector3 pos, float yawRad,
                   CharAnim a, float frame, Color tint);

#endif /* CHARMODEL_H */
