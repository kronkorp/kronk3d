#include "Shading.hpp"
#include "Sampling.hpp"
#include <algorithm>
#include <cmath>

namespace
{

    using k3::Math::Vector3f;

    float smoothstep(float edge0, float edge1, float x) noexcept
    {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    // Inverse-square falloff, windowed so the light reaches exactly 0 at its range.
    float distanceAttenuation(float distance, float range) noexcept
    {
        const float ratio = distance / range;
        const float window = std::clamp(1.f - ratio * ratio * ratio * ratio, 0.f, 1.f);
        return window * window / (distance * distance + 1.f);
    }

}

k3::sw::ShadingContext k3::sw::ShadingContext::prepare(const Camera& camera, const Environment& environment)
{
    ShadingContext context;

    context.cameraPosition = camera.position;
    context.viewDirection = -camera.forward();
    context.orthographic = camera.projection == Projection::Orthographic;
    context.ambient = environment.ambient;
    for (const Light& light : environment.lights) {
        if (context.lightCount == MAX_LIGHTS)
            break;

        const Vector3f direction = Vector3f::normalize(light.direction);
        context.lights[context.lightCount++] = {
            light.type,
            light.position,
            -direction,
            direction,
            light.color * light.intensity,
            std::max(light.range, 1e-4f),
            std::cos(light.innerCone),
            std::cos(std::max(light.outerCone, light.innerCone + 1e-4f)),
        };
    }
    return context;
}

float k3::sw::coverage(const Material& material, const Fragment& fragment) noexcept
{
    float alpha = material.diffuse.a * fragment.color.a;

    if (material.diffuseMap)
        alpha *= sampleTexture(material.diffuseMap.get(), fragment.uv, fragment.lod).a;
    if (material.opacityMap)
        alpha *= sampleTexture(material.opacityMap.get(), fragment.uv, fragment.lod).r;
    return alpha;
}

k3::Math::Color k3::sw::shade(const ShadingContext& context, const Material& material, const Fragment& fragment) noexcept
{
    Math::Color albedo = material.diffuse * fragment.color * sampleTexture(material.diffuseMap.get(), fragment.uv, fragment.lod);

    if (material.opacityMap)
        albedo.a *= sampleTexture(material.opacityMap.get(), fragment.uv, fragment.lod).r;
    if (material.unlit)
        return albedo;

    // Back faces (only drawn for double-sided materials) are lit as seen from behind.
    Vector3f n = Vector3f::normalize(fragment.normal);
    if (!fragment.frontFacing)
        n = -n;

    const Vector3f v = context.orthographic ? context.viewDirection : Vector3f::normalize(context.cameraPosition - fragment.position);
    Math::Color specular = material.specular;
    if (material.specularMap)
        specular = specular * sampleTexture(material.specularMap.get(), fragment.uv, fragment.lod);
    const bool hasSpecular = specular.r > 0.f || specular.g > 0.f || specular.b > 0.f;
    const float shininess = std::max(material.shininess, 1.f);

    Math::Color result = context.ambient * albedo + material.emissive;

    for (std::size_t i = 0; i < context.lightCount; ++i) {
        const PreparedLight& light = context.lights[i];
        Vector3f l = light.toLight;
        float attenuation = 1.f;

        if (light.type != LightType::Directional) {
            const Vector3f toLight = light.position - fragment.position;
            const float distance = Vector3f::length(toLight);

            if (distance >= light.range || distance <= 0.f)
                continue;
            l = toLight / distance;
            attenuation = distanceAttenuation(distance, light.range);
            if (light.type == LightType::Spot)
                attenuation *= smoothstep(light.cosOuter, light.cosInner, Vector3f::dot(-l, light.direction));
        }

        const float nDotL = Vector3f::dot(n, l);
        if (nDotL <= 0.f || attenuation <= 0.f)
            continue;

        const Math::Color radiance = light.radiance * attenuation;
        result += radiance * albedo * nDotL;

        if (hasSpecular) {
            const float nDotH = std::max(Vector3f::dot(n, Vector3f::normalize(l + v)), 0.f);
            result += radiance * specular * std::pow(nDotH, shininess);
        }
    }

    result.a = albedo.a;
    return result;
}
