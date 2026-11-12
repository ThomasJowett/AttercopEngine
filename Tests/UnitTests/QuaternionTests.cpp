#include "vendor/doctest.h"

#include "math/Matrix.h"
#include "math/Quaternion.h"

#include <cmath>

namespace QuaternionTestHelpers
{
	const float k_QuarterTurn = 1.5707963f;

	void CheckSameVector(const Vector3f& a, const Vector3f& b)
	{
		CHECK(a.x == doctest::Approx(b.x).scale(1.0f));
		CHECK(a.y == doctest::Approx(b.y).scale(1.0f));
		CHECK(a.z == doctest::Approx(b.z).scale(1.0f));
	}

	Vector3f Rotate(Quaternion q, Vector3f v) { return q.RotateVectorByQuaternion(v); }

	float Length(const Quaternion& q) { return std::sqrt(q.r * q.r + q.i * q.i + q.j * q.j + q.k * q.k); }

	// The angle a quaternion turns (1, 0, 0) through, about Z
	float AngleAboutZ(const Quaternion& q)
	{
		Vector3f turned = Rotate(q, Vector3f(1.0f, 0.0f, 0.0f));
		return std::atan2(turned.y, turned.x);
	}
}

using namespace QuaternionTestHelpers;

TEST_CASE("An axis and angle rotation turns vectors the right way round")
{
	Quaternion aboutZ(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	CheckSameVector(Rotate(aboutZ, Vector3f(1.0f, 0.0f, 0.0f)), Vector3f(0.0f, 1.0f, 0.0f));

	Quaternion aboutX(Vector3f(2.0f, 0.0f, 0.0f), k_QuarterTurn);	// the axis needn't be unit length
	CheckSameVector(Rotate(aboutX, Vector3f(0.0f, 1.0f, 0.0f)), Vector3f(0.0f, 0.0f, 1.0f));
}

TEST_CASE("Rotating a vector by a quaternion matches its rotation matrix")
{
	Quaternion q(Vector3f(0.3f, -0.7f, 1.2f));
	Vector3f v(1.0f, 2.0f, 3.0f);
	CheckSameVector(Rotate(q, v), Matrix4x4::Rotate(q) * v);
}

TEST_CASE("Multiplying quaternions applies the right-hand one first")
{
	Quaternion a(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	Quaternion b(Vector3f(1.0f, 0.0f, 0.0f), k_QuarterTurn);
	Vector3f v(0.0f, 1.0f, 0.0f);
	CheckSameVector(Rotate(a * b, v), Rotate(a, Rotate(b, v)));
}

TEST_CASE("Slerp goes from the first rotation to the second at a steady rate")
{
	Quaternion start(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
	Quaternion end(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 0.0)) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 0.25)) == doctest::Approx(k_QuarterTurn * 0.25f));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 0.5)) == doctest::Approx(k_QuarterTurn * 0.5f));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 1.0)) == doctest::Approx(k_QuarterTurn));
	CHECK(Length(Quaternion::Slerp(start, end, 0.3)) == doctest::Approx(1.0f));
}

TEST_CASE("Slerp takes the shorter way round")
{
	Quaternion start(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
	Quaternion end(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	Quaternion negatedEnd(-end.r, -end.i, -end.j, -end.k);
	CHECK(AngleAboutZ(Quaternion::Slerp(start, negatedEnd, 0.5)) == doctest::Approx(k_QuarterTurn * 0.5f));
}

TEST_CASE("Nlerp reaches both ends, stays unit length and takes the shorter way round")
{
	Quaternion start(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
	Quaternion end(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	CHECK(AngleAboutZ(Quaternion::Nlerp(start, end, 0.0f)) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(AngleAboutZ(Quaternion::Nlerp(start, end, 1.0f)) == doctest::Approx(k_QuarterTurn));
	CHECK(AngleAboutZ(Quaternion::Nlerp(start, end, 0.5f)) == doctest::Approx(k_QuarterTurn * 0.5f));
	CHECK(Length(Quaternion::Nlerp(start, end, 0.3f)) == doctest::Approx(1.0f));

	Quaternion negatedEnd(-end.r, -end.i, -end.j, -end.k);
	CHECK(AngleAboutZ(Quaternion::Nlerp(start, negatedEnd, 0.5f)) == doctest::Approx(k_QuarterTurn * 0.5f));
}
