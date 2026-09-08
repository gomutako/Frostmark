#version 330

/* Una sola luce direzionale - il sole - piu' un'ombra letta da una mappa di
 * profondita'. Lo stesso shader serve anche il passaggio d'ombra: li' scrive
 * solo la profondita', e il colore non interessa a nessuno. */
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragTangent;
in vec3 fragLocal;
in vec3 fragLocalNormal;
in vec2 fragYawSC;
in vec3 fragInvScale;
in vec3 fragProjOffset;

uniform sampler2D texture0;   /* albedo   - raylib: MATERIAL_MAP_DIFFUSE */
uniform sampler2D texture2;   /* normali  - raylib: MATERIAL_MAP_NORMAL  */
uniform vec4 colDiffuse;

/* 0 = le UV della mesh, 1 = proiezione sull'asse dominante, 2 = miscela a tre.
 * Viaggia per LOTTO, come alphaCut. */
uniform int   projMode;
uniform float projTile;    /* metri coperti da una ripetizione */

/* Soglia sotto la quale il frammento sparisce. 0 spegne il ritaglio, ed e' il
 * valore di riposo: la stragrande maggioranza delle superfici e' opaca e non
 * deve pagare ne' il confronto ne' il 'discard', che su molte schede spegne il
 * test di profondita' anticipato. Chi ne ha bisogno lo accende per il suo
 * gruppo - vedi instancing.c. */
uniform float alphaCut;

uniform vec3  lightDir;      /* direzione VERSO il sole, normalizzata */
uniform float sunAmount;     /* 1 a mezzogiorno, 0 di notte            */
uniform int   depthOnly;     /* 1 durante il passaggio d'ombra         */
uniform int   shadowOn;
uniform int   shadowRes;
uniform vec3  viewPos;
uniform float splitDist;      /* oltre questa distanza si usa la mappa larga */
uniform mat4  lightVP0;       /* mappa vicina, texel piccoli   */
uniform mat4  lightVP1;       /* mappa lontana, texel grossi   */
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

out vec4 finalColor;

/* Ambiente e sole sommano circa 1 su una superficie ben esposta: cosi' la luce
 * ridistribuisce il colore invece di schiarire o scurire tutta la scena, e la
 * tinta del ciclo giorno/notte resta quella decisa da GameAmbientTint().
 * Il sole pesa quasi il doppio dell'ambiente: con i due alla pari l'ombra
 * toglieva un quarto della luce e non si vedeva. */
const float AMBIENT = 0.45;
const float SUN     = 0.85;

/* Media 4x4 su mezzo texel di passo: sedici confronti invece di nove, e su
 * mezzo texel invece che uno intero. Il bordo dell'ombra passa da chiaro a
 * scuro in piu' gradini, quindi non si legge piu' come una scaletta. */
