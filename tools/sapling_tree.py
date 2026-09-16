#!/usr/bin/env python3
"""Genera un albero con Sapling e lo esporta in glTF. Gira DENTRO Blender.

    blender --background --python tools/sapling_tree.py -- \
            <preset> '<override json>' [uscita.glb]

Esempio, un abete da circa settemilacinquecento vertici:

    blender --background --python tools/sapling_tree.py -- douglas_fir \
      '{"resU":1,"bevelRes":0,"curveRes":[5,3,2,2],"levels":3,
        "branches":[0,40,10,6],"showLeaves":true,"leaves":10}' \
      assets/models/abete.glb

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
La strada giusta sono poche schede grandi con una texture di ciuffo e canale
alfa - twig_diff piu' twig_alpha di pine_tree_01, mezzo mega, scaricabili SENZA
i 949 MB di geometria. Attenzione pero': light.c accende il ritaglio guardando
il FORMATO della texture, non l'alphaMode del glTF, quindi i due file vanno
uniti in un RGBA o il ritaglio resta spento - e' lo stesso motivo per cui il
ritaglio di bush non e' mai stato attivo in gioco.
"""
import ast
import json
import os
import sys

import addon_utils
import bpy


def argomenti():
    if "--" not in sys.argv:
        print("uso: blender --background --python tools/sapling_tree.py -- "
              "<preset> '<override json>' [uscita.glb]", file=sys.stderr)
        raise SystemExit(2)
    a = sys.argv[sys.argv.index("--") + 1:]
    if not a:
        raise SystemExit(2)
    return a[0], json.loads(a[1]) if len(a) > 1 else {}, a[2] if len(a) > 2 else ""


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


def main():
    preset, override, uscita = argomenti()
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


main()
