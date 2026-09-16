#!/usr/bin/env python3
"""Confronta due fotogrammi del banco sulla luminanza.

Serve alla modalita' fp32: due inquadrature che in aritmetica esatta sarebbero
identiche e differiscono solo per dove stanno nel mondo. I quattro numeri
stampati hanno gli STESSI NOMI della modalita' rumore, perche' misurano la
stessa cosa su coppie diverse - li' fotogrammi consecutivi, qui due posizioni
di mondo - e nomi diversi per la stessa grandezza costringono chi legge a
tenere a mente una tabella di traduzione.

La luminanza e' quella di sempre: 0,2126 R + 0,7152 G + 0,0722 B.
"""
import sys

import numpy as np
from PIL import Image


def luminanza(path):
    a = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def main(argv):
    if len(argv) != 3:
        print("uso: confronta_png.py A.png B.png", file=sys.stderr)
        return 2

    a = luminanza(argv[1])
    b = luminanza(argv[2])
    if a.shape != b.shape:
        print("dimensioni diverse: %s contro %s" % (a.shape, b.shape),
              file=sys.stderr)
        return 2

    d = np.abs(a - b)
    # 'frazione_cambiati' conta i pixel che differiscono di almeno mezzo
    # livello su 255: la stessa soglia della modalita' rumore, ed e' li' che
    # sta scritto il perche' - sotto mezzo livello il confronto misura
    # l'arrotondamento a 8 bit dello schermo, non il fenomeno.
    print("diff_media=%.5f pixel=%d frazione_cambiati=%.5f diff_max=%.2f"
          % (d.mean(), d.size, float((d >= 0.5).sum()) / d.size, d.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
