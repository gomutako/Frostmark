#version 330

/* Come scene.vs, ma la trasformazione arriva per ISTANZA invece che per
 * disegno: cosi' mille alberi costano una chiamata sola. Il fragment shader e'
 * lo stesso file di scene.vs - scene.fs - perche' la luce deve vivere in un
 * posto solo, o le due strade divergono e non si capisce piu' quale sbaglia.
 *
 * Gli slot 8 e 9 non sono scelti a caso: raylib usa 0 per la posizione, 1 per
 * le UV, 2 per la normale, 3 per il colore, 4 per la tangente, 5 per le
 * seconde UV, 6 per gli indici, 7 e 8 per le ossa quando la skinning GPU e'
 * accesa. Sopra l'8 e' terra libera, e OpenGL 3.3 garantisce almeno sedici
 * attributi. */
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexTangent;

/* La w porta seno e coseno dell'imbardata, gia' calcolati dalla CPU: farli qui
 * significherebbe rifarli per OGNI VERTICE di ogni istanza, invece che una
 * volta per istanza. Quattro byte in piu' su un buffer minuscolo, qualche
 * milione di sincos in meno per fotogramma. */
layout(location = 8) in vec4 instPosSin;      /* xyz: posizione, w: sin */
layout(location = 9) in vec4 instScaleCos;    /* xyz: scala,     w: cos */

/* Non c'e' 'mvp': la matrice del modello e' per istanza, quindi vista e
 * proiezione servono separate. Le imposta instancing.c, perche' raylib le
 * manda da se' solo per DrawMesh(). */
uniform mat4 matView;
uniform mat4 matProjection;
/* La camera. light.c la mette gia' su ENTRAMBI i programmi, e un uniform e'
 * del programma linkato, non dello stadio: dichiararla qui non costa una riga
 * di C. */
uniform vec3 viewPos;

/* La posizione RELATIVA ALLA CAMERA, in metri - stessa regola di scene.vs, e
 * per la stessa ragione: scene.fs ne prende le derivate di schermo, e in fp32
 * una coordinata assoluta a 3000 m ha un ulp che vale un terzo del passo di
 * mondo fra due pixel a un metro. Vedi docs/06, domanda F. */
out vec3 fragPosRel;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragTangent;

/* Per i materiali proiettati: la posizione e la normale in spazio OGGETTO, e
 * quel che serve al fragment per riportare in mondo la normale perturbata.
 * La posizione e' gia' moltiplicata per la scala, cioe' e' in metri: senza,
 * la texture si stirerebbe con il pezzo, e la falda del tetto e' scalata
 * (cella, cella*1.6, cella*nz). */
out vec3 fragLocal;
out vec3 fragLocalNormal;
out vec2 fragYawSC;       /* seno e coseno dell'imbardata */
out vec3 fragInvScale;
/* Lo sfalsamento per istanza: la posizione dell'istanza riportata negli ASSI
 * DEL PEZZO, in metri. Si somma alla posizione prima di proiettare, non alla
 * UV dopo - vedi PosProiezione() in scene.fs - quindi qui deve gia' essere
 * ruotata all'indietro dell'imbardata, o due pannelli affiancati della stessa
 * parete non hanno le file di assi alla stessa quota. */
out vec3 fragProjOffset;

/* Rotazione attorno a Y con lo stesso verso di MatrixRotateY() di raymath:
 * x' = cos*x + sin*z, z' = -sin*x + cos*z. Sbagliare il segno qui specchia
 * tutta la foresta, e guardando un albero solo non si vede. */
vec3 RuotaY(vec3 v, float s, float c)
{
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

void main()
{
    float s  = instPosSin.w;
    float c  = instScaleCos.w;
    vec3  sc = instScaleCos.xyz;

    /* Stesso ordine di DrawModelEx: prima la scala, poi la rotazione, poi la
     * posizione. Invertire scala e rotazione sposta gli oggetti scalati in
     * modo non uniforme, e sono proprio quelli che si notano - le falde. */
    vec3 local = RuotaY(vertexPosition * sc, s, c);
    vec3 world = local + instPosSin.xyz;

    /* La normale si trasforma con l'inversa trasposta, che per una scala piu'
     * una rotazione attorno a Y vuol dire DIVIDERE per la scala e poi ruotare.
     * Dividere, non moltiplicare: su una scala non uniforme - la falda del
     * tetto e' (cella, cella*1.6, cella*nz) - moltiplicare darebbe normali
     * storte, e l'illuminazione sbagliata proprio sugli oggetti schiacciati.
     * Nessuna scala vale zero in questo gioco, quindi la divisione e' sicura. */
    vec3 nrm = RuotaY(vertexNormal / sc, s, c);

    /* La tangente invece giace SULLA superficie: segue la matrice del modello
     * come una posizione, quindi si moltiplica. La w resta com'e', e' un
     * segno. Il fragment pero' non la legge piu': da quando SurfaceNormal()
     * costruisce la terna dalle derivate di schermo, questo varying non lo
     * consuma nessuno. Resta calcolato perche' toglierlo e' un lavoro suo -
     * docs/06, domanda F. */
    vec3 tan = RuotaY(vertexTangent.xyz * sc, s, c);

    /* Stessa regola di scene.vs: mai formare il numero grande. La differenza
     * fra la posizione dell'istanza e la camera e' fra due numeri vicini,
     * quindi in fp32 e' esatta o quasi; sommarle la posizione locale, che e'
     * in metri, non la sporca. Sottrarre viewPos da 'world' non basterebbe:
     * 'world' e' gia' arrotondato a ulp(3000). */
    fragPosRel = local + (instPosSin.xyz - viewPos);
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = normalize(nrm);
    fragTangent  = vec4(tan, vertexTangent.w);

    /* Qui scala e imbardata arrivano dal dato d'istanza: niente da estrarre. */
    fragLocal       = vertexPosition * sc;
    fragLocalNormal = vertexNormal;
    fragYawSC       = vec2(s, c);
    fragInvScale    = 1.0 / max(sc, vec3(1e-6));
    fragProjOffset  = RuotaY(instPosSin.xyz, -s, c);

    gl_Position = matProjection * matView * vec4(world, 1.0);
}
