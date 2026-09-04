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

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
