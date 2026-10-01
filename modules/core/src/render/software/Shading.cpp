#include "Shading.hpp"
#include "Sampling.hpp"

k3::sw::ShadingContext k3::sw::ShadingContext::prepare(const Camera& camera, const Environment& environment)
{
    return {camera.position, environment.ambient};
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

k3::Math::Color k3::sw::shade(const ShadingContext& /*context*/, const Material& material, const Fragment& fragment) noexcept
{
    Math::Color albedo = material.diffuse * fragment.color * sampleTexture(material.diffuseMap.get(), fragment.uv, fragment.lod);

    if (material.opacityMap)
        albedo.a *= sampleTexture(material.opacityMap.get(), fragment.uv, fragment.lod).r;
    return albedo;
}
