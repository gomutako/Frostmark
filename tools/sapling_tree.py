#!/usr/bin/env python3
"""Genera un albero con Sapling e lo esporta in glTF. Gira DENTRO Blender.

    blender --background --python tools/sapling_tree.py -- \
            <preset> '<override json>' [uscita.glb] [cartella texture]

Esempio, l'abete che sta in gioco - schede di fronda, corteccia fotografata,
1.450 schede e 10.670 vertici nel file:

    blender --background --python tools/sapling_tree.py -- douglas_fir \
      '{"resU":1,"bevelRes":0,"curveRes":[5,3,2,2],"levels":3,
        "branches":[0,45,12,8],"showLeaves":true,"leaves":16,
        "leafShape":"rect","leafScale":5.0,"leafScaleX":0.6,"leafangle":-35}' \
      assets/models/abete.glb assets/textures/abete

Le texture le scarica ./tools/fetch_assets.sh abete: tre megabyte di mappe del
pino, SENZA i 958 MB della sua geometria. Senza la cartella l'albero esce nudo,
com'era prima, e si vede grigio.

PERCHE' UNO SCRIPT E NON UN .GLB NEL REPOSITORY. E' la stessa scelta di
fetch_assets.sh: si versiona il modo di ottenere l'asset, non l'asset. Qui in
piu' c'e' una ragione forte, ed e' il motivo per cui questo file esiste: il
conteggio dei vertici e' un PARAMETRO, non un dato subito dal catalogo. Chi
riprende puo' rigenerare lo stesso albero piu' povero o piu' ricco cambiando un
numero, e il banco dice quanto costa.

COSA SERVE, E NON STA NEL REPOSITORY:

  1. Blender. Dalla 4.2 gli addon di serie sono passati alla piattaforma
     'extensions' e Sapling NON e' stato ripubblicato: nel pacchetto non c'e'
     piu'. Provato il 16 settembre 2026 sulla 5.0.1 scaricata da blender.org
     ed estratta in ~/opt, senza sudo e senza pacchetti di sistema.

  2. Sapling, ripreso dal sorgente GPL, che e' ancora online. Due file piu' i
     preset, dentro la cartella degli addon della versione che si usa:

       SAP=~/.config/blender/5.0/scripts/addons/add_curve_sapling
       mkdir -p "$SAP/presets"
       base=https://raw.githubusercontent.com/blender/blender-addons/main/add_curve_sapling
       curl -sfL -o "$SAP/__init__.py" "$base/__init__.py"
       curl -sfL -o "$SAP/utils.py"    "$base/utils.py"
       # e i nove preset da "$base/presets/<nome>.py"

     I preset sono callistemon, douglas_fir, japanese_maple, quaking_aspen,
     small_maple, small_pine, weeping_willow, white_birch, willow.

LA LICENZA. Sapling e' GPL-2.0-or-later, come Blender. Questo NON tinge la mesh
prodotta: l'uscita di uno strumento e' di chi la genera, ed e' lo stesso
ragionamento gia' agli atti in docs/06 per MakeHuman/MPFB. Blender stesso e' GPL
e l'industria ci spedisce asset commerciali da vent'anni.

LE MANOPOLE CHE CONTANO, misurate su douglas_fir il 16 settembre:

    levels 2                                        400 vertici
    resU 1, bevelRes 0, curveRes 5/3/2/2, levels 3
      branches 0/25/8/4                            1.788
      branches 0/40/10/6                           3.208
      ...con showLeaves e leaves 10                7.624
    branches 0/60/20/12, leaves 40                58.940
    default del preset                            231.528   (oltre il tetto)

Il tetto e' 65.535 vertici PER PRIMITIVA: oltre, raylib tronca gli indici e
LoadExtProps() scarta il modello. Vedi docs/06, vincolo 1.

E LA COSA CHE I NUMERI NON DICONO: il budget di vertici si compra SVUOTANDO LA
CHIOMA. A 7.624 vertici l'abete ha la forma giusta ed e' spoglio come a
febbraio, perche' Sapling spende geometria in migliaia di foglioline minuscole.
Da qui le SCHEDE: 'leafShape': 'rect' fa foglie da quattro vertici, e su ognuna
si stampa un ramoscello intero preso da una texture con l'alfa. Le 1.450 schede
dell'abete sono 5.800 vertici e una chioma piena; le foglioline di Sapling, per
riempire altrettanto, ne costerebbero decine di migliaia.

E ATTENZIONE AL CONTEGGIO: quello stampato per primo e' di Blender, quello
stampato dopo l'esportazione e' del FILE, ed e' piu' alto - il glTF vuole un
attributo per vertice e l'esportatore sdoppia sulle cuciture. Il tetto dei
65.535 per primitiva vale sul secondo.

DUE COSE CHE FANNO SEMBRARE ROTTO IL RISULTATO, se non si sanno prima:

  1. IL RITAGLIO SI ACCENDE DAL FORMATO. LightAlphaCutFor() in src/light.c
     guarda se la texture diffusa ha un canale alfa, non l'alphaMode del glTF.
     twig_diff e twig_alpha sono due jpg separati, e un jpg l'alfa non ce l'ha:
     vanno uniti in un PNG RGBA, ed e' quello che fa unisci_rgba() qui sotto.
     Saltare il passo da' foglie a rettangoli opachi - ed e' gia' successo: il
     ritaglio di bush non e' mai stato attivo in gioco per questa ragione.

  2. L'ATLANTE NON E' FATTO DI SCHEDE. twig_diff e' lo spiegamento della mesh
     originale del pino: due ramoscelli buoni, delle pigne, e tutto intorno il
     riempimento sbavato dei bordi, che nell'alfa e' BIANCO, cioe' opaco.
     Mappare una scheda su tutto 0..1 darebbe una macchia marrone. Le schede si
     mappano sul rettangolo CIUFFO_UV, misurato sull'alfa.
"""
import ast
import json
import os
import struct
import sys

