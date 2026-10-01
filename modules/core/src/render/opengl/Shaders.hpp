/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** GLSL sources of the OpenGL backend. The lighting mirrors render/software/Shading.cpp line for line:
** change both together, the parity tests compare their output.
*/
#pragma once

namespace k3::gl::shaders
{

    // Attribute locations shared by every program.
    inline constexpr unsigned POSITION = 0;
    inline constexpr unsigned NORMAL   = 1;
    inline constexpr unsigned UV       = 2;
    inline constexpr unsigned COLOR    = 3;
    inline constexpr unsigned TANGENT  = 4;

    inline constexpr const char* MESH_VERTEX = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;

uniform mat4  uViewProjection;
uniform mat4  uModel;
uniform mat4  uNormalMatrix;
uniform float uHandedness;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;
out vec4 vTangent;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);

    vWorld = world.xyz / world.w;
    vNormal = mat3(uNormalMatrix) * aNormal;
    vUV = aUV;
    vColor = aColor;
    // A mirroring transform flips the frame's handedness.
    vTangent = vec4(mat3(uModel) * aTangent.xyz, aTangent.w * uHandedness);
    gl_Position = uViewProjection * world;
}
)glsl";

    inline constexpr const char* MESH_FRAGMENT = R"glsl(
#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;
in vec4 vTangent;

out vec4 fragColor;

const int MAX_LIGHTS = 8;
const int DIRECTIONAL = 0;
const int SPOT = 2;
const int MASK = 1;
const int BLEND = 2;

// Frame
uniform vec3  uCameraPosition;
uniform vec3  uViewDirection;
uniform bool  uOrthographic;
uniform vec3  uAmbient;
uniform int   uLightCount;
uniform int   uLightType[MAX_LIGHTS];
uniform vec3  uLightPosition[MAX_LIGHTS];
uniform vec3  uLightToLight[MAX_LIGHTS];
uniform vec3  uLightDirection[MAX_LIGHTS];
uniform vec3  uLightRadiance[MAX_LIGHTS];
uniform float uLightRange[MAX_LIGHTS];
uniform float uLightCosInner[MAX_LIGHTS];
uniform float uLightCosOuter[MAX_LIGHTS];

uniform int       uShadowLight;
uniform sampler2D uShadowMap;
uniform mat4      uShadowViewProjection;
uniform int       uShadowSize;
uniform float     uShadowDepthBias;
uniform float     uShadowNormalOffset;
uniform int       uShadowPcfRadius;

// Draw
uniform vec4      uDiffuse;
uniform vec3      uSpecular;
uniform vec3      uEmissive;
uniform float     uShininess;
uniform bool      uUnlit;
uniform int       uAlphaMode;
uniform float     uAlphaCutoff;
uniform bool      uHasNormals;
uniform bool      uHasDiffuseMap;
uniform bool      uHasSpecularMap;
uniform bool      uHasOpacityMap;
uniform sampler2D uDiffuseMap;
uniform sampler2D uSpecularMap;
uniform sampler2D uOpacityMap;
uniform bool      uHasNormalMap;
uniform float     uNormalScale;
uniform sampler2D uNormalMap;

float shadowVisibility(vec3 position, vec3 normal)
{
    // The light's projection is orthographic: w = 1, no divide needed.
    vec4 clip = uShadowViewProjection * vec4(position + normal * uShadowNormalOffset, 1.0);
    float u = clip.x * 0.5 + 0.5;
    float v = 0.5 - clip.y * 0.5;
    float depth = clip.z * 0.5 + 0.5 - uShadowDepthBias;

    if (!(u >= 0.0 && u < 1.0 && v >= 0.0 && v < 1.0) || depth > 1.0)
        return 1.0;

    int cx = int(u * float(uShadowSize));
    int cy = int(v * float(uShadowSize));
    int lit = 0;
    int taps = 0;

    for (int dy = -uShadowPcfRadius; dy <= uShadowPcfRadius; ++dy) {
        int y = clamp(cy + dy, 0, uShadowSize - 1);
        for (int dx = -uShadowPcfRadius; dx <= uShadowPcfRadius; ++dx) {
            int x = clamp(cx + dx, 0, uShadowSize - 1);
            // v grows downward, texture rows upward.
            lit += depth <= texelFetch(uShadowMap, ivec2(x, uShadowSize - 1 - y), 0).r ? 1 : 0;
            ++taps;
        }
    }
    return float(lit) / float(taps);
}

