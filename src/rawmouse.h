/* ============================================================================
 * rawmouse.h - Movimento grezzo del mouse letto da XInput2 (solo Linux/X11).
 *
 * Serve perche' sotto WSLg la modalita' "cursore bloccato" di GLFW da' scarti
 * inventati: riporta un offset costante - misurato (-210, -100) px per
 * fotogramma a mouse fermo - perche' lo spostamento del puntatore che GLFW usa
 * per ricentrare non viene rispettato da XWayland. Gli eventi RawMotion di
 * XInput2 arrivano invece dal dispositivo, prima di qualsiasi puntatore: warp
 * e accelerazione non li toccano.
 *
 * Nota: vanno chiesti per XIAllDevices. Sui soli master - che e' quel che
 * ascolta GLFW - XWayland non ne consegna nessuno.
 *
 * Questo file non include raylib.h: Xlib.h e raylib.h dichiarano entrambi un
 * tipo "Font" e non stanno nella stessa unita' di compilazione.
 * ========================================================================== */
#ifndef RAWMOUSE_H
#define RAWMOUSE_H

#include <stdbool.h>

/* false se non c'e' X11 o XInput2: il chiamante torna alla via di GLFW. */
bool RawMouseInit(void);
/* Somma degli eventi arrivati dall'ultima chiamata. Va chiamata ogni
 * fotogramma anche quando il risultato si butta, altrimenti la coda cresce e
 * al rientro arriva tutto insieme. */
void RawMouseDelta(float *dx, float *dy);
void RawMouseShutdown(void);

#endif /* RAWMOUSE_H */
