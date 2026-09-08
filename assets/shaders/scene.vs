#version 330

/* Vertici del terreno, dei prop e dei personaggi. Gli attributi e le matrici
 * hanno i nomi che raylib riempie da se': cambiarli significa perderli. */
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexTangent;   /* xyz: tangente; w: verso della bitangente */

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
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

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    /* La tangente segue la superficie come la normale, quindi la stessa
     * matrice. Il verso della bitangente (w) e' un segno, non una direzione:
     * non va trasformato. Se la mesh non porta tangenti raylib passa qui un
     * vettore nullo, e il fragment se ne accorge. */
    fragTangent  = vec4(vec3(matNormal * vec4(vertexTangent.xyz, 1.0)), vertexTangent.w);

    /* Scala e imbardata stanno dentro matModel: la scala e' la lunghezza delle
     * prime tre colonne, e la prima colonna normalizzata e' l'asse X ruotato,
     * cioe' (cos, 0, -sin) con la convenzione di RuotaY(). */
    vec3 sc = vec3(length(matModel[0].xyz), length(matModel[1].xyz),
                   length(matModel[2].xyz));
    vec3 ax = matModel[0].xyz / max(sc.x, 1e-6);
    fragLocal       = vertexPosition * sc;
    fragLocalNormal = vertexNormal;
    fragYawSC       = vec2(-ax.z, ax.x);
    fragInvScale    = 1.0 / max(sc, vec3(1e-6));
    /* La rotazione all'indietro: RuotaY() con il seno cambiato di segno.
     * Qui non c'e' la funzione perche' non c'e' un dato d'istanza da ruotare;
     * il seno e il coseno sono quelli appena estratti da matModel. */
    vec3 wp = matModel[3].xyz;
    fragProjOffset  = vec3(fragYawSC.y * wp.x - fragYawSC.x * wp.z, wp.y,
                           fragYawSC.x * wp.x + fragYawSC.y * wp.z);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
