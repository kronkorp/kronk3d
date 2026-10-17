#include <kronk3d/scene/BillboardBatch.hpp>
#include <algorithm>
#include <cmath>

void k3::BillboardBatch::draw(IRasterizer& rasterizer, const Camera& camera, std::shared_ptr<const Material> material)
{
    if (m_billboards.empty())
        return;

    const Math::Vector3f right = camera.right(), up = camera.up(), back = -1.f * camera.forward();

    // Farthest first: blended quads overlap in the order they are listed.
    const Math::Vector3f forward = camera.forward();
    std::stable_sort(m_billboards.begin(), m_billboards.end(), [&](const Billboard& a, const Billboard& b) {
        return Math::Vector3f::dot(a.position - camera.position, forward) > Math::Vector3f::dot(b.position - camera.position, forward);
    });

    const std::size_t count = m_billboards.size();
    Mesh& mesh = *m_mesh;

    mesh.positions.resize(count * 4);
    mesh.normals.assign(count * 4, back);       // Lit materials see the quads facing the camera
    mesh.uvs.resize(count * 4);
    mesh.colors.resize(count * 4);
    if (mesh.indices.size() != count * 6) {
        mesh.indices.resize(count * 6);
        for (std::size_t i = 0; i < count; ++i) {
            const auto first = static_cast<std::uint32_t>(i * 4);
            const std::uint32_t quad[6] = {first, first + 1, first + 2, first, first + 2, first + 3};

            std::copy(std::begin(quad), std::end(quad), mesh.indices.begin() + static_cast<std::ptrdiff_t>(i * 6));
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        const Billboard& b = m_billboards[i];
        const float c = std::cos(b.rotation), s = std::sin(b.rotation);
        const Math::Vector3f axisX = (right * c + up * s) * (b.size.x * 0.5f);      // The quad's half width and half height,
        const Math::Vector3f axisY = (up * c - right * s) * (b.size.y * 0.5f);      // turned around the view axis

        // Bottom left, bottom right, top right, top left, as seen by the camera.
        const std::size_t v = i * 4;
        mesh.positions[v + 0] = b.position - axisX - axisY;
        mesh.positions[v + 1] = b.position + axisX - axisY;
        mesh.positions[v + 2] = b.position + axisX + axisY;
        mesh.positions[v + 3] = b.position - axisX + axisY;
        mesh.uvs[v + 0] = {b.uv.x, b.uv.w};
        mesh.uvs[v + 1] = {b.uv.z, b.uv.w};
        mesh.uvs[v + 2] = {b.uv.z, b.uv.y};
        mesh.uvs[v + 3] = {b.uv.x, b.uv.y};
        for (std::size_t k = 0; k < 4; ++k)
            mesh.colors[v + k] = b.color;
    }

    ++mesh.revision;
    rasterizer.draw(m_mesh, std::move(material), Math::Matrix4::identity());
}
