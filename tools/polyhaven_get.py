#!/usr/bin/env python3
"""Scarica un modello glTF di Poly Haven, con il suo .bin e le sue texture.

    polyhaven_get.py <files.json> <cartella> <nome-da-dare-al-gltf>

Il primo argomento e' la risposta di https://api.polyhaven.com/files/<asset>,
gia' salvata: cosi' lo script non decide da solo cosa scaricare, e chi lo
chiama puo' guardare il JSON prima.

Il .gltf prende il nome che il gioco cerca - rock.gltf - mentre il .bin e le
texture tengono il loro, perche' il .gltf li nomina per come stanno nel
pacchetto. Rinominarli lo romperebbe.

Si prende sempre la risoluzione 1k. Le texture del terreno e dei prop in
Frostmark si vedono da metri di distanza, e 4k o 8k sarebbero centinaia di MB
per una differenza che non si nota.

Come gli altri strumenti del repo: nessuna dipendenza oltre alla libreria
standard.
"""
import json
import os
import sys
import urllib.request


def scarica(url, dove, base):
    os.makedirs(os.path.dirname(dove) or ".", exist_ok=True)
    urllib.request.urlretrieve(url, dove)
    print(f"  {os.path.getsize(dove) / 1e6:6.2f} MB  {os.path.relpath(dove, base)}")


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2

    dati, cartella, nome = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(dati, encoding="utf-8") as f:
        d = json.load(f)

    if "gltf" not in d:
        print("questo asset non ha una versione glTF: mi fermo")
        return 1

    g = d["gltf"]["1k"]["gltf"]
    scarica(g["url"], os.path.join(cartella, nome), cartella)
    for allegato, v in sorted(g.get("include", {}).items()):
        scarica(v["url"], os.path.join(cartella, allegato), cartella)
    return 0


if __name__ == "__main__":
    sys.exit(main())