import addon_utils
import bpy
import numpy as np      # spedito dentro Blender, non e' una dipendenza in piu'


# Il ramoscello dentro l'atlante twig_diff di pine_tree_01: u0, u1, v0, v1.
# Misurato sull'alfa - bianco e' opaco - il 17 settembre 2026. L'atlante ne ha
# due utilizzabili; questo e' quello verticale, che sta a testa in su come la
# scheda. L'altro e' orizzontale, u 0,70..0,95 e v 0,50..0,67, e vorrebbe schede
# piu' larghe che alte: e' li' se un giorno si vuole variare la chioma.
CIUFFO_UV = (0.029, 0.220, 0.663, 0.966)


def argomenti():
    if "--" not in sys.argv:
        print("uso: blender --background --python tools/sapling_tree.py -- "
              "<preset> '<override json>' [uscita.glb] [cartella texture]",
              file=sys.stderr)
        raise SystemExit(2)
    a = sys.argv[sys.argv.index("--") + 1:]
    if not a:
        raise SystemExit(2)
    return (a[0],
            json.loads(a[1]) if len(a) > 1 else {},
            a[2] if len(a) > 2 else "",
            a[3] if len(a) > 3 else "")


def parametri(preset, override):
    """I preset di Sapling sono un dizionario Python su una riga sola, dopo
    l'intestazione di licenza: si leggono con literal_eval, non si eseguono."""
    # Il percorso si chiede al modulo dell'addon, non si ricostruisce a mano:
    # Blender cerca gli addon in piu' posti e ricostruirlo funzionerebbe su
    # questa macchina e non sulla prossima.
    modulo = sys.modules.get("add_curve_sapling")
    if modulo is None:
        print("Sapling non e' attivo: vedi le istruzioni in testa a questo file",
              file=sys.stderr)
        raise SystemExit(1)
    cartella = os.path.join(os.path.dirname(modulo.__file__), "presets")
    percorso = os.path.join(cartella, preset + ".py")
    if not os.path.exists(percorso):
        print("preset non trovato: %s" % percorso, file=sys.stderr)
        raise SystemExit(1)
    testo = open(percorso).read()
    d = ast.literal_eval(testo[testo.index("{"):].strip())
    d.update(override)
    return d


def immagine(percorso, colore=True):
    """Carica una texture. 'colore' falso e' per le normal map: vanno lette
    come numeri, non come colori, o Blender ci applica la curva sRGB."""
    im = bpy.data.images.load(percorso, check_existing=True)
    if not colore:
        im.colorspace_settings.name = 'Non-Color'
    return im


