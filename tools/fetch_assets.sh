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
#        ./tools/fetch_assets.sh models     scarica i modelli CC0 di Kenney
#        ./tools/fetch_assets.sh player     scarica il personaggio animato CC0
#        ./tools/fetch_assets.sh npc        scarica i personaggi animati degli NPC
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
EOF
echo "creato assets/CREDITS.md"
fi

# ---- personaggi animati per gli NPC (KayKit) ------------------------------
#  Un modello per ruolo. Popolano, mercante e anziano condividono lo stesso file
#  (vedi NPC_MODEL in src/entity.c), quindi viene caricato una volta sola.
#  Il lupo resta procedurale: fra questi pacchetti non c'e' un quadrupede.
if [ "${1:-}" = "npc" ]; then
    for cmd in curl python3; do
        command -v "$cmd" >/dev/null 2>&1 || { echo "serve $cmd"; exit 1; }
    done

    ADV="https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/main"
    SKE="https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0/main"

    echo "verifico le licenze..."
    for base in "$ADV" "$SKE"; do
        if ! curl -sSL "$base/LICENSE.txt" | grep -qi "Creative Commons Zero"; then
            echo "ATTENZIONE: licenza non CC0 su $base. Mi fermo."
            exit 1
        fi
    done
    echo "licenze verificate: CC0"

    while IFS=: read -r pack who dst; do
        [ -n "$pack" ] || continue
        case "$pack" in
            adv) url="$ADV/addons/kaykit_character_pack_adventures/Characters/gltf/$who.glb" ;;
            ske) url="$SKE/addons/kaykit_character_pack_skeletons/Characters/gltf/$who.glb" ;;
        esac
        echo "scarico $who.glb -> assets/models/$dst.glb"
        if ! curl -fsSL -o "$ASSETS/models/$dst.glb" "$url"; then
            rm -f "$ASSETS/models/$dst.glb"
            echo "  download fallito, salto"
            continue
        fi
        python3 "$ROOT/tools/glb_attachments.py" "$ASSETS/models/$dst.glb" | head -1

        TODAY="$(date +%Y-%m-%d)"
        if ! grep -q "assets/models/$dst.glb" "$ASSETS/CREDITS.md" 2>/dev/null; then
            echo "| assets/models/$dst.glb | Kay Lousberg (KayKit, $who) | https://github.com/KayKit-Game-Assets | CC0 | $TODAY |" \
                >> "$ASSETS/CREDITS.md"
        fi
    done <<'NPCS'
adv:Mage:npc_villager
adv:Knight:npc_guard
adv:Rogue_Hooded:npc_bandit
ske:Skeleton_Minion:npc_revenant
ske:Skeleton_Warrior:npc_boss
NPCS

    cat <<'NOTE'

Fatto. Gli NPC animati compaiono entro NPC_MODEL_DIST (140 m), oltre tornano
alle primitive. Ogni modello porta con se' tutte le sue animazioni: cinque
personaggi occupano qualche decina di MB di RAM.
NOTE
    exit 0
fi

# ---- personaggio animato CC0 (KayKit, Kay Lousberg) -----------------------
#  Un solo .glb con scheletro, texture inclusa e 76 animazioni: camminata,
#  corsa, attacco, parata, incantesimo, salto, colpito, morte. src/player.c le
#  cerca per nome, quindi vanno bene anche altri pacchetti (vedi ANIM_WANTED).
if [ "${1:-}" = "player" ]; then
    command -v curl >/dev/null 2>&1 || { echo "serve curl"; exit 1; }

    WHO="${2:-Knight}"          # Knight, Barbarian, Mage, Rogue, Rogue_Hooded
    BASE="https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/main"
    PACK="$BASE/addons/kaykit_character_pack_adventures"

    echo "verifico la licenza..."
    if ! curl -sSL "$BASE/LICENSE.txt" | grep -qi "Creative Commons Zero"; then
        echo "ATTENZIONE: la licenza non risulta CC0. Mi fermo."
        exit 1
    fi
    echo "licenza verificata: CC0"

    echo "scarico $WHO.glb (circa 3,5 MB)..."
    if ! curl -fsSL -o "$ASSETS/models/player.glb" "$PACK/Characters/gltf/$WHO.glb"; then
        rm -f "$ASSETS/models/player.glb"
        echo "download fallito: personaggi disponibili -> Knight, Barbarian, Mage, Rogue, Rogue_Hooded"
        exit 1
    fi
    echo "  -> assets/models/player.glb"

    # Arma, scudo, elmo e mantello sono mesh senza pesi: raylib non le anima e
    # perde il legame con l'osso. Questo strumento lo ricostruisce in un file di
    # testo che il gioco legge all'avvio.
    if command -v python3 >/dev/null 2>&1; then
        python3 "$ROOT/tools/glb_attachments.py" "$ASSETS/models/player.glb"
    else
        echo "python3 assente: niente agganci, il personaggio sara' a mani nude"
    fi

    TODAY="$(date +%Y-%m-%d)"
    if ! grep -q "assets/models/player.glb" "$ASSETS/CREDITS.md" 2>/dev/null; then
        echo "| assets/models/player.glb | Kay Lousberg (KayKit Adventurers, $WHO) | https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0 | CC0 | $TODAY |" \
            >> "$ASSETS/CREDITS.md"
    fi

    cat <<'NOTE'

