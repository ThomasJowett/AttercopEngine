#include "vendor/doctest.h"

#include "Math/MathUtils.h"
#include "math/Matrix.h"
#include "math/Line3D.h"

#include <cmath>

namespace MatrixTestHelpers
{
	void CheckSameMatrix(const Matrix4x4& a, const Matrix4x4& b, float epsilon = 1e-4f)
	{
		for (int row = 0; row < 4; row++)
			for (int column = 0; column < 4; column++)
			{
				CAPTURE(row);
				CAPTURE(column);
				CHECK(a(row, column) == doctest::Approx(b(row, column)).epsilon(epsilon).scale(1.0f));
			}
	}

	Matrix4x4 Trs(const Vector3f& translation, const Vector3f& euler, const Vector3f& scale)
	{
		return Matrix4x4::Translate(translation) * Matrix4x4::Rotate(Quaternion(euler)) * Matrix4x4::Scale(scale);
	}

	// Clip-space depth of a point at this view-space depth, after the divide by w
	float ClipDepth(const Matrix4x4& projection, float viewZ)
	{
		Vector4f clip = projection * Vector4f(0.0f, 0.0f, viewZ, 1.0f);
		return clip.z / clip.w;
	}

	float ClipX(const Matrix4x4& projection, float viewX)
	{
		Vector4f clip = projection * Vector4f(viewX, 0.0f, 0.0f, 1.0f);
		return clip.x / clip.w;
	}
}

using namespace MatrixTestHelpers;

TEST_CASE("A matrix times its inverse is the identity")
{
	CheckSameMatrix(Matrix4x4::Inverse(Matrix4x4()), Matrix4x4());

	Matrix4x4 trs = Trs(Vector3f(3.0f, -2.0f, 5.0f), Vector3f(0.3f, -0.7f, 1.2f), Vector3f(2.0f, 0.5f, 3.0f));
	CheckSameMatrix(trs * Matrix4x4::Inverse(trs), Matrix4x4());
	CheckSameMatrix(Matrix4x4::Inverse(trs) * trs, Matrix4x4());

	Matrix4x4 perspective = Matrix4x4::PerspectiveRH(1.0f, 1.5f, 0.1f, 100.0f);
	CheckSameMatrix(perspective * Matrix4x4::Inverse(perspective), Matrix4x4());

	Matrix4x4 orthographic = Matrix4x4::OrthographicRH(-8.0f, 8.0f, -4.5f, 4.5f, -1.0f, 1.0f);
	CheckSameMatrix(orthographic * Matrix4x4::Inverse(orthographic), Matrix4x4());
}

TEST_CASE("The determinant is the product of the scale, for any rotation")
{
	CHECK(Matrix4x4::Determinant(Matrix4x4()) == doctest::Approx(1.0f));
	CHECK(Matrix4x4::Determinant(Matrix4x4::Scale(Vector3f(2.0f, 3.0f, 4.0f))) == doctest::Approx(24.0f));
	Matrix4x4 trs = Trs(Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.4f, 1.1f, -0.6f), Vector3f(2.0f, 3.0f, 4.0f));
	CHECK(Matrix4x4::Determinant(trs) == doctest::Approx(24.0f));
	CHECK(Matrix4x4::Determinant(Matrix4x4::Scale(Vector3f(-1.0f, 1.0f, 1.0f))) == doctest::Approx(-1.0f));
}

TEST_CASE("Decompose gives back the translation, rotation and scale of a matrix")
{
	Matrix4x4 trs = Trs(Vector3f(3.0f, -2.0f, 5.0f), Vector3f(0.3f, -0.7f, 1.2f), Vector3f(2.0f, 0.5f, 3.0f));
	Vector3f translation, rotation, scale;
	trs.Decompose(translation, rotation, scale);
	CHECK(translation == Vector3f(3.0f, -2.0f, 5.0f));
	CHECK(scale.x == doctest::Approx(2.0f));
	CHECK(scale.y == doctest::Approx(0.5f));
	CHECK(scale.z == doctest::Approx(3.0f));
	CheckSameMatrix(Trs(translation, rotation, scale), trs);
}

TEST_CASE("Decompose gives back a mirrored matrix")
{
	Matrix4x4 mirrored = Trs(Vector3f(1.0f, 2.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.6f), Vector3f(-1.0f, 1.0f, 1.0f));
	Vector3f translation, rotation, scale;
	mirrored.Decompose(translation, rotation, scale);
	CheckSameMatrix(Trs(translation, rotation, scale), mirrored);
}