float Pcf(sampler2D map, vec3 proj, float bias)
{
    float texel = 1.0 / float(shadowRes);
    float sum = 0.0;
    for (int x = -2; x <= 1; x++)
        for (int y = -2; y <= 1; y++) {
            vec2 off = (vec2(x, y) + 0.5) * 0.5 * texel;
            float d = texture(map, proj.xy + off).r;
            sum += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    return sum / 16.0;
}

/* La normale del frammento. Quella del vertice descrive la forma grossa; la
 * normal map aggiunge il rilievo che la mesh non ha - la corteccia, la fuga fra
 * due pietre - ed e' meta' di cio' che fa sembrare realistico un asset.
 *
 * La mappa e' in spazio tangente, cioe' relativa alla superficie: per usarla
 * serve la terna (tangente, bitangente, normale). La tangente si raddrizza
 * rispetto alla normale (Gram-Schmidt) perche' interpolare fra vertici le
 * sfasa, e la bitangente si ricava dal prodotto vettore con il segno che il
 * .glb porta in w - i due versi esistono entrambi, e sbagliarlo ribalta il
 * rilievo.
 *
 * Se la mesh non porta tangenti raylib passa un vettore nullo: normalizzarlo
 * darebbe NaN, quindi in quel caso si resta alla normale del vertice. E i
 * materiali senza normal map ne ricevono una piatta da light.c, cosi' qui non
 * serve sapere se ce n'e' una vera: il conto e' sempre lo stesso. */
/* --- Materiali proiettati --------------------------------------------------
 * I pezzi dei kit non hanno UV utilizzabili: il muro ha 64 vertici e tutte le
 * sue coordinate stanno in una cella della tavolozza. Per loro la texture si
 * ricava dalla POSIZIONE, e in spazio oggetto: proiettando in coordinate di
 * mondo, una casa ruotata prenderebbe la venatura di traverso.
 *
 * Asse dominante e non miscela a tre: i pezzi sono pannelli allineati agli
 * assi, quindi su una faccia piatta la scelta e' netta e costa un prelievo
 * invece di tre. La miscela serve solo dove la normale e' diagonale. */
int AsseDominante(vec3 n)
{
    vec3 a = abs(n);
    if (a.x >= a.y && a.x >= a.z) return 1;
    if (a.z >= a.y) return 2;
    return 0;
}

/* La V segue sempre la verticale dell'oggetto sulle facce laterali: e' quello
 * che tiene le assi verticali e le file parallele al muro. */
vec2 ProiettaUV(vec3 p, int asse)
{
    if (asse == 1) return vec2(p.z, p.y);
    if (asse == 2) return vec2(p.x, p.y);
    return vec2(p.x, p.z);
}

/* La posizione su cui si proietta: quella locale piu' la posizione
 * dell'istanza riportata negli assi del pezzo. Sommarla PRIMA di proiettare, e
 * non alla UV dopo, e' cio' che rende la parete un campo continuo: due pannelli
 * affiancati della stessa parete differiscono solo lungo la direzione della
 * parete, che e' lo stesso asse della U, quindi il motivo prosegue attraverso
 * il giunto invece di ripartire. La rotazione non tocca la Y, quindi le file
 * restano alla stessa quota su tutti i pannelli dello stesso livello.
 *
 * Sommarla alla UV dopo la proiezione - come si faceva prima - mandava la
 * componente Z sulla verticale di una parete: 55 cm di scarto fra due pannelli
 * della stessa casa, misurati.
 *
 * Serve anche a dare varieta': senza, trenta case avrebbero la venatura
 * identica nello stesso punto del proprio corpo. */
vec3 PosProiezione() { return fragLocal + fragProjOffset; }

vec2 CoordProiettata()
{
    return ProiettaUV(PosProiezione(), AsseDominante(fragLocalNormal))
           / max(projTile, 1e-4);
}

/* Dove la normale e' diagonale l'asse dominante oscilla, e a meta' falda
 * nascerebbe un gradino netto. Li' si prelevano tutte e tre le proiezioni e si
 * pesano con il quadrato della normale: tre prelievi, su un tipo di pezzo
 * solo. Il quadrato invece del valore assoluto stringe la fascia in cui due
 * proiezioni si sovrappongono, e quindi la sfocatura. */
vec4 CampionaMiscelato(sampler2D tex)
{
    vec3 w = fragLocalNormal * fragLocalNormal;
    w /= max(w.x + w.y + w.z, 1e-4);
    float t = max(projTile, 1e-4);
    vec3 p = PosProiezione();

    /* Le tre proiezioni le da' ProiettaUV(), non tre vec2 riscritte a mano:
     * la regola di come si proietta deve stare in un posto solo, o i due posti
     * si disallineano - ed e' gia' successo. Il peso va con l'asse che nomina:
     * w.x con l'asse X (asse 1), w.y con Y (asse 0), w.z con Z (asse 2). */
    return texture(tex, ProiettaUV(p, 1) / t) * w.x
         + texture(tex, ProiettaUV(p, 0) / t) * w.y
         + texture(tex, ProiettaUV(p, 2) / t) * w.z;
}

vec3 SurfaceNormal()
{
    vec3 n = normalize(fragNormal);
    if (dot(fragTangent.xyz, fragTangent.xyz) < 1e-8) return n;

    vec3 t = fragTangent.xyz - n * dot(n, fragTangent.xyz);
    if (dot(t, t) < 1e-8) return n;          /* tangente parallela alla normale */
    t = normalize(t);

    vec3 b  = cross(n, t) * fragTangent.w;
    vec3 ts = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(mat3(t, b, n) * ts);
}

/* Sotto proiezione la tangente del vertice non serve: la terna si costruisce
 * dagli ASSI DELLA PROIEZIONE, che sono gli assi dell'oggetto. La normale
 * perturbata nasce quindi in spazio oggetto e va riportata in mondo con la
 * stessa regola che il vertex shader usa per le normali: dividere per la scala
 * e ruotare attorno a Y. Dividere, non moltiplicare: su una scala non uniforme
 * moltiplicare darebbe normali storte. */
vec3 RuotaYFrag(vec3 v)
{
    float s = fragYawSC.x, c = fragYawSC.y;
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

vec3 NormaleProiettata()
{
    int asse = AsseDominante(fragLocalNormal);
    vec3 n = normalize(fragLocalNormal);

    /* Tangente e bitangente sono gli ASSI DELLA PROIEZIONE: la U corre lungo
     * il primo, la V lungo il secondo, e ProiettaUV() dice quali sono. Vanno
     * dati tutti e due esplicitamente, e non la bitangente con cross(n, t):
     * il prodotto vettore da' una terna destrorsa, non la direzione in cui
     * cresce la V, e i due coincidono solo su meta' degli orientamenti. Con
     * cross() la parete rivolta a +Z e quella rivolta a -Z della stessa casa
     * illuminavano le stesse scanalature in versi opposti - solchi su una,
     * nervature sull'altra - e il piano del solaio era fra le rovesciate. */
    vec3 t, bt;
    if      (asse == 1) { t = vec3(0.0, 0.0, 1.0); bt = vec3(0.0, 1.0, 0.0); }
    else if (asse == 2) { t = vec3(1.0, 0.0, 0.0); bt = vec3(0.0, 1.0, 0.0); }
    else                { t = vec3(1.0, 0.0, 0.0); bt = vec3(0.0, 0.0, 1.0); }

    /* Il raddrizzamento serve solo alla falda del tetto, dove la normale non
     * sta su un asse e i due assi della proiezione non sono perpendicolari a
     * lei. Su una parete non cambia niente. */
    t  = t  - n * dot(n, t);
    bt = bt - n * dot(n, bt);
    if (dot(t, t) < 1e-8 || dot(bt, bt) < 1e-8)
        return normalize(RuotaYFrag(n * fragInvScale));
    t  = normalize(t);
    bt = normalize(bt);

    /* La stessa coordinata dell'albedo, sempre: se il rilievo si campionasse
     * altrove il solco non starebbe sopra il solco. */
    vec3 ts = texture(texture2, CoordProiettata()).rgb * 2.0 - 1.0;

    vec3 nObj = normalize(mat3(t, bt, n) * ts);
    return normalize(RuotaYFrag(nObj * fragInvScale));
}

float ShadowFactor(vec3 n)
{
    if (shadowOn == 0) return 1.0;

    /* Lo scostamento cresce sulle superfici radenti, dove un solo texel della
     * mappa copre molto terreno: senza, comparirebbero strisce di ombra
     * sulle facce illuminate. La mappa larga ha texel piu' grossi e ne chiede
     * di piu'. */
    float ndl = dot(n, lightDir);
    float dist = length(fragPosition - viewPos);

    if (dist < splitDist) {
        vec4 lp = lightVP0 * vec4(fragPosition, 1.0);
        vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
        if (proj.z <= 1.0 && proj.x > 0.0 && proj.x < 1.0 && proj.y > 0.0 && proj.y < 1.0) {
            float bias = max(0.0012 * (1.0 - ndl), 0.0003);
            /* dissolvenza verso l'altra mappa: senza, il passaggio si vede
               come una linea netta sul terreno */
            float f = Pcf(shadowMap0, proj, bias);
            float edge = smoothstep(splitDist * 0.82, splitDist, dist);
            if (edge <= 0.0) return f;

            vec4 lp1 = lightVP1 * vec4(fragPosition, 1.0);
            vec3 p1 = lp1.xyz / lp1.w * 0.5 + 0.5;
            if (p1.z > 1.0 || p1.x < 0.0 || p1.x > 1.0 || p1.y < 0.0 || p1.y > 1.0) return f;
            return mix(f, Pcf(shadowMap1, p1, max(0.0035 * (1.0 - ndl), 0.0009)), edge);
        }
    }

    vec4 lp = lightVP1 * vec4(fragPosition, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;                       /* fuori da entrambe: niente ombra */
    return Pcf(shadowMap1, proj, max(0.0035 * (1.0 - ndl), 0.0009));
}

void main()
{
    /* Gli edifici sono opachi: nel passaggio d'ombra il loro colore non lo
     * guarda nessuno, e sul tetto il modo 2 costava tre prelievi per frammento
     * in ogni cascata, buttati. Chi ha il ritaglio - le foglie - passa dalla
     * strada lunga qui sotto, perche' li' il colore decide la profondita'. */
    if (depthOnly == 1 && alphaCut <= 0.0) { finalColor = vec4(1.0); return; }

    vec4 base = (projMode == 2) ? CampionaMiscelato(texture0)
                                : texture(texture0, (projMode == 0) ? fragTexCoord
                                                                    : CoordProiettata());
    vec4 albedo = base * colDiffuse * fragColor;

    /* Il ritaglio va PRIMA dell'uscita anticipata del passaggio d'ombra: le
     * foglie sono ritagli su quadrati, e un frammento buttato via qui non
     * scrive profondita'. Senza, l'ombra di una fronda sarebbe un rettangolo.
     * Costa una lettura di texture anche nel passaggio di profondita', ed e'
     * il prezzo obbligato per avere ombre che somiglino alla pianta. */
    if (alphaCut > 0.0 && albedo.a < alphaCut) discard;

    if (depthOnly == 1) { finalColor = vec4(1.0); return; }

    vec3  n    = (projMode == 0) ? SurfaceNormal() : NormaleProiettata();
    float diff = max(dot(n, lightDir), 0.0);
    float light = AMBIENT + SUN * diff * ShadowFactor(n) * sunAmount;

    finalColor = vec4(albedo.rgb * light, albedo.a);
}
