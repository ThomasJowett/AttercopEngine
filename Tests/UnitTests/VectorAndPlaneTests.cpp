#include "vendor/doctest.h"

#include "Math/BoundingBox.h"
#include "math/Line3D.h"
#include "math/Plane.h"
#include "math/Vector2f.h"
#include "math/Vector3f.h"
#include "math/Vector4f.h"

#include <cmath>
#include <vector>

namespace VectorTestHelpers
{
	const float k_RootHalf = 0.70710678f;
}

using namespace VectorTestHelpers;

TEST_CASE("Cross products follow the right-hand rule")
{
	CHECK(Vector3f::Cross(Vector3f(1.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f)) == Vector3f(0.0f, 0.0f, 1.0f));
	CHECK(Vector3f::Cross(Vector3f(0.0f, 1.0f, 0.0f), Vector3f(1.0f, 0.0f, 0.0f)) == Vector3f(0.0f, 0.0f, -1.0f));
	CHECK(Vector2f::Cross(Vector2f(1.0f, 0.0f), Vector2f(0.0f, 1.0f)) == 1.0f);
}

TEST_CASE("Reflect bounces a vector off a surface, whatever the normal's length")
{
	CHECK(Vector2f::Reflect(Vector2f(1.0f, -1.0f), Vector2f(0.0f, 3.0f)) == Vector2f(1.0f, 1.0f));
	CHECK(Vector3f::Reflect(Vector3f(1.0f, -2.0f, 0.5f), Vector3f(0.0f, 2.0f, 0.0f)) == Vector3f(1.0f, 2.0f, 0.5f));
}

TEST_CASE("Lerp halfway is the midpoint")
{
	CHECK(Vector2f::Lerp(Vector2f(0.0f, 0.0f), Vector2f(4.0f, 2.0f), 0.5f) == Vector2f(2.0f, 1.0f));
	CHECK(Vector3f::Lerp(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(4.0f, 2.0f, 6.0f), 0.5f) == Vector3f(2.0f, 1.0f, 3.0f));
}

TEST_CASE("Lerp gives the first vector at 0 and the second at 1")
{
	Vector2f a(1.0f, 2.0f), b(5.0f, 6.0f);
	CHECK(Vector2f::Lerp(a, b, 0.0f) == a);
	CHECK(Vector2f::Lerp(a, b, 1.0f) == b);
	CHECK(Vector2f::Lerp(a, b, 0.25f) == Vector2f(2.0f, 3.0f));
	Vector3f c(1.0f, 2.0f, 3.0f), d(5.0f, 6.0f, 7.0f);
	CHECK(Vector3f::Lerp(c, d, 0.0f) == c);
	CHECK(Vector3f::Lerp(c, d, 1.0f) == d);
}

TEST_CASE("Vector3f Slerp turns between unit vectors at a steady rate")
{
	Vector3f result = Vector3f::Slerp(Vector3f(1.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f), 0.5f);
	CHECK(result.x == doctest::Approx(k_RootHalf));
	CHECK(result.y == doctest::Approx(k_RootHalf));
	CHECK(result.z == doctest::Approx(0.0f).scale(1.0f));
}

TEST_CASE("Vector2f Slerp between perpendicular vectors")
{
	Vector2f result = Vector2f::Slerp(Vector2f(1.0f, 0.0f), Vector2f(0.0f, 1.0f), 0.5f);
	CHECK(result.x == doctest::Approx(k_RootHalf));
	CHECK(result.y == doctest::Approx(k_RootHalf));
}

TEST_CASE("Vector2f Slerp turns between vectors at a steady rate")
{
	Vector2f result = Vector2f::Slerp(Vector2f(1.0f, 0.0f), Vector2f(0.5f, 0.8660254f), 0.5f);
	CHECK(result.x == doctest::Approx(0.8660254f));
	CHECK(result.y == doctest::Approx(0.5f));

	// Inputs of any length, and the same or opposite directions
	Vector2f quarter = Vector2f::Slerp(Vector2f(3.0f, 0.0f), Vector2f(0.0f, 2.0f), 0.5f);
	CHECK(quarter.x == doctest::Approx(0.70710678f));
	CHECK(quarter.y == doctest::Approx(0.70710678f));
	CHECK(Vector2f::Slerp(Vector2f(1.0f, 0.0f), Vector2f(2.0f, 0.0f), 0.5f) == Vector2f(1.0f, 0.0f));
	Vector2f half = Vector2f::Slerp(Vector2f(1.0f, 0.0f), Vector2f(-1.0f, 0.0f), 0.5f);
	CHECK(half.x == doctest::Approx(0.0f).scale(1.0f));
	CHECK(std::abs(half.y) == doctest::Approx(1.0f));
}