TEST_CASE("A right-handed perspective maps the near plane to depth 0 and the far plane to 1")
{
	Matrix4x4 projection = Matrix4x4::PerspectiveRH(1.0f, 1.5f, 0.5f, 50.0f);
	CHECK(ClipDepth(projection, -0.5f) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(ClipDepth(projection, -50.0f) == doctest::Approx(1.0f));
}

TEST_CASE("A right-handed orthographic projection maps symmetric near and far to depth 0 and 1")
{
	Matrix4x4 projection = Matrix4x4::OrthographicRH(-8.0f, 8.0f, -4.5f, 4.5f, -1.0f, 1.0f);
	CHECK(ClipDepth(projection, 1.0f) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(ClipDepth(projection, -1.0f) == doctest::Approx(1.0f));
	CHECK(ClipX(projection, 8.0f) == doctest::Approx(1.0f));
	CHECK(ClipX(projection, -8.0f) == doctest::Approx(-1.0f));
}

TEST_CASE("A right-handed orthographic projection maps any near and far to depth 0 and 1")
{
	Matrix4x4 projection = Matrix4x4::OrthographicRH(-8.0f, 8.0f, -4.5f, 4.5f, -2.0f, 3.0f);
	CHECK(ClipDepth(projection, 2.0f) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(ClipDepth(projection, -3.0f) == doctest::Approx(1.0f));
}

TEST_CASE("An off-centre orthographic projection maps its edges to -1 and 1")
{
	Matrix4x4 projection = Matrix4x4::OrthographicRH(0.0f, 4.0f, 0.0f, 2.0f, -1.0f, 1.0f);
	CHECK(ClipX(projection, 0.0f) == doctest::Approx(-1.0f));
	CHECK(ClipX(projection, 4.0f) == doctest::Approx(1.0f));
}

TEST_CASE("A screen point's camera ray passes through the world point drawn there")
{
	Vector2f viewport(800.0f, 600.0f);
	Matrix4x4 camera = Matrix4x4::Translate(Vector3f(2.0f, 1.0f, 10.0f)) * Matrix4x4::RotateY(0.2f);
	Matrix4x4 view = Matrix4x4::Inverse(camera);

	Matrix4x4 projections[] = {
		Matrix4x4::PerspectiveRH(1.0f, viewport.x / viewport.y, 0.1f, 100.0f),
		Matrix4x4::OrthographicRH(-8.0f, 8.0f, -6.0f, 6.0f, -50.0f, 50.0f),
	};
	for (const Matrix4x4& projection : projections)
	{
		Vector3f world(1.5f, -0.5f, 2.0f);
		Vector3f screen = MathUtils::WorldToScreenSpace(view, projection, world, viewport);
		CHECK(screen.x > 0.0f);
		CHECK(screen.x < viewport.x);

		Line3D ray = MathUtils::ComputeCameraRay(view, projection, Vector2f(screen.x, screen.y), viewport);
		CHECK(ray.DistanceToPoint(world) == doctest::Approx(0.0f).scale(1.0f).epsilon(1e-3));
	}
}

TEST_CASE("A screen point unprojects onto the plane at the chosen depth")
{
	Vector2f viewport(800.0f, 600.0f);
	Matrix4x4 view = Matrix4x4::Inverse(Matrix4x4::Translate(Vector3f(0.0f, 0.0f, 10.0f)));
	Matrix4x4 projection = Matrix4x4::PerspectiveRH(1.0f, viewport.x / viewport.y, 0.1f, 100.0f);

	Vector3f world(3.0f, -2.0f, 1.5f);
	Vector3f screen = MathUtils::WorldToScreenSpace(view, projection, world, viewport);
	Vector3f back = MathUtils::ScreenToWorldPoint(view, projection, Vector2f(screen.x, screen.y), viewport, 1.5f);
	CHECK(back.x == doctest::Approx(3.0f).epsilon(1e-3));
	CHECK(back.y == doctest::Approx(-2.0f).epsilon(1e-3));
	CHECK(back.z == doctest::Approx(1.5f).epsilon(1e-3));

	// The centre of the screen is straight ahead
	Vector3f centre = MathUtils::ScreenToWorldPoint(view, projection, viewport * 0.5f, viewport, 0.0f);
	CHECK(centre.x == doctest::Approx(0.0f).scale(1.0f).epsilon(1e-3));
	CHECK(centre.y == doctest::Approx(0.0f).scale(1.0f).epsilon(1e-3));
}
