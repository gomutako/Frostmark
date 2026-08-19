#!/usr/bin/env bash
# ============================================================================
#  tools/fetch_assets.sh - prepara la cartella assets/ per Frostmark.
#
#  IMPORTANTE: il gioco funziona senza alcun asset. Questo script
#    1. crea la struttura di cartelle;
#    2. genera un CREDITS.md da compilare;
#    3. stampa l'elenco dei pacchetti CC0 consigliati con i relativi link.
#
#  Lo script NON scarica nulla automaticamente: gli URL dei cataloghi cambiano
#  e la licenza va verificata a mano, asset per asset. Vedi
#  docs/03-asset-pubblici.md.
#
#  Uso:  ./tools/fetch_assets.sh            solo struttura + istruzioni
#        ./tools/fetch_assets.sh heightmap  genera anche una heightmap di prova
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSETS="$ROOT/assets"

mkdir -p "$ASSETS"/{textures,models,fonts,audio}

if [ ! -f "$ASSETS/CREDITS.md" ]; then
cat > "$ASSETS/CREDITS.md" <<'EOF'
# Crediti asset

Compilare una riga per ogni file aggiunto. Anche per gli asset CC0, che non
richiedono attribuzione: serve a dimostrare la provenienza in caso di dubbi.

| File | Autore | Fonte (URL) | Licenza | Data |
|---|---|---|---|---|
|  |  |  |  |  |
EOF
echo "creato assets/CREDITS.md"
fi

cat <<'EOF'

------------------------------------------------------------------------
 Pacchetti CC0 consigliati (scaricare a mano, verificare la licenza)
------------------------------------------------------------------------

 TEXTURE DEL TERRENO  ->  assets/textures/
   ambientCG            https://ambientcg.com
     Grass004, Rock030, Snow006, Ground054 (formato PNG 1K, "Color")
     Rinominare in: grass.png, rock.png, snow.png, sand.png

 MODELLI LOW-POLY     ->  assets/models/
   Quaternius           https://quaternius.com
     "Ultimate Nature Pack", "Ultimate Modular Ruins", "Animated Humans"
   Kenney               https://kenney.nl/assets
     "Nature Kit", "Fantasy Town Kit", "Mini Characters"
     Formati utili: .glb / .gltf / .obj

 FONT INTERFACCIA     ->  assets/fonts/
   Google Fonts         https://fonts.google.com
     Consigliati: "Cinzel" (titoli), "Inter" o "IBM Plex Sans" (testo)
     Rinominare in: ui.ttf

 AUDIO                ->  assets/audio/
   Kenney               https://kenney.nl/assets  ("Impact Sounds", "RPG Audio")
   Freesound            https://freesound.org     FILTRARE PER CC0

 TERRENO REALE        ->  assets/heightmap.png
   USGS EarthExplorer   https://earthexplorer.usgs.gov   (SRTM, dominio pubblico)
   Conversione:
     gdal_translate -of PNG -ot Byte -scale -outsize 1024 1024 dem.tif heightmap.png

------------------------------------------------------------------------
 NON usare asset estratti da giochi commerciali (incluso Skyrim).
 Vedi docs/03-asset-pubblici.md per i punti di innesto nel codice.
------------------------------------------------------------------------
EOF

# ---- heightmap di prova generata localmente, senza rete --------------------
if [ "${1:-}" = "heightmap" ]; then
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$ASSETS/heightmap.png" <<'PY'
import sys, struct, zlib, math, random

# Heightmap 512x512 in scala di grigi, PNG scritto a mano (nessuna dipendenza).
N = 512
random.seed(7)

def fbm_grid(cells):
    g = [[random.random() for _ in range(cells + 1)] for _ in range(cells + 1)]
    def sample(x, y):
        fx, fy = x * cells, y * cells
        x0, y0 = int(fx), int(fy)
        tx, ty = fx - x0, fy - y0
        sx = tx * tx * (3 - 2 * tx)
        sy = ty * ty * (3 - 2 * ty)
        a = g[y0][x0] + (g[y0][x0 + 1] - g[y0][x0]) * sx
        b = g[y0 + 1][x0] + (g[y0 + 1][x0 + 1] - g[y0 + 1][x0]) * sx
        return a + (b - a) * sy
    return sample

layers = [(fbm_grid(4), 0.55), (fbm_grid(8), 0.25),
          (fbm_grid(16), 0.13), (fbm_grid(32), 0.07)]

rows = []
for j in range(N):
    row = bytearray([0])            # filtro 0 per ogni riga
    y = j / (N - 1) * 0.999
    for i in range(N):
        x = i / (N - 1) * 0.999
        h = sum(f(x, y) * w for f, w in layers)
        # attenua i bordi: isola circondata dal mare
        dx, dy = x - 0.5, y - 0.5
        d = min(1.0, math.hypot(dx, dy) * 2.0)
        h *= (1.0 - d ** 2.2)
        row.append(max(0, min(255, int(h * 300))))
    rows.append(bytes(row))

raw = b"".join(rows)

def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 0, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(raw, 9))
       + chunk(b"IEND", b""))
open(sys.argv[1], "wb").write(png)
print("generata assets/heightmap.png (512x512, isola di prova)")
PY
    else
        echo "python3 non trovato: impossibile generare la heightmap di prova."
    fi
    echo
    echo "Avvia il gioco: il file assets/heightmap.png viene rilevato in automatico."
    echo "Per tornare al terreno procedurale, rimuovilo o rinominalo."
fi