TEST_CASE("Angle between unit vectors")
{
	CHECK(Vector2f::Angle(Vector2f(1.0f, 0.0f), Vector2f(0.0f, 1.0f)) == doctest::Approx(1.5707963f));
	CHECK(Vector2f::Angle(Vector2f(1.0f, 0.0f), Vector2f(-1.0f, 0.0f)) == doctest::Approx(3.1415926f));
}

TEST_CASE("Angle between vectors of any length")
{
	CHECK(Vector2f::Angle(Vector2f(1.0f, 0.0f), Vector2f(1.0f, 1.0f)) == doctest::Approx(0.7853982f));
	CHECK(Vector2f::Angle(Vector2f(2.0f, 0.0f), Vector2f(3.0f, 0.0f)) == doctest::Approx(0.0f).scale(1.0f));
}

TEST_CASE("Vector2f helpers with no callers yet")
{
	CHECK(Vector2f::Projection(Vector2f(3.0f, 4.0f), Vector2f(2.0f, 0.0f)) == Vector2f(3.0f, 0.0f));
	CHECK(Vector2f::Projection(Vector2f(3.0f, 4.0f), Vector2f(0.0f, 0.0f)) == Vector2f(0.0f, 0.0f));
	Vector2f up = Vector2f::ForAngle(1.5707963f);
	CHECK(up.x == doctest::Approx(0.0f).scale(1.0f));
	CHECK(up.y == doctest::Approx(1.0f));
	CHECK(Vector2f::Near(Vector2f(1.0f, 1.0f), Vector2f(1.1f, 1.0f), 0.2f));
	CHECK_FALSE(Vector2f::Near(Vector2f(1.0f, 1.0f), Vector2f(1.5f, 1.0f), 0.2f));
}

TEST_CASE("Vector4f Normalize makes a unit vector")
{
	Vector4f v(2.0f, 0.0f, 0.0f, 0.0f);
	v.Normalize();
	CHECK(v.x == doctest::Approx(1.0f));

	Vector4f mixed(1.0f, 1.0f, 1.0f, 1.0f);
	mixed.Normalize();
	CHECK(mixed.w == doctest::Approx(0.5f));

	// A zero vector stays zero
	Vector4f zero(0.0f, 0.0f, 0.0f, 0.0f);
	zero.Normalize();
	CHECK(zero.x == 0.0f);
}

TEST_CASE("A plane through three points")
{
	Plane plane(Vector3f(0.0f, 0.0f, 2.0f), Vector3f(1.0f, 0.0f, 2.0f), Vector3f(0.0f, 1.0f, 2.0f));
	CHECK(plane.SignedDistance(Vector3f(5.0f, -3.0f, 7.0f)) == doctest::Approx(5.0f));
	CHECK(plane.SignedDistance(Vector3f(0.0f, 0.0f, 0.0f)) == doctest::Approx(-2.0f));
	CHECK(plane.UnsignedDistance(Vector3f(0.0f, 0.0f, 0.0f)) == doctest::Approx(2.0f));
}

TEST_CASE("A plane from a point and a normal of any length")
{
	Vector3f normal(0.0f, 4.0f, 0.0f);
	Plane plane(Vector3f(3.0f, 1.0f, 3.0f), normal);
	CHECK(plane.SignedDistance(Vector3f(0.0f, 6.0f, 0.0f)) == doctest::Approx(5.0f));
	Vector3f closest = plane.ClosestPoint(Vector3f(2.0f, 6.0f, -1.0f));
	CHECK(closest == Vector3f(2.0f, 1.0f, -1.0f));
}

