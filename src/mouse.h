/* ============================================================================
 * mouse.h - Presa del mouse per la visuale.
 *
 * Esiste come modulo a se' perche' la modalita' "cursore bloccato" di GLFW
 * (DisableCursor) sotto WSLg non funziona: il puntatore non viene davvero
 * riportato al centro e ogni fotogramma torna lo stesso scarto - misurato
 * (-272, +337) px a mouse fermo, cioe' 0,7 radianti di rotazione per
 * fotogramma. Li' ci si prende in carico il ricentraggio.
 * ========================================================================== */
#ifndef MOUSE_H
#define MOUSE_H

#include "raylib.h"

/* Presa: da chiamare quando si entra nel gioco vero e proprio. */
void    MouseLookBegin(void);
/* Rilascio: menu, inventario, dialoghi. */
void    MouseLookEnd(void);
/* Scarto del fotogramma corrente, in pixel. Va chiamata una volta per
 * fotogramma: sulla via WSL e' lei a rimettere il puntatore al centro. */
Vector2 MouseLookDelta(void);
/* Chiude la connessione a X11 usata dal movimento grezzo. */
void    MouseLookShutdown(void);
/* Nome della via in uso: finisce nell'avviso all'avvio. */
const char *MouseLookModeName(void);

#endif /* MOUSE_H */
