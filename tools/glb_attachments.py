#!/usr/bin/env python3
"""Elenca le mesh agganciate a un osso in un modello .glb animato.

A cosa serve: raylib deforma solo le mesh dotate di pesi. Nei pacchetti di
personaggi armi, scudi ed elmi sono mesh separate agganciate a un osso, e
raylib perde sia il nome della mesh sia il legame con l'osso. Senza queste
informazioni la spada resterebbe ferma nella posa di riposo.

Questo script ricostruisce la corrispondenza e la scrive in un file di testo
accanto al modello (player.attach), che src/player.c legge all'avvio:

    <indice della mesh> <nome dell'osso>    # nome originale

L'indice e' quello che avra' la mesh in raylib, che crea una mesh per primitiva
visitando i nodi in ordine (vedi LoadGLTF in rmodels.c). Le righe che iniziano
con '#' sono ignorate: per cambiare arma o scudo basta spostare il cancelletto.

Uso: python3 tools/glb_attachments.py modello.glb [file.attach]
"""
import json
import struct
import sys

# Cosa agganciare per default, in ordine di preferenza. Il resto viene scritto
# commentato, pronto da attivare a mano.
PREFERRED = (
    ("arma",   ("1h_sword", "sword", "axe", "dagger", "mace", "staff", "wand")),
    ("scudo",  ("round_shield", "shield")),
    ("elmo",   ("helmet", "hat", "hood")),
    ("mantello", ("cape", "cloak")),
)
EXCLUDE = ("2h_", "offhand", "_color")


def read_gltf(path):
    data = open(path, 'rb').read()
    if data[:4] != b'glTF':
        sys.exit(f"{path}: non e' un .glb")
    json_len = struct.unpack_from('<I', data, 12)[0]
    return json.loads(data[20:20 + json_len])


def collect(gltf):
    """Ritorna (attachments, skinned) in ordine di mesh raylib."""
    nodes = gltf.get('nodes', [])
    parent = {}
    for i, node in enumerate(nodes):
        for child in node.get('children') or []:
            parent[child] = i

    joints = set()
    for skin in gltf.get('skins', []):
        joints.update(skin.get('joints', []))

    attachments, skinned, mesh_index = [], [], 0
    for i, node in enumerate(nodes):
        if 'mesh' not in node:
            continue
        prims = len(gltf['meshes'][node['mesh']]['primitives'])
        for _ in range(prims):
            name = node.get('name') or f"nodo{i}"
            if 'skin' in node:
                skinned.append((mesh_index, name))
            else:
                p = parent.get(i)
                if p is not None and p in joints:
                    attachments.append((mesh_index, nodes[p].get('name'), name))
            mesh_index += 1
    return attachments, skinned


def choose(attachments):
    """Un aggancio per slot, secondo PREFERRED."""
    chosen = {}
    for slot, patterns in PREFERRED:
        for pattern in patterns:
            for mesh, bone, name in attachments:
                low = name.lower()
                if any(x in low for x in EXCLUDE):
                    continue
                if pattern in low:
                    chosen.setdefault(slot, (mesh, bone, name))
                    break
            if slot in chosen:
                break
    return chosen


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else path.rsplit('.', 1)[0] + '.attach'

    gltf = read_gltf(path)
    attachments, skinned = collect(gltf)
    chosen = choose(attachments)
    picked = {m for m, _, _ in chosen.values()}

    lines = [
        f"# Agganci alle ossa per {path.split('/')[-1]}",
        "# Generato da tools/glb_attachments.py - vedi docs/03-asset-pubblici.md.",
        "# Formato: <indice mesh> <nome osso>   # nome originale",
        f"# Mesh con scheletro (animate da raylib): {len(skinned)}",
        "",
    ]
    for slot, (mesh, bone, name) in chosen.items():
        lines.append(f"{mesh} {bone}   # {slot}: {name}")
    lines.append("")
    lines.append("# Alternative: togli il cancelletto per usarle al posto di sopra.")
    for mesh, bone, name in attachments:
        if mesh not in picked:
            lines.append(f"# {mesh} {bone}   # {name}")

    open(out, 'w').write("\n".join(lines) + "\n")
    print(f"scritto {out}: {len(chosen)} agganci attivi su {len(attachments)} disponibili")
    for slot, (mesh, bone, name) in chosen.items():
        print(f"  {slot:9s} mesh {mesh:2d} -> osso {bone} ({name})")


if __name__ == '__main__':
    main()
