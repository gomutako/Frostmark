#!/usr/bin/env python3
"""Scarica singole mappe di un asset di Poly Haven, scelte per nome.

    polyhaven_mappe.py <files.json> <cartella> <mappa>...

Lascia <cartella>/<mappa>.jpg. Il primo argomento e' la risposta di
https://api.polyhaven.com/files/<asset> gia' salvata, come per i modelli e i
materiali: cosi' lo script non decide da solo cosa scaricare.

PERCHE' ESISTE, VISTO CHE C'E' GIA' polyhaven_tex.py. Quello scarica un
MATERIALE, che ha sempre le stesse due chiavi - 'Diffuse' e 'nor_gl'. Un
MODELLO ne ha una per ogni mappa di ogni suo materiale: pine_tree_01 ne
dichiara trentaquattro, da 'bark_diff' a 'twig_alpha'. E soprattutto le mappe
si scaricano SENZA la geometria: il ciuffo e la corteccia del pino sono tre
megabyte, il modello e' 958 MB e non entrerebbe comunque sotto il tetto dei
65.535 vertici per primitiva (docs/06, vincolo 1).

Si prende sempre 1k in jpg, per la stessa ragione di polyhaven_tex.py: una
scheda di fronda si guarda da qualche metro. Il jpg non ha canale alfa, e va
bene cosi': la mappa dell'alfa e' un file a se', e unirla alla diffusa e'
un altro passo - vedi tools/sapling_tree.py, che lo fa in Blender.

Come gli altri strumenti del repo: nessuna dipendenza oltre alla standard.
"""
import json
import os
import sys
import urllib.request


def scarica(url, dove):
    os.makedirs(os.path.dirname(dove) or ".", exist_ok=True)
    urllib.request.urlretrieve(url, dove)
    print(f"  {os.path.getsize(dove) / 1e6:6.2f} MB  {os.path.basename(dove)}")


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2

    dati, cartella, mappe = sys.argv[1], sys.argv[2], sys.argv[3:]
    with open(dati, encoding="utf-8") as f:
        d = json.load(f)

    mancanti = []
    for nome in mappe:
        url = d.get(nome, {}).get("1k", {}).get("jpg", {}).get("url")
        if url is None:
            mancanti.append(nome)
            continue
        scarica(url, os.path.join(cartella, f"{nome}.jpg"))

    if mancanti:
        print("non trovate a 1k in jpg: " + ", ".join(mancanti))
        print("le chiavi di questo asset sono: " + ", ".join(sorted(d)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
