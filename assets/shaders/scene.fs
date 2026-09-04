#version 330

/* Una sola luce direzionale - il sole - piu' un'ombra letta da una mappa di
 * profondita'. Lo stesso shader serve anche il passaggio d'ombra: li' scrive
 * solo la profondita', e il colore non interessa a nessuno. */
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragTangent;

uniform sampler2D texture0;   /* albedo   - raylib: MATERIAL_MAP_DIFFUSE */
uniform sampler2D texture2;   /* normali  - raylib: MATERIAL_MAP_NORMAL  */
uniform vec4 colDiffuse;

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
    if (depthOnly == 1) { finalColor = vec4(1.0); return; }

    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    vec3  n    = SurfaceNormal();
    float diff = max(dot(n, lightDir), 0.0);
    float light = AMBIENT + SUN * diff * ShadowFactor(n) * sunAmount;

    finalColor = vec4(albedo.rgb * light, albedo.a);
}
