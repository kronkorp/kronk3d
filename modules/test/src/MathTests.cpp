#include "Test.hpp"
#include "Bounds.hpp"
#include "Matrix.hpp"
#include "Vector.hpp"

using namespace k3::Math;

static bool nearIdentity(const Matrix4& m)
{
    for (int i = 0; i < 16; ++i)
        if (std::abs(m.values[i] - ((i % 5 == 0) ? 1.f : 0.f)) > 1e-4f)
            return false;
    return true;
}

K3_TEST(matrix_inverse_of_transform_gives_identity)
{
    const Matrix4 m = Matrix4::translate({1.f, -2.f, 3.f}) * Matrix4::rotate({1.f, 2.f, 3.f}, 0.7f) * Matrix4::scale({2.f, 0.5f, 4.f});

    K3_CHECK(nearIdentity(m * m.inverse()));
    K3_CHECK(nearIdentity(m.inverse() * m));
}

K3_TEST(matrix_inverse_of_perspective_gives_identity)
{
    const Matrix4 p = Matrix4::perspective(0.1f, 100.f, 1.f, 1.5f);

    K3_CHECK(nearIdentity(p * p.inverse()));
}

K3_TEST(matrix_look_at_moves_eye_to_origin_and_target_to_minus_z)
{
    const Vector3f eye{3.f, 2.f, 5.f};
    const Vector3f target{0.f, 1.f, 0.f};
    const Matrix4 view = Matrix4::lookAt(eye, target, {0.f, 1.f, 0.f});

    const Vector3f e = view.transformPoint(eye);
    const Vector3f t = view.transformPoint(target);

    K3_CHECK_NEAR(Vector3f::length(e), 0.f, 1e-5f);
    K3_CHECK_NEAR(t.x, 0.f, 1e-5f);
    K3_CHECK_NEAR(t.y, 0.f, 1e-5f);
    K3_CHECK_NEAR(t.z, -Vector3f::length(target - eye), 1e-4f);
}

K3_TEST(matrix_normal_matrix_keeps_normals_perpendicular_under_non_uniform_scale)
{
    const Matrix4 m = Matrix4::scale({4.f, 1.f, 1.f});
    // Plane x = y, normal (1, -1, 0), tangent (1, 1, 0).
    const Vector3f tangent = m.transformDirection({1.f, 1.f, 0.f});
    const Vector3f normal = m.normalMatrix().transformDirection({1.f, -1.f, 0.f});

    K3_CHECK_NEAR(Vector3f::dot(tangent, normal), 0.f, 1e-5f);
}

K3_TEST(vector_cross_and_normalize)
{
    const Vector3f z = Vector3f::cross({1.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

    K3_CHECK_NEAR(z.z, 1.f, 1e-6f);
    K3_CHECK_NEAR(Vector3f::length(Vector3f::normalize({3.f, 4.f, 12.f})), 1.f, 1e-6f);
    // Zero vector stays zero instead of becoming NaN.
    K3_CHECK(Vector3f::normalize({}).x == 0.f);
}

K3_TEST(bounds_transformed_encloses_rotated_box)
{
    Bounds3f box;
    box.expand({-1.f, -1.f, -1.f});
    box.expand({1.f, 1.f, 1.f});

    const Bounds3f rotated = box.transformed(Matrix4::rotateXY(0.785398f));

    K3_CHECK_NEAR(rotated.max.x, 1.41421f, 1e-4f);
    K3_CHECK_NEAR(rotated.max.z, 1.f, 1e-5f);
    K3_CHECK(Bounds3f{}.empty());
}