def unisci_rgba(diff, alfa, uscita):
    """Diffusa piu' maschera in un PNG con il canale alfa.

    E' il passo che accende il ritaglio: light.c lo decide dal FORMATO della
    texture. Poly Haven spedisce la maschera come file a se', e un jpg il
    canale alfa non ce l'ha.

    Il giro dentro Blender e' esatto, non approssimato: per un'immagine a 8 bit
    'pixels' restituisce i byte come stanno, gia' divisi per 255, e il PNG
    riscritto e' identico al jpg di partenza sui tre canali. Verificato.
    """
    if os.path.exists(uscita) and \
       os.path.getmtime(uscita) >= max(os.path.getmtime(diff),
                                       os.path.getmtime(alfa)):
        return uscita

    d = immagine(diff)
    a = immagine(alfa)
    if tuple(d.size) != tuple(a.size):
        print("sapling: diffusa %dx%d e alfa %dx%d non combaciano"
              % (d.size[0], d.size[1], a.size[0], a.size[1]), file=sys.stderr)
        raise SystemExit(1)

    pd = np.empty(d.size[0] * d.size[1] * 4, dtype=np.float32)
    pa = np.empty(a.size[0] * a.size[1] * 4, dtype=np.float32)
    d.pixels.foreach_get(pd)
    a.pixels.foreach_get(pa)
    pd[3::4] = pa[0::4]          # il rosso della maschera diventa l'alfa

    out = bpy.data.images.new("twig_rgba", d.size[0], d.size[1], alpha=True)
    out.colorspace_settings.name = d.colorspace_settings.name
    out.pixels.foreach_set(pd)
    out.file_format = 'PNG'
    out.filepath_raw = uscita
    out.save()
    print("sapling: unite %s e %s in %s"
          % (os.path.basename(diff), os.path.basename(alfa),
             os.path.basename(uscita)))
    return uscita


