#!/usr/bin/env python3
"""Ripunta la scena di un file .glb sui nodi radice veri.

Perche' serve: diversi pacchetti CC0 (i kit di Kenney, esportati con UniGLTF)
indicano come radice della scena un nodo che ha gia' un genitore, tipicamente
chiamato "tmpParent". La specifica glTF 2.0 lo vieta e cgltf - il parser usato
da raylib - rifiuta l'intero file con "Failed to load glTF data". Blender e
three.js sono piu' permissivi, per questo il problema non salta all'occhio.

La riparazione e' minima: la geometria, i materiali e le trasformazioni non
vengono toccati, cambia solo l'elenco 'scenes[].nodes'.

Uso: python3 tools/glb_fix_scene.py file1.glb [file2.glb ...]
"""
import json
import struct
import sys


def fix(path):
    data = open(path, 'rb').read()
    if data[:4] != b'glTF':
        return f"{path}: non e' un .glb"

    json_len = struct.unpack_from('<I', data, 12)[0]
    gltf = json.loads(data[20:20 + json_len])
    blob = data[20 + json_len + 8:]          # chunk BIN, invariato

    children = set()
    for node in gltf.get('nodes', []):
        children.update(node.get('children') or [])
    roots = [i for i in range(len(gltf.get('nodes', []))) if i not in children]
    if not roots:
        return f"{path}: nessun nodo radice, lascio il file com'e'"

    before = [scene.get('nodes') for scene in gltf.get('scenes', [])]
    for scene in gltf.get('scenes', []):
        scene['nodes'] = roots
    if before == [roots] * len(before):
        return f"{path}: gia' a posto"

    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)     # i chunk vanno allineati a 4 byte
    open(path, 'wb').write(
        struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(js) + 8 + len(blob))
        + struct.pack('<I4s', len(js), b'JSON') + js
        + struct.pack('<I4s', len(blob), b'BIN\0') + blob)
    return f"{path}: scena ripuntata su {roots}"


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for arg in sys.argv[1:]:
        print(fix(arg))
