#!/usr/bin/env python3
"""Scarica un materiale di Poly Haven: albedo e normal map.

    polyhaven_tex.py <files.json> <cartella> <nome>

Lascia <nome>_diff.jpg e <nome>_nor.jpg. Il primo argomento e' la risposta di
https://api.polyhaven.com/files/<asset> gia' salvata, come per i modelli: cosi'
lo script non decide da solo cosa scaricare.

Si prende sempre 1k in jpg. Gli edifici si guardano da qualche metro e la
proiezione ripete la texture ogni paio di metri: 4k sarebbe peso senza
differenza. La normale e' la variante OpenGL - 'nor_gl' - perche' e' la
convenzione che scene.fs si aspetta, con la Y verso l'alto; 'nor_dx' darebbe il
rilievo ribaltato.

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


def mappa(d, chiave):
    v = d.get(chiave, {}).get("1k", {})
    return v.get("jpg", {}).get("url")


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2

    dati, cartella, nome = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(dati, encoding="utf-8") as f:
        d = json.load(f)

    diff = mappa(d, "Diffuse")
    nor = mappa(d, "nor_gl")
    if diff is None:
        print("questo asset non ha una mappa Diffuse a 1k in jpg: mi fermo")
        return 1

    scarica(diff, os.path.join(cartella, f"{nome}_diff.jpg"))
    if nor is None:
        print("  senza normal map: il materiale restera' piatto")
    else:
        scarica(nor, os.path.join(cartella, f"{nome}_nor.jpg"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
