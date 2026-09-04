/* ============================================================================
 * luce.c - Il modulo della luce regge DUE programmi, non uno.
 *
 * Il disegno instanziato non puo' condividere il vertex shader con quello
 * normale: DrawMesh() manda la matrice del modello come uniform, il disegno a
 * istanze la manda come attributo di vertice. Due vertex shader, quindi, e uno
 * stesso fragment - la luce deve restare in un posto solo.
 *
 * Il fragment essendo lo stesso file, le stesse uniform devono risolversi su
 * tutti e due i programmi. Ma sono due glProgram distinti, quindi le LOCATION
 * sono diverse: e' l'errore che questa prova esiste per prendere, perche' un
 * uniform impostato sulla location dell'altro programma non da' nessun errore,
 * scrive solo nel posto sbagliato.
 * ========================================================================== */
#include "../../src/light.c"
#include "prova.h"

#include <string.h>
#include <unistd.h>

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova luce");
    if (!IsWindowReady()) {
        printf("niente contesto GL: prova saltata\n");
        return PROVA_SALTATA;
    }

    Ok("LightInit()", LightInit());
    if (!LightReady()) { CloseWindow(); return 1; }

    Shader inst = LightInstShader();
    Ok("il programma instanziato esiste",  inst.id != 0);
    Ok("ed e' diverso da quello normale",  inst.id != gShader.id);

    /* Le uniform del fragment: lo stesso file, quindi devono esserci su
     * entrambi. */
    static const char *comuni[] = {
        "texture0", "texture2", "colDiffuse", "lightDir", "sunAmount",
        "depthOnly", "shadowOn", "shadowRes", "viewPos", "splitDist",
        "lightVP0", "lightVP1", "shadowMap0", "shadowMap1"
    };
    for (int i = 0; i < (int)(sizeof comuni / sizeof *comuni); i++) {
        char msg[96];
        snprintf(msg, sizeof msg, "instanziato: uniform %s", comuni[i]);
        Ok(msg, GetShaderLocation(inst, comuni[i]) != -1);
    }

    /* Quelle che ha in piu': la mvp non esiste, perche' la trasformazione
     * arriva per istanza e vista e proiezione servono separate. */
    Ok("instanziato: uniform matView",
       GetShaderLocation(inst, "matView") != -1);
    Ok("instanziato: uniform matProjection",
       GetShaderLocation(inst, "matProjection") != -1);

    /* E gli attributi d'istanza, agli slot 8 e 9: raylib usa 0-4 per posizione,
     * UV, normale, colore e tangente, 5 per le seconde UV, 6 per gli indici, 7
     * e 8 per le ossa. Sopra l'8 e' terra libera. */
    Ok("instanziato: attributo instPosSin allo slot 8",
       GetShaderLocationAttrib(inst, "instPosSin") == 8);
    Ok("instanziato: attributo instScaleCos allo slot 9",
       GetShaderLocationAttrib(inst, "instScaleCos") == 9);

    /* Il vecchio percorso non deve essersi rotto: e' meta' del gioco. */
    Ok("normale: uniform mvp c'e' ancora",
       GetShaderLocation(gShader, "mvp") != -1);
    Ok("normale: uniform texture2 c'e' ancora",
       GetShaderLocation(gShader, "texture2") != -1);

    CloseWindow();
    return ProveEsito();
}
