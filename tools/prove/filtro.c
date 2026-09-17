/* ============================================================================
 * filtro.c - Il campionamento delle texture dei prop: a punti per le
 * tavolozze dei kit, con i mipmap per le mappe fotografiche.
 *
 * Perche' merita una prova. La regola e' una riga di codice e un difetto
 * VISIVO: senza mipmap una chioma fotografata formicola, e chi guarda il log
 * non vede niente di strano - il modello si carica, le texture pure. L'unico
 * modo di accorgersene e' guardare il gioco in movimento, ed e' esattamente
 * cio' che una prova puo' evitare di rifare ogni volta.
 *
 * Cosa NON prova: che l'immagine sia piu' bella. Il filtro impostato con
 * SetTextureFilter() non si rilegge da Texture2D - raylib non lo conserva -
 * quindi qui si controlla cio' che resta leggibile, cioe' la CATENA DI MIPMAP:
 * generata sopra la soglia, assente sotto. E' la meta' che decide il difetto.
 * ========================================================================== */
#include "../../src/meshgroup.c"
#include "../../src/world.c"
#include "prova.h"

#include <unistd.h>

static Texture2D Tinta(int lato)
{
    Image im = GenImageColor(lato, lato, (Color){ 120, 90, 60, 255 });
    Texture2D t = LoadTextureFromImage(im);
    UnloadImage(im);
    return t;
}

int main(void)
{
    if (access("/dev/dxg", F_OK) == 0) setenv("GALLIUM_DRIVER", "d3d12", 0);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "prova filtro");
    if (!IsWindowReady()) {
        printf("niente contesto GL: prova saltata\n");
        return PROVA_SALTATA;
    }

    /* La tavolozza dei kit: 512 e' la piu' grande che c'e' in gioco
     * (assets/models/Textures/colormap.png di Kenney). Resta senza mipmap. */
    Texture2D tavolozza = Tinta(512);
    FiltroDiProp(&tavolozza);
    Ok("una tavolozza da 512 non prende i mipmap", tavolozza.mipmaps == 1);

    /* Una mappa di Poly Haven: 1k, e i mipmap li vuole. */
    Texture2D foto = Tinta(1024);
    FiltroDiProp(&foto);
    Ok("una mappa da 1024 prende la catena di mipmap", foto.mipmaps > 1);
    Ok("la catena arriva fino al texel singolo",       foto.mipmaps == 11);

    /* Chiamarla due volte non rigenera niente: LoadExtProps la chiama su ogni
     * mappa di ogni materiale, e i modelli condividono le texture. */
    int prima = foto.mipmaps;
    FiltroDiProp(&foto);
    Ok("due chiamate lasciano la stessa catena", foto.mipmaps == prima);

    /* Una mappa che non c'e' - lo slot della normale di un materiale senza
     * rilievo - ha id 0 e non si tocca: GenTextureMipmaps() su quella
     * lavorerebbe sulla texture bianca 1x1 predefinita di raylib, che e'
     * CONDIVISA da tutto il gioco. */
    Texture2D vuota = { 0 };
    FiltroDiProp(&vuota);
    Ok("una mappa assente resta assente", vuota.id == 0 && vuota.mipmaps == 0);

    UnloadTexture(tavolozza);
    UnloadTexture(foto);
    CloseWindow();
    return ProveEsito();
}
