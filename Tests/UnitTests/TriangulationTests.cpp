#include "vendor/doctest.h"

#include "Math/Triangulation.h"

#include <cmath>
#include <set>
#include <vector>

// Inputs here are ones that finish: a polygon where no ear is found loops forever

namespace TriangulationTestHelpers
{
	float SignedArea(const Vector2f& a, const Vector2f& b, const Vector2f& c)
	{
		return 0.5f * ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
	}

	float PolygonArea(const std::vector<Vector2f>& polygon)
	{
		float area = 0.0f;
		for (size_t i = 0; i < polygon.size(); i++)
		{
			const Vector2f& a = polygon[i];
			const Vector2f& b = polygon[(i + 1) % polygon.size()];
			area += a.x * b.y - b.x * a.y;
		}
		return std::abs(area) * 0.5f;
	}

	bool InsidePolygon(const std::vector<Vector2f>& polygon, const Vector2f& point)
	{
		bool inside = false;
		for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
		{
			const Vector2f& a = polygon[i];
			const Vector2f& b = polygon[j];
			if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
				inside = !inside;
		}
		return inside;
	}

	// n - 2 triangles of valid, distinct indices that cover the polygon's area and stay inside it
	void CheckTriangulation(const std::vector<Vector2f>& polygon)
	{
		std::vector<uint32_t> triangles;
		REQUIRE(Triangulation::Triangulate(polygon, triangles));
		REQUIRE(triangles.size() == (polygon.size() - 2) * 3);

		float area = 0.0f;
		for (size_t t = 0; t < triangles.size(); t += 3)
		{
			std::set<uint32_t> corners = { triangles[t], triangles[t + 1], triangles[t + 2] };
			CHECK(corners.size() == 3);
			for (uint32_t index : corners)
				REQUIRE(index < polygon.size());

			const Vector2f& a = polygon[triangles[t]];
			const Vector2f& b = polygon[triangles[t + 1]];
			const Vector2f& c = polygon[triangles[t + 2]];
			area += std::abs(SignedArea(a, b, c));

			Vector2f centre((a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f);
			CHECK(InsidePolygon(polygon, centre));
		}
		CHECK(area == doctest::Approx(PolygonArea(polygon)));
	}

	std::vector<Vector2f> Reversed(std::vector<Vector2f> polygon)
	{
		return std::vector<Vector2f>(polygon.rbegin(), polygon.rend());
	}

	// Clockwise with y up
	const std::vector<Vector2f> k_Square = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } };
	const std::vector<Vector2f> k_LShape = { { 0.0f, 0.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 1.0f, 1.0f }, { 3.0f, 1.0f }, { 3.0f, 0.0f } };
	const std::vector<Vector2f> k_Arrow = { { 0.0f, 0.0f }, { 2.0f, 4.0f }, { 4.0f, 0.0f }, { 2.0f, 1.0f } };
}

using namespace TriangulationTestHelpers;

TEST_CASE("Winding is clockwise when the shoelace area is negative with y up")
{
	CHECK(Triangulation::IsClockwiseWinding(k_Square));
	CHECK_FALSE(Triangulation::IsClockwiseWinding(Reversed(k_Square)));
	CHECK(Triangulation::IsClockwiseWinding(k_LShape));
}

TEST_CASE("A point in a clockwise triangle is inside it")
{
	Vector2f a(0.0f, 0.0f), b(0.0f, 2.0f), c(2.0f, 0.0f);
	CHECK(Triangulation::IsPointInTriangle(Vector2f(0.5f, 0.5f), a, b, c));
	CHECK_FALSE(Triangulation::IsPointInTriangle(Vector2f(2.0f, 2.0f), a, b, c));
	CHECK_FALSE(Triangulation::IsPointInTriangle(Vector2f(-0.5f, 0.5f), a, b, c));
}

TEST_CASE("A polygon with fewer than 3 vertices can't be triangulated")
{
	std::vector<uint32_t> triangles;
	CHECK_FALSE(Triangulation::Triangulate({ { 0.0f, 0.0f }, { 1.0f, 0.0f } }, triangles));
}

TEST_CASE("Convex polygons are triangulated in either winding")
{
	CheckTriangulation(k_Square);
	CheckTriangulation(Reversed(k_Square));

	// The polygon collider's default pentagon, which is anticlockwise
	std::vector<Vector2f> pentagon = { { 0.0f, 1.0f }, { -0.9510565f, 0.309017f }, { -0.5877852f, -0.8090171f },
		{ 0.5877854f, -0.8090169f }, { 0.9510565f, 0.3090171f } };
	CheckTriangulation(pentagon);
}

TEST_CASE("Concave polygons are triangulated inside their outline")
{
	CheckTriangulation(k_LShape);
	CheckTriangulation(Reversed(k_LShape));
	CheckTriangulation(k_Arrow);
	CheckTriangulation(Reversed(k_Arrow));
}

TEST_CASE("Triangles keep the polygon's winding")
{
	for (const std::vector<Vector2f>& polygon : { k_LShape, Reversed(k_LShape) })
	{
		bool clockwise = Triangulation::IsClockwiseWinding(polygon);
		std::vector<uint32_t> triangles;
		REQUIRE(Triangulation::Triangulate(polygon, triangles));
		for (size_t t = 0; t < triangles.size(); t += 3)
		{
			float area = SignedArea(polygon[triangles[t]], polygon[triangles[t + 1]], polygon[triangles[t + 2]]);
			CHECK((clockwise ? area < 0.0f : area > 0.0f));
		}
	}
}