TEST_CASE("A line meets a plane, given a unit direction")
{
	Vector3f normal(0.0f, 0.0f, 1.0f);
	Plane ground(Vector3f(0.0f, 0.0f, 0.0f), normal);
	Line3D line(Vector3f(1.0f, 2.0f, 5.0f), Vector3f(0.0f, 0.0f, -1.0f));
	Vector3f hit;
	REQUIRE(Plane::PlaneLineIntersection(ground, line, Vector3f(0.0f, 0.0f, 0.0f), hit));
	CHECK(hit == Vector3f(1.0f, 2.0f, 0.0f));

	Line3D parallel(Vector3f(0.0f, 0.0f, 5.0f), Vector3f(1.0f, 0.0f, 0.0f));
	CHECK_FALSE(Plane::PlaneLineIntersection(ground, parallel, Vector3f(0.0f, 0.0f, 0.0f), hit));
}

TEST_CASE("A line meets a plane, whatever the length of its direction")
{
	Vector3f normal(0.0f, 0.0f, 1.0f);
	Plane ground(Vector3f(0.0f, 0.0f, 0.0f), normal);
	Line3D line(Vector3f(1.0f, 2.0f, 5.0f), Vector3f(0.0f, 0.0f, -2.0f));
	Vector3f hit;
	REQUIRE(Plane::PlaneLineIntersection(ground, line, Vector3f(0.0f, 0.0f, 0.0f), hit));
	CHECK(hit == Vector3f(1.0f, 2.0f, 0.0f));
}

TEST_CASE("Two planes meet in a line")
{
	Plane x(1.0f, 0.0f, 0.0f, 0.0f);
	Plane y(0.0f, 1.0f, 0.0f, 0.0f);
	Line3D line;
	REQUIRE(Plane::PlanePlaneIntersection(x, y, line));
	CHECK(std::abs(line.d.z) == doctest::Approx(1.0f));
	CHECK(x.SignedDistance(line.p) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(y.SignedDistance(line.p) == doctest::Approx(0.0f).scale(1.0f));

	// Planes away from the origin, meeting along a line parallel to Y
	Plane atX(1.0f, 0.0f, 0.0f, -1.0f);
	Plane atZ(0.0f, 0.0f, 1.0f, -2.0f);
	REQUIRE(Plane::PlanePlaneIntersection(atX, atZ, line));
	CHECK(std::abs(line.d.y) == doctest::Approx(1.0f));
	CHECK(atX.SignedDistance(line.p) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(atZ.SignedDistance(line.p) == doctest::Approx(0.0f).scale(1.0f));

	Plane parallel(1.0f, 0.0f, 0.0f, -5.0f);
	CHECK_FALSE(Plane::PlanePlaneIntersection(atX, parallel, line));
}

TEST_CASE("Distances from lines")
{
	Line3D xAxis(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(2.0f, 0.0f, 0.0f));
	CHECK(xAxis.DistanceToPoint(Vector3f(5.0f, 3.0f, 4.0f)) == doctest::Approx(5.0f));

	Line3D raised(Vector3f(0.0f, 0.0f, 3.0f), Vector3f(0.0f, 1.0f, 0.0f));
	CHECK(Line3D::Distance(xAxis, raised) == doctest::Approx(3.0f));
	CHECK(Line3D::SqrDistance(xAxis, raised) == doctest::Approx(9.0f));
}

TEST_CASE("A bounding box encloses strided points and merges with others")
{
	// Position then two other floats per vertex
	std::vector<float> vertices = {
		1.0f, -2.0f, 3.0f, 99.0f, 99.0f,
		-4.0f, 5.0f, 0.5f, -99.0f, -99.0f,
		2.0f, 0.0f, -6.0f, 99.0f, 99.0f,
	};
	BoundingBox box;
	box.EnclosePoints(vertices.data(), 3, 5);
	REQUIRE(box.IsValid());
	CHECK(box.Min() == Vector3f(-4.0f, -2.0f, -6.0f));
	CHECK(box.Max() == Vector3f(2.0f, 5.0f, 3.0f));
	CHECK(box.Center() == Vector3f(-1.0f, 1.5f, -1.5f));

	box.Merge(BoundingBox(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(10.0f, 1.0f, 1.0f)));
	CHECK(box.Max() == Vector3f(10.0f, 5.0f, 3.0f));
	CHECK(box.Min() == Vector3f(-4.0f, -2.0f, -6.0f));

	// Merging an invalid box changes nothing
	box.Merge(BoundingBox());
	CHECK(box.Max() == Vector3f(10.0f, 5.0f, 3.0f));

	BoundingBox grown;
	grown.Merge(box);
	CHECK(grown.Min() == box.Min());
	CHECK(grown.Max() == box.Max());
}
