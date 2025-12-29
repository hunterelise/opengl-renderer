#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform vec3 uColor;

uniform vec3 uCamPos;
uniform vec3 uLightDir;   // direction *towards* the surface (world space)
uniform vec3 uAmbient;
uniform float uShininess;

// Clipping plane in world space: dot(n, p) + d = 0
uniform bool  uClipEnabled;
uniform vec3  uClipNormal; // should be normalized
uniform float uClipD;

void main()
{
    if (uClipEnabled)
    {
        float side = dot(uClipNormal, vWorldPos) + uClipD;
        if (side < 0.0) discard; // discard the "negative" side
    }

    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(uLightDir);

    float diff = max(dot(N, L), 0.0);

    // Blinn-Phong
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);

    vec3 lit = uAmbient + diff * vec3(1.0) + spec * vec3(1.0);
    vec3 finalColor = uColor * lit;

    FragColor = vec4(finalColor, 1.0);
}