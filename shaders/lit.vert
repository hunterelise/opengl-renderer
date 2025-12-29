#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // Proper normal transform (handles non-uniform scale too)
    mat3 normalMat = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMat * aNormal);

    gl_Position = uProj * uView * worldPos;
}
