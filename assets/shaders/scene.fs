#version 330

/* Una sola luce direzionale - il sole - piu' un'ombra letta da una mappa di
 * profondita'. Lo stesso shader serve anche il passaggio d'ombra: li' scrive
 * solo la profondita', e il colore non interessa a nessuno. */
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3  lightDir;      /* direzione VERSO il sole, normalizzata */
uniform float sunAmount;     /* 1 a mezzogiorno, 0 di notte            */
uniform int   depthOnly;     /* 1 durante il passaggio d'ombra         */
uniform int   shadowOn;
uniform int   shadowRes;
uniform mat4  lightVP;
uniform sampler2D shadowMap;

out vec4 finalColor;

/* Ambiente e sole sommano circa 1 su una superficie ben esposta: cosi' la luce
 * ridistribuisce il colore invece di schiarire o scurire tutta la scena, e la
 * tinta del ciclo giorno/notte resta quella decisa da GameAmbientTint().
 * Il sole pesa quasi il doppio dell'ambiente: con i due alla pari l'ombra
 * toglieva un quarto della luce e non si vedeva. */
const float AMBIENT = 0.45;
const float SUN     = 0.85;

float ShadowFactor(vec3 n)
{
    if (shadowOn == 0) return 1.0;

    vec4 lp = lightVP * vec4(fragPosition, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;                       /* fuori dalla mappa: niente ombra */

    /* Lo scostamento cresce sulle superfici radenti, dove un solo texel della
     * mappa copre molto terreno: senza, comparirebbero strisce di ombra
     * sulle facce illuminate. */
    float bias = max(0.0025 * (1.0 - dot(n, lightDir)), 0.0006);
    float texel = 1.0 / float(shadowRes);

    float sum = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float d = texture(shadowMap, proj.xy + vec2(x, y) * texel).r;
            sum += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    return sum / 9.0;                     /* tre per tre: bordi meno scalettati */
}

void main()
{
    if (depthOnly == 1) { finalColor = vec4(1.0); return; }

    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    vec3  n    = normalize(fragNormal);
    float diff = max(dot(n, lightDir), 0.0);
    float light = AMBIENT + SUN * diff * ShadowFactor(n) * sunAmount;

    finalColor = vec4(albedo.rgb * light, albedo.a);
}
