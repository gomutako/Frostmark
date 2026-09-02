#include "rawmouse.h"

#if defined(__linux__) && !defined(__ANDROID__) && \
    (!defined(__has_include) || __has_include(<X11/extensions/XInput2.h>))

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <dlfcn.h>
#include <string.h>

/* Connessione tutta nostra: quella di GLFW non si tocca. Gli eventi raw
 * vengono consegnati a ogni client che li chiede, quindi le due convivono. */
static Display *dpy;
static int      xiOpcode;

/* Ogni movimento puo' arrivare due volte: dal dispositivo vero
 * (deviceid == sourceid) e dal puntatore virtuale che lo rappresenta
 * (deviceid = master). Tenerli entrambi fa girare la visuale il doppio,
 * scartare a priori quelli del master la fa stare ferma dove - come sotto
 * WSLg col mouse fisico - la copia del dispositivo non arriva mai.
 * Quindi si guarda cosa arriva davvero: appena si vede una copia del
 * dispositivo, da li' in poi si ignorano quelle del puntatore. */

static int (*pXIQueryVersion)(Display *, int *, int *);
static int (*pXISelectEvents)(Display *, Window, XIEventMask *, int);
static XIDeviceInfo *(*pXIQueryDevice)(Display *, int, int *);
static void (*pXIFreeDeviceInfo)(XIDeviceInfo *);

/* Un valuatore puo' riportare lo spostamento (XIModeRelative, un mouse
 * normale) oppure la posizione (XIModeAbsolute, ed e' il caso del puntatore
 * di WSLg, che viene da Wayland). Nel secondo caso il valore da solo non
 * significa niente: conta la differenza con l'evento precedente. Il modo si
 * chiede al dispositivo la prima volta che lo si incontra. */
#define MAX_TRACKED 8
typedef struct {
    int    id;
    bool   absolute[2];
    bool   have[2];
    double last[2];
} Device;

static Device devs[MAX_TRACKED];
static int    devCount;

static Device *LookupDevice(int id)
{
    for (int i = 0; i < devCount; i++)
        if (devs[i].id == id) return &devs[i];
    if (devCount >= MAX_TRACKED || !pXIQueryDevice) return NULL;

    Device *d = &devs[devCount++];
    d->id = id;
    d->absolute[0] = d->absolute[1] = false;
    d->have[0] = d->have[1] = false;

    int n = 0;
    XIDeviceInfo *info = pXIQueryDevice(dpy, id, &n);
    if (info) {
        for (int c = 0; c < info->num_classes; c++) {
            if (info->classes[c]->type != XIValuatorClass) continue;
            XIValuatorClassInfo *v = (XIValuatorClassInfo *)info->classes[c];
            if (v->number == 0 || v->number == 1)
                d->absolute[v->number] = (v->mode == XIModeAbsolute);
        }
        if (pXIFreeDeviceInfo) pXIFreeDeviceInfo(info);
    }
    return d;
}

bool RawMouseInit(void)
{
    if (dpy) return true;

    /* libXi si carica a runtime: cosi' il gioco si compila e gira anche dove
     * non c'e'. */
    void *xi = dlopen("libXi.so.6", RTLD_NOW);
    if (!xi) return false;
    pXIQueryVersion   = (int (*)(Display *, int *, int *))dlsym(xi, "XIQueryVersion");
    pXISelectEvents   = (int (*)(Display *, Window, XIEventMask *, int))dlsym(xi, "XISelectEvents");
    pXIQueryDevice    = (XIDeviceInfo *(*)(Display *, int, int *))dlsym(xi, "XIQueryDevice");
    pXIFreeDeviceInfo = (void (*)(XIDeviceInfo *))dlsym(xi, "XIFreeDeviceInfo");
    if (!pXIQueryVersion || !pXISelectEvents) return false;

    dpy = XOpenDisplay(NULL);
    if (!dpy) return false;

    int event, error;
    if (!XQueryExtension(dpy, "XInputExtension", &xiOpcode, &event, &error)) goto fail;

    int major = 2, minor = 0;
    if (pXIQueryVersion(dpy, &major, &minor) != Success) goto fail;

    unsigned char mask[XIMaskLen(XI_LASTEVENT)];
    memset(mask, 0, sizeof(mask));
    XISetMask(mask, XI_RawMotion);
    XIEventMask em = { .deviceid = XIAllDevices,   /* non XIAllMasterDevices */
                       .mask_len = sizeof(mask),
                       .mask     = mask };
    pXISelectEvents(dpy, DefaultRootWindow(dpy), &em, 1);
    XFlush(dpy);
    return true;

fail:
    XCloseDisplay(dpy);
    dpy = NULL;
    return false;
}

void RawMouseDelta(float *dx, float *dy)
{
    *dx = 0.0f; *dy = 0.0f;
    if (!dpy) return;

    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.xcookie.type != GenericEvent || ev.xcookie.extension != xiOpcode) continue;
        if (!XGetEventData(dpy, &ev.xcookie)) continue;

        if (ev.xcookie.evtype == XI_RawMotion) {
            XIRawEvent *re = (XIRawEvent *)ev.xcookie.data;

            bool fromDevice = (re->deviceid == re->sourceid);

            /* Le copie del puntatore virtuale portano valori inventati -
             * misurati oltre un milione di conteggi in dieci secondi contro i
             * 600 veri - perche' ci finisce dentro ogni riposizionamento del
             * puntatore. Si tengono solo quelle del dispositivo. */
            if (!fromDevice) {
                XFreeEventData(dpy, &ev.xcookie);
                continue;
            }
            /* raw_values elenca solo i valuatori presenti nella maschera: si
             * avanza di uno a ogni bit acceso, non a ogni indice. */
            Device *d = LookupDevice(re->deviceid);
            const double *v = re->raw_values;
            int n = re->valuators.mask_len * 8;
            for (int i = 0; i < n; i++) {
                if (!XIMaskIsSet(re->valuators.mask, i)) continue;
                double value = *v++;
                if (i > 1) continue;

                double step = value;
                if (d && d->absolute[i]) {
                    /* Posizione, non spostamento: il primo campione serve solo
                     * a fissare il riferimento. */
                    step = d->have[i] ? value - d->last[i] : 0.0;
                    d->last[i] = value;
                    d->have[i] = true;
                }
                if (i == 0) *dx += (float)step;
                else        *dy += (float)step;
            }
        }
        XFreeEventData(dpy, &ev.xcookie);
    }
}

void RawMouseShutdown(void)
{
    if (dpy) { XCloseDisplay(dpy); dpy = NULL; }
}

#else   /* niente X11: il chiamante usa la via di GLFW */

bool RawMouseInit(void) { return false; }
void RawMouseDelta(float *dx, float *dy) { *dx = 0.0f; *dy = 0.0f; }
void RawMouseShutdown(void) { }

#endif
