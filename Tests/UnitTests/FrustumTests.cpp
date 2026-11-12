#include "vendor/doctest.h"

#include "Math/Frustum.h"

// Built the same way Renderer::BeginScene builds it, before the transpose for upload
static Matrix4x4 ViewProjection(const Matrix4x4& cameraTransform, const Matrix4x4& projection)
{
	return projection * Matrix4x4::Inverse(cameraTransform);
}

static Matrix4x4 Perspective(float nearDepth = 1.0f, float farDepth = 100.0f)
{
	return Matrix4x4::PerspectiveRH((float)PIDIV2, 1.0f, nearDepth, farDepth);
}

TEST_CASE("Default frustum culls nothing")
{
	Frustum frustum;
	CHECK(frustum.Contains(Vector3f(1e6f, -1e6f, 1e6f)));
	CHECK(frustum.Intersects(BoundingBox(Vector3f(1e6f, 1e6f, 1e6f), Vector3f(2e6f, 2e6f, 2e6f))));
}

TEST_CASE("Perspective camera at the origin looks down negative Z")
{
	Frustum frustum(ViewProjection(Matrix4x4(), Perspective()));

	CHECK(frustum.Contains(Vector3f(0.0f, 0.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 0.0f, 5.0f)));    // behind
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 0.0f, -0.5f)));   // before the near plane
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 0.0f, -150.0f))); // past the far plane

	// 90 degree field of view, so the sides are at 45 degrees
	CHECK(frustum.Contains(Vector3f(-4.0f, 0.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(-6.0f, 0.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(6.0f, 0.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 6.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, -6.0f, -5.0f)));
}

TEST_CASE("Planes are normalized and match the near and far depths")
{
	Frustum frustum(ViewProjection(Matrix4x4(), Perspective(1.0f, 100.0f)));

	const Plane& nearPlane = frustum.GetPlane(Frustum::Side::Near);
	CHECK(nearPlane.SignedDistance(Vector3f(0.0f, 0.0f, -1.0f)) == doctest::Approx(0.0f).epsilon(1e-4));
	CHECK(nearPlane.SignedDistance(Vector3f(0.0f, 0.0f, -3.0f)) == doctest::Approx(2.0f).epsilon(1e-4));

	const Plane& farPlane = frustum.GetPlane(Frustum::Side::Far);
	CHECK(farPlane.SignedDistance(Vector3f(0.0f, 0.0f, -100.0f)) == doctest::Approx(0.0f).epsilon(1e-2));
	CHECK(farPlane.SignedDistance(Vector3f(0.0f, 0.0f, -90.0f)) == doctest::Approx(10.0f).epsilon(1e-2));
}

TEST_CASE("Frustum follows a moved and rotated camera")
{
	// Standing at x = 10, turned to look down negative X towards the origin
	Matrix4x4 cameraTransform = Matrix4x4::Translate(Vector3f(10.0f, 0.0f, 0.0f))
		* Matrix4x4::Rotate(Quaternion(Vector3f(0.0f, (float)PIDIV2, 0.0f)));
	Frustum frustum(ViewProjection(cameraTransform, Perspective()));

	CHECK(frustum.Contains(Vector3f(0.0f, 0.0f, 0.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(20.0f, 0.0f, 0.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 0.0f, -15.0f))); // 10 ahead, 15 to the side
}

TEST_CASE("Flipping Y for WebGPU doesn't change what's visible")
{
	Matrix4x4 flipped = Matrix4x4::Scale(Vector3f(1.0f, -1.0f, 1.0f)) * Perspective();
	Frustum frustum(ViewProjection(Matrix4x4(), flipped));

	CHECK(frustum.Contains(Vector3f(0.0f, 4.0f, -5.0f)));
	CHECK(frustum.Contains(Vector3f(0.0f, -4.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 6.0f, -5.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, -6.0f, -5.0f)));
}

TEST_CASE("World space boxes")
{
	Frustum frustum(ViewProjection(Matrix4x4(), Perspective()));

	CHECK(frustum.Intersects(BoundingBox(Vector3f(-1.0f, -1.0f, -6.0f), Vector3f(1.0f, 1.0f, -4.0f))));
	CHECK_FALSE(frustum.Intersects(BoundingBox(Vector3f(-1.0f, -1.0f, 4.0f), Vector3f(1.0f, 1.0f, 6.0f))));
	CHECK_FALSE(frustum.Intersects(BoundingBox(Vector3f(10.0f, -1.0f, -6.0f), Vector3f(12.0f, 1.0f, -4.0f))));

	SUBCASE("Straddling a plane counts as visible")
	{
		CHECK(frustum.Intersects(BoundingBox(Vector3f(4.0f, -1.0f, -6.0f), Vector3f(8.0f, 1.0f, -4.0f))));
	}

	SUBCASE("Enclosing the whole frustum counts as visible")
	{
		CHECK(frustum.Intersects(BoundingBox(Vector3f(-1000.0f, -1000.0f, -1000.0f), Vector3f(1000.0f, 1000.0f, 1000.0f))));
	}

	SUBCASE("Invalid boxes are never culled")
	{
		CHECK(frustum.Intersects(BoundingBox()));
	}
}

TEST_CASE("Local space boxes with a transform")
{
	Frustum frustum(ViewProjection(Matrix4x4(), Perspective()));
	BoundingBox unitBox(Vector3f(-0.5f, -0.5f, -0.5f), Vector3f(0.5f, 0.5f, 0.5f));

	CHECK(frustum.Intersects(unitBox, Matrix4x4::Translate(Vector3f(0.0f, 0.0f, -5.0f))));
	CHECK_FALSE(frustum.Intersects(unitBox, Matrix4x4::Translate(Vector3f(-20.0f, 0.0f, -5.0f))));

	SUBCASE("Scale reaches into view")
	{
		Matrix4x4 transform = Matrix4x4::Translate(Vector3f(-20.0f, 0.0f, -5.0f)) * Matrix4x4::Scale(Vector3f(40.0f, 1.0f, 1.0f));
		CHECK(frustum.Intersects(unitBox, transform));
	}

	SUBCASE("Rotation reaches into view")
	{
		// A long thin box off to the left, turned so it points across the view
		BoundingBox longBox(Vector3f(-0.5f, -0.5f, -20.0f), Vector3f(0.5f, 0.5f, 20.0f));
		Matrix4x4 transform = Matrix4x4::Translate(Vector3f(-15.0f, 0.0f, -10.0f)) * Matrix4x4::RotateY((float)PIDIV2);
		CHECK_FALSE(frustum.Intersects(longBox, Matrix4x4::Translate(Vector3f(-60.0f, 0.0f, -10.0f))));
		CHECK(frustum.Intersects(longBox, transform));
	}
}

TEST_CASE("Orthographic 2D editor camera")
{
	Matrix4x4 projection = Matrix4x4::OrthographicRH(-2.0f, 2.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	Frustum frustum(ViewProjection(Matrix4x4::Translate(Vector3f(100.0f, 0.0f, 0.0f)), projection));

	CHECK(frustum.Contains(Vector3f(100.0f, 0.0f, 0.0f)));
	CHECK(frustum.Contains(Vector3f(101.5f, 0.5f, 0.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(0.0f, 0.0f, 0.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(102.5f, 0.0f, 0.0f)));
	CHECK_FALSE(frustum.Contains(Vector3f(100.0f, 1.5f, 0.0f)));
}

TEST_CASE("Quad corner points")
{
	Matrix4x4 projection = Matrix4x4::OrthographicRH(-2.0f, 2.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	Frustum frustum(ViewProjection(Matrix4x4(), projection));

	auto quad = [](float x, float y, float halfWidth, float halfHeight) {
		struct Corners { Vector3f points[4]; } corners = { {
			{ x - halfWidth, y - halfHeight, 0.0f }, { x + halfWidth, y - halfHeight, 0.0f },
			{ x + halfWidth, y + halfHeight, 0.0f }, { x - halfWidth, y + halfHeight, 0.0f } } };
		return corners;
	};

	CHECK(frustum.Intersects(quad(0.0f, 0.0f, 0.5f, 0.5f).points, 4));
	CHECK_FALSE(frustum.Intersects(quad(10.0f, 0.0f, 0.5f, 0.5f).points, 4));

	SUBCASE("Partly on screen")
	{
		CHECK(frustum.Intersects(quad(2.2f, 0.0f, 0.5f, 0.5f).points, 4));
	}

	SUBCASE("Covering the whole view with every corner off screen")
	{
		CHECK(frustum.Intersects(quad(0.0f, 0.0f, 50.0f, 50.0f).points, 4));
	}
}