def materiale(nome, diffusa, normale, ritaglio):
    """Un materiale PBR con diffusa e normal map. 'ritaglio' lega anche l'alfa,
    che e' quello che il glTF esporta come alphaMode MASK."""
    mat = bpy.data.materials.new(nome)
    mat.use_nodes = True
    nodi = mat.node_tree.nodes
    lega = mat.node_tree.links.new
    bsdf = nodi["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.9   # e' vegetazione, non vernice
    bsdf.inputs["Metallic"].default_value = 0.0

    tex = nodi.new("ShaderNodeTexImage")
    tex.image = immagine(diffusa)
    lega(tex.outputs["Color"], bsdf.inputs["Base Color"])
    if ritaglio:
        lega(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        mat.blend_method = 'CLIP'
        mat.alpha_threshold = 0.5      # lo stesso 0,5 di ALPHA_CUT in light.c

    if normale and os.path.exists(normale):
        nt = nodi.new("ShaderNodeTexImage")
        nt.image = immagine(normale, colore=False)
        nm = nodi.new("ShaderNodeNormalMap")
        lega(nt.outputs["Color"], nm.inputs["Color"])
        lega(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def schede_sul_ciuffo(me):
    """Rimappa le UV delle foglie sul ramoscello dentro l'atlante.

    Sapling manda ogni foglia su tutto 0..1, che nell'atlante del pino e' per
    meta' riempimento sbavato e opaco. Qui ogni scheda va sul rettangolo
    misurato, e una su due si specchia: e' l'unica varieta' che non costa
    niente, perche' cambia solo le coordinate.
    """
    if not me.uv_layers:
        print("sapling: le foglie non hanno UV, niente da rimappare")
        return
    u0, u1, v0, v1 = CIUFFO_UV
    uv = me.uv_layers[0].data
    for n, poly in enumerate(me.polygons):
        specchio = (n % 2) == 1
        for l in poly.loop_indices:
            u, v = uv[l].uv
            if specchio:
                u = 1.0 - u
            uv[l].uv = (u0 + u * (u1 - u0), v0 + v * (v1 - v0))


def vesti(cartella):
    """Corteccia sul tronco, ciuffo con il ritaglio sulle schede.

    I nomi sono quelli di Poly Haven, cosi' la cartella e' quella che lascia
    ./tools/fetch_assets.sh abete senza rinominare niente.
    """
    def f(nome):
        return os.path.join(cartella, nome)

    for nome in ("bark_diff.jpg", "twig_diff.jpg", "twig_alpha.jpg"):
        if not os.path.exists(f(nome)):
            print("sapling: manca %s: lancia ./tools/fetch_assets.sh abete"
                  % f(nome), file=sys.stderr)
            raise SystemExit(1)

    rgba = unisci_rgba(f("twig_diff.jpg"), f("twig_alpha.jpg"), f("twig_rgba.png"))
    corteccia = materiale("corteccia", f("bark_diff.jpg"), f("bark_nor_gl.jpg"),
                          ritaglio=False)
    ciuffo = materiale("ciuffo", rgba, f("twig_nor_gl.jpg"), ritaglio=True)

    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue
        # I due oggetti li nomina Sapling: 'tree' sono i rami, 'leaves' le
        # foglie. Un nome sconosciuto prende la corteccia, che e' il verso
        # giusto in cui sbagliare: un ramo opaco si vede, una scheda senza
        # ritaglio e' un rettangolo marrone in mezzo alla chioma.
        foglie = ob.name.startswith("leaves")
        ob.data.materials.clear()
        ob.data.materials.append(ciuffo if foglie else corteccia)
        if foglie:
            schede_sul_ciuffo(ob.data)


def conta_glb(percorso):
    """I vertici COME LI VEDE IL GIOCO, riletti dal file appena scritto.

    Non sono quelli di Blender, e la differenza va nella direzione che fa male:
    il glTF vuole un attributo per vertice, quindi l'esportatore SDOPPIA i
    vertici sulle cuciture di UV e normali. Il nostro abete passa da 9.696 a
    10.670. Il tetto dei 65.535 per primitiva lo misura raylib sul file, non
    Blender sulla mesh: controllarlo sul conteggio sbagliato vuol dire
    lasciarsi passare un modello che in gioco esce sfregiato.
    """
    with open(percorso, "rb") as f:
        b = f.read()
    n = struct.unpack("<I", b[12:16])[0]
    j = json.loads(b[20:20 + n])
    peggiore = 0
    for mesh in j.get("meshes", []):
        for prim in mesh.get("primitives", []):
            v = j["accessors"][prim["attributes"]["POSITION"]]["count"]
            peggiore = max(peggiore, v)
            print("sapling:   %-10s %6d vertici nel file" % (mesh.get("name", "?"), v))
    if peggiore > 65535:
        print("sapling: ATTENZIONE, nel FILE una primitiva ha %d vertici: oltre "
              "il tetto di 65535, LoadExtProps() lo scarterebbe" % peggiore)
    return peggiore


def main():
    preset, override, uscita, texture = argomenti()
    addon_utils.enable("add_curve_sapling", default_set=False, persistent=False)

    d = parametri(preset, override)
    # Solo le proprieta' che l'operatore conosce: un preset di una versione
    # diversa puo' portarne di sconosciute, e passarle farebbe fallire tutto
    # invece di ignorarle.
    note = set(bpy.ops.curve.tree_add.get_rna_type().properties.keys())
    kw = {k: v for k, v in d.items() if k in note}
    ignorate = sorted(set(d) - set(kw))

    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    bpy.ops.curve.tree_add(do_update=True, **kw)

    # Sapling produce CURVE; il gioco vuole mesh. Con showLeaves nascono DUE
    # oggetti - rami e foglie - e sono due materiali, quindi due lotti per
    # albero: il ritaglio alfa serve solo al secondo.
    for ob in list(bpy.data.objects):
        if ob.type == 'CURVE':
            bpy.context.view_layer.objects.active = ob
            ob.select_set(True)
            bpy.ops.object.convert(target='MESH')
            ob.select_set(False)

    if texture:
        vesti(texture)

    vert = tri = 0
    peggiore = 0
    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue
        me = ob.data
        me.calc_loop_triangles()
        vert += len(me.vertices)
        tri += len(me.loop_triangles)
        peggiore = max(peggiore, len(me.vertices))

    print("sapling: preset=%s vertici=%d (peggiore per oggetto %d) triangoli=%d "
          "oggetti=%d" % (preset, vert, peggiore, tri, len(bpy.data.objects)))
    if peggiore > 65535:
        print("sapling: ATTENZIONE, %d vertici in un oggetto superano il tetto "
              "di 65535 e LoadExtProps() scarterebbe il modello" % peggiore)
    if ignorate:
        print("sapling: proprieta' ignorate dal preset:", ", ".join(ignorate))

    if uscita:
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.export_scene.gltf(filepath=uscita, export_format='GLB',
                                  use_selection=True)
        print("sapling: scritto %s (%d byte)" % (uscita, os.path.getsize(uscita)))
        conta_glb(uscita)


main()
