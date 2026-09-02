/* ============================================================================
 * light.h - Sole, ombre e lo shader che le applica.
 *
 * Prima il gioco non aveva luce: il terreno portava un'illuminazione cotta nei
 * colori dei vertici, e tutto il resto veniva moltiplicato per la tinta del
 * ciclo giorno/notte. Nessuna faccia era piu' chiara di un'altra, e niente
 * proiettava ombra.
 *
 * Ora c'e' una luce direzionale - il sole - e una mappa di profondita' vista
 * dal sole, ristretta a un raggio attorno al giocatore: le ombre si vedono
 * dove si guarda, e il passaggio in piu' costa poco perche' disegna solo cio'
 * che sta vicino.
 *
 * Se assets/shaders/ manca, tutto continua a funzionare senza luce: come per i
 * modelli, l'assenza di un file non e' un errore.
 * ========================================================================== */
#ifndef LIGHT_H
#define LIGHT_H

#include "raylib.h"
#include <stdbool.h>

bool LightInit(void);
void LightUnload(void);
bool LightReady(void);

/* Da applicare a ogni materiale che deve ricevere luce: terreno, prop,
 * personaggi. Senza, l'oggetto resta piatto come prima. */
void LightApplyToMaterial(Material *m);
void LightApplyToModel(Model *m);

/* Direzione verso il sole e quanta luce da', dal ciclo giorno/notte. */
void LightSetSun(Vector3 dirToSun, float amount);

/* Il passaggio d'ombra: fra queste due si disegna la scena che proietta.
 * 'center' e' il punto attorno a cui si concentra la mappa - il giocatore. */
void LightShadowBegin(Vector3 base, int cascade);
/* Dove finisce davvero il quadrato della mappa: spostato verso il sole rispetto
 * al giocatore, perche' gli occlusori stanno da quella parte. Serve a chi
 * decide quali oggetti disegnare nel passaggio. */
Vector3 LightShadowCenter(Vector3 base, int cascade);
void LightShadowEnd(void);

/* Uniform che cambiano a ogni fotogramma: da chiamare prima di disegnare. */
void LightFrame(Camera3D cam);

/* Le mappe sono due: una stretta e nitida attorno al giocatore, una larga per
 * il resto. Chi disegna il passaggio d'ombra le percorre entrambe. */
int   LightCascades(void);
float LightShadowRadius(int cascade);

#endif /* LIGHT_H */