void main()
{
    vec4 albedo = uDiffuse * vColor;
    if (uHasDiffuseMap)
        albedo *= texture(uDiffuseMap, vUV);
    if (uHasOpacityMap)
        albedo.a *= texture(uOpacityMap, vUV).r;
    if (uAlphaMode == MASK && albedo.a < uAlphaCutoff)
        discard;

    float alpha = uAlphaMode == BLEND ? albedo.a : 1.0;
    if (uUnlit) {
        fragColor = vec4(albedo.rgb, alpha);
        return;
    }

    vec3 n;
    if (uHasNormals) {
        // Back faces (only drawn for double-sided materials) are lit as seen from behind.
        n = normalize(vNormal);
        if (!gl_FrontFacing)
            n = -n;
    } else {
        // Face normal, always turned toward the viewer.
        n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    }

    if (uHasNormalMap && vTangent.w != 0.0) {
        // Tangent frame re-orthogonalized after interpolation; the map's x/y/z go along t/b/n.
        vec3 t = normalize(vTangent.xyz - n * dot(n, vTangent.xyz));
        vec3 b = cross(n, t) * (vTangent.w < 0.0 ? -1.0 : 1.0);
        vec3 m = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;

        n = normalize(t * (m.x * uNormalScale) + b * (m.y * uNormalScale) + n * m.z);
    }

    vec3 v = uOrthographic ? uViewDirection : normalize(uCameraPosition - vWorld);
    vec3 specular = uSpecular;
    if (uHasSpecularMap)
        specular *= texture(uSpecularMap, vUV).rgb;
    bool hasSpecular = specular.r > 0.0 || specular.g > 0.0 || specular.b > 0.0;
    float shininess = max(uShininess, 1.0);

    vec3 result = uAmbient * albedo.rgb + uEmissive;

    for (int i = 0; i < uLightCount; ++i) {
        vec3 l = uLightToLight[i];
        float attenuation = 1.0;

        if (uLightType[i] != DIRECTIONAL) {
            vec3 toLight = uLightPosition[i] - vWorld;
            float distance = length(toLight);

            if (distance >= uLightRange[i] || distance <= 0.0)
                continue;
            l = toLight / distance;

            float ratio = distance / uLightRange[i];
            float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);
            attenuation = window * window / (distance * distance + 1.0);
            if (uLightType[i] == SPOT)
                attenuation *= smoothstep(uLightCosOuter[i], uLightCosInner[i], dot(-l, uLightDirection[i]));
        }

        float nDotL = dot(n, l);
        if (nDotL <= 0.0 || attenuation <= 0.0)
            continue;
        if (i == uShadowLight) {
            attenuation *= shadowVisibility(vWorld, n);
            if (attenuation <= 0.0)
                continue;
        }

        vec3 radiance = uLightRadiance[i] * attenuation;
        result += radiance * albedo.rgb * nDotL;

        if (hasSpecular) {
            float nDotH = max(dot(n, normalize(l + v)), 0.0);
            result += radiance * specular * pow(nDotH, shininess);
        }
    }

    fragColor = vec4(result, alpha);
}
)glsl";

    // Shadow pass: depth only, plus the alpha test of masked materials.
    inline constexpr const char* SHADOW_VERTEX = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec2 vUV;
out float vAlpha;

void main()
{
    vUV = aUV;
    vAlpha = aColor.a;
    gl_Position = uViewProjection * (uModel * vec4(aPosition, 1.0));
}
)glsl";

    inline constexpr const char* SHADOW_FRAGMENT = R"glsl(
#version 330 core
in vec2 vUV;
in float vAlpha;

uniform bool      uAlphaTest;
uniform float     uDiffuseAlpha;
uniform float     uAlphaCutoff;
uniform bool      uHasDiffuseMap;
uniform bool      uHasOpacityMap;
uniform sampler2D uDiffuseMap;
uniform sampler2D uOpacityMap;

void main()
{
    if (uAlphaTest) {
        float alpha = uDiffuseAlpha * vAlpha;
        if (uHasDiffuseMap)
            alpha *= texture(uDiffuseMap, vUV).a;
        if (uHasOpacityMap)
            alpha *= texture(uOpacityMap, vUV).r;
        if (alpha < uAlphaCutoff)
            discard;
    }
}
)glsl";

    // Full-screen triangle showing a CPU image (ImagePresenter).
    inline constexpr const char* PRESENT_VERTEX = R"glsl(
#version 330 core
out vec2 vUV;

void main()
{
    vec2 position = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    // Image row 0 is the top of the screen.
    vUV = vec2(position.x, 1.0 - position.y);
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

    inline constexpr const char* PRESENT_FRAGMENT = R"glsl(
#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uImage;

void main()
{
    fragColor = texture(uImage, vUV);
}
)glsl";

}
