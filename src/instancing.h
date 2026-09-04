/* ============================================================================
 * instancing.h - Disegnare la stessa mesh molte volte con una chiamata sola.
 *
 * Misurato PRIMA di scrivere questo modulo, su un percorso di 75 secondi: il
 * passaggio principale costava ~4,4 microsecondi per chiamata di disegno e
 * quello d'ombra ~9, con mesh da un centinaio di triangoli. Non erano i
 * triangoli, erano le chiamate - e la controprova e' che abbassare la
 * risoluzione delle mappe d'ombra da 2048 a 1024, cioe' un quarto del
 * riempimento, non cambiava il tempo di un decimo di millisecondo. Un lotto
 * sostituisce tutte quelle chiamate con una.
 *
 * Un lotto tiene un VAO PROPRIO, che punta ai VBO della mesh piu' il suo
 * buffer d'istanza. Il VAO della mesh non si tocca, ed e' deliberato:
 * DrawMeshInstanced() di raylib attacca gli attributi d'istanza al VAO della
 * mesh e alla fine cancella il buffer SENZA spegnere i divisor, per cui la
 * stessa mesh disegnata poi senza istanze legge un buffer che non esiste piu'.
 * Qui i due percorsi non si vedono nemmeno, e i prop si disegnano tre volte per
 * fotogramma - principale piu' due cascate d'ombra - quindi il caso non e'
 * teorico.
 *
 * Il buffer d'istanza e' persistente: cresce per raddoppi e non viene
 * distrutto per fotogramma. E' l'altra ragione per non usare l'API di raylib,
 * che alloca e libera un VBO a ogni chiamata.
 * ========================================================================== */
#ifndef INSTANCING_H
#define INSTANCING_H

#include "raylib.h"

typedef struct InstBatch InstBatch;

/* Crea un lotto per una mesh e un materiale. Il materiale viene COPIATO e gli
 * si monta lo shader instanziato: il chiamante continua a usare il suo, con lo
 * shader normale, per il percorso non instanziato.
 *
 * Ritorna NULL se lo shader instanziato non c'e' o la mesh non e' sulla GPU.
 * Non e' un errore: il chiamante disegna come prima, che e' quel che deve
 * succedere quando assets/shaders/ manca. */
InstBatch *InstCreate(Mesh mesh, Material mat);
void       InstFree(InstBatch *b);

/* Un giro completo per PASSAGGIO, non per fotogramma: il passaggio principale
 * culla a cono e quello d'ombra a raggio, quindi le liste sono diverse e vanno
 * rifatte da capo. */
void InstBegin(InstBatch *b);

/* L'imbardata si passa in GRADI, come la porta il Prop e come la vuole
 * DrawModelEx. Seno e coseno si calcolano qui, una volta per istanza: e' il
 * motivo per cui il dato d'istanza li porta gia' fatti invece dell'angolo. */
void InstAdd(InstBatch *b, Vector3 pos, float yawDeg, Vector3 scale);

void InstFlush(InstBatch *b);

/* Il colore che moltiplica l'albedo: e' la tinta del ciclo giorno/notte, ed e'
 * per LOTTO, non per istanza. */
void InstTint(InstBatch *b, Color tint);

#endif /* INSTANCING_H */