Fatto. Il modello viene usato in terza persona (tasto F).
All'avvio il gioco stampa a quale animazione ha associato ogni ruolo:

  INFO: PLAYER:   camminata    -> Walking_A
  INFO: PLAYER:   parata       -> Blocking

Se cambi pacchetto e qualche ruolo risulta "assente", aggiungi il nome giusto
alla tabella ANIM_WANTED in src/player.c. La scala si regola con
PLAYER_MODEL_SCALE in src/config.h.

Arma, scudo ed elmo sono elencati in assets/models/player.attach: e' un file di
testo, per cambiare arma basta spostare un cancelletto.
NOTE
    exit 0
fi

# ---- modelli low-poly CC0 dal Nature Kit di Kenney -------------------------
#  E' l'unica sorgente CC0 con download diretto, stile coerente e poligoni bassi
#  (78-114 triangoli per modello): serve proprio questo, perche' Frostmark
#  disegna centinaia di prop per frame senza LOD ne' instancing. Poly Haven e
#  ambientCG sono altrettanto CC0 ma fotogrammetrici: il loro albero piu'
#  leggero sta a 150.000 triangoli, qui inutilizzabile.
if [ "${1:-}" = "models" ]; then
    for cmd in curl unzip python3; do
        command -v "$cmd" >/dev/null 2>&1 || { echo "serve $cmd"; exit 1; }
    done

    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT

    # L'URL contiene un hash che cambia a ogni aggiornamento del pacchetto:
    # lo si legge dalla pagina invece di inchiodarlo qui.
    echo "cerco il pacchetto su kenney.nl..."
    ZIP_URL="$(curl -sSL https://kenney.nl/assets/nature-kit \
               | grep -oE 'https://[^"]*nature-kit[^"]*\.zip' | head -1)"
    if [ -z "$ZIP_URL" ]; then
        echo "link non trovato: scarica a mano da https://kenney.nl/assets/nature-kit"
        exit 1
    fi

    echo "scarico $ZIP_URL"
    curl -sSL -o "$TMP/kit.zip" "$ZIP_URL"

    # Verifica della licenza prima di usare qualunque file.
    if ! unzip -p "$TMP/kit.zip" License.txt 2>/dev/null | grep -qi "Creative Commons Zero"; then
        echo "ATTENZIONE: License.txt non dichiara CC0. Mi fermo."
        exit 1
    fi
    echo "licenza verificata: CC0"

    # nome nel pacchetto -> nome cercato da src/world.c
    while IFS=: read -r src dst; do
        [ -n "$src" ] || continue
        unzip -p "$TMP/kit.zip" "Models/GLTF format/$src.glb" > "$ASSETS/models/$dst.glb"
        python3 "$ROOT/tools/glb_fix_scene.py" "$ASSETS/models/$dst.glb" >/dev/null
        echo "  $src.glb -> assets/models/$dst.glb"
    done <<'MODELS'
tree_default:tree
tree_pineTallA:pine
rock_largeA:rock
plant_bushLarge:bush
flower_yellowA:herb
MODELS

    # Crediti: una riga per file, anche se CC0 non lo richiede.
    TODAY="$(date +%Y-%m-%d)"
    for dst in tree pine rock bush herb; do
        grep -q "assets/models/$dst.glb" "$ASSETS/CREDITS.md" 2>/dev/null && continue
        echo "| assets/models/$dst.glb | Kenney | https://kenney.nl/assets/nature-kit | CC0 | $TODAY |" \
            >> "$ASSETS/CREDITS.md"
    done

    echo
    echo "Fatto. Avvia il gioco: i modelli vengono rilevati in automatico."
    echo "Per tornare alle primitive, svuota assets/models/."
    exit 0
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
