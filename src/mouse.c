#include "mouse.h"
#include "rawmouse.h"
#include <stdlib.h>

/* Tre vie possibili, in ordine di preferenza:
 *
 *   RAW    - presa e cursore nascosto li fa GLFW, ma lo scarto lo leggiamo da
 *            XInput2. E' quel che serve sotto WSLg, dove la modalita' di GLFW
 *            riporta un offset costante (misurato -210,-100 px per fotogramma
 *            a mouse fermo) perche' XWayland non rispetta il ricentraggio.
 *   GLFW   - DisableCursor() e GetMouseDelta(): la via normale, giusta ovunque
 *            il puntatore si lasci spostare.
 *   MANUAL - cursore nascosto e ricentraggio a mano. Ultima spiaggia: se il
 *            warp non funziona nemmeno questa e' affidabile, ma almeno non
 *            dipende da XInput2.
 */
typedef enum { LOOK_GLFW, LOOK_RAW, LOOK_MANUAL } LookMode;

static LookMode mode = LOOK_GLFW;
static bool     grabbed = false;

static bool RunningUnderWSL(void)
{
    return getenv("WSL_DISTRO_NAME") != NULL || getenv("WSL_INTEROP") != NULL;
}

static void Center(int *cx, int *cy)
{
    *cx = GetScreenWidth()  / 2;
    *cy = GetScreenHeight() / 2;
}

void MouseLookBegin(void)
{
    static bool chosen = false;
    if (!chosen) {
        chosen = true;
        if (RunningUnderWSL()) {
            mode = RawMouseInit() ? LOOK_RAW : LOOK_MANUAL;
            /* Nessuna delle due vie e' buona: sotto WSLg il puntatore e' un
             * dispositivo assoluto, tenuto dentro la finestra satura contro il
             * bordo e lasciato libero smette di consegnare eventi. Il gioco
             * gira, ma la visuale col mouse resta zoppa: si dice, invece di
             * far cercare all'utente un difetto che e' sotto di noi. */
            TraceLog(LOG_WARNING,
                     "MOUSE: via '%s'. Sotto WSL non c'e' movimento relativo "
                     "del mouse: la visuale sara' scattosa. Per giocare "
                     "davvero: make windows && ./frostmark.exe",
                     MouseLookModeName());
        }
    }

    grabbed = true;

    if (mode == LOOK_MANUAL) {
        HideCursor();
        int cx, cy; Center(&cx, &cy);
        SetMousePosition(cx, cy);
    } else if (mode == LOOK_RAW) {
        /* La presa serve: XWayland consegna input solo finche' il puntatore
         * sta sopra la finestra, e senza presa al primo movimento esce e non
         * arriva piu' niente (misurato). I riposizionamenti che GLFW fa per
         * tenerlo dentro sporcano solo le copie del puntatore virtuale, che
         * rawmouse.c scarta: le copie del dispositivo restano esatte. */
        DisableCursor();
    } else {
        DisableCursor();
    }

    if (mode == LOOK_RAW) {           /* scarta l'arretrato dei menu */
        float dx, dy;
        RawMouseDelta(&dx, &dy);
    }
}

void MouseLookEnd(void)
{
    grabbed = false;
    if (mode == LOOK_MANUAL) ShowCursor();
    else                     EnableCursor();
}

Vector2 MouseLookDelta(void)
{
    Vector2 zero = { 0.0f, 0.0f };

    if (mode == LOOK_RAW) {
        float dx, dy;
        RawMouseDelta(&dx, &dy);      /* sempre, anche per buttarli: la coda
                                         altrimenti si scarica tutta insieme */
        if (!grabbed || !IsWindowFocused()) return zero;
        return (Vector2){ dx, dy };
    }

    if (mode == LOOK_MANUAL) {
        if (!grabbed || !IsWindowFocused()) return zero;
        int cx, cy; Center(&cx, &cy);
        Vector2 p = GetMousePosition();
        Vector2 d = { p.x - (float)cx, p.y - (float)cy };
        SetMousePosition(cx, cy);
        return d;
    }

    return GetMouseDelta();
}

void MouseLookShutdown(void)
{
    RawMouseShutdown();
}

const char *MouseLookModeName(void)
{
    switch (mode) {
        case LOOK_RAW:    return "raw-xinput2";
        case LOOK_MANUAL: return "ricentraggio-manuale";
        default:          return "glfw";
    }
}
