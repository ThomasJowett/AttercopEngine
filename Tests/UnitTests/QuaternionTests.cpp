#include "vendor/doctest.h"

#include "math/Matrix.h"
#include "math/Quaternion.h"

#include <cmath>
#include <random>
#include <vector>

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

	void CheckSameMatrix(const Quaternion& a, const Quaternion& b)
	{
		Matrix4x4 ma = Matrix4x4::Rotate(a);
		Matrix4x4 mb = Matrix4x4::Rotate(b);
		for (int row = 0; row < 3; row++)
			for (int column = 0; column < 3; column++)
				CHECK(ma(row, column) == doctest::Approx(mb(row, column)).scale(1.0f).epsilon(1e-5));
	}

	void CheckSameAngles(const Vector3f& a, const Vector3f& b)
	{
		CHECK(a.x == doctest::Approx(b.x).scale(1.0f).epsilon(1e-4));
		CHECK(a.y == doctest::Approx(b.y).scale(1.0f).epsilon(1e-4));
		CHECK(a.z == doctest::Approx(b.z).scale(1.0f).epsilon(1e-4));
	}

	Vector3f RandomAngles(std::mt19937& random, float maxPitch)
	{
		std::uniform_real_distribution<float> angle(-3.1f, 3.1f);
		std::uniform_real_distribution<float> pitch(-maxPitch, maxPitch);
		return Vector3f(angle(random), pitch(random), angle(random));
	}

	// The Euler constructor's general formula with X and Y zero
	Quaternion GeneralEulerAboutZ(float z)
	{
		float cosZ = cosf(0.5f * z), sinZ = sinf(0.5f * z);
		float cos0 = cosf(0.0f), sin0 = sinf(0.0f);
		return Quaternion(cosZ * cos0 * cos0 + sinZ * sin0 * sin0, cosZ * cos0 * sin0 - sinZ * sin0 * cos0,
			cosZ * sin0 * cos0 + sinZ * cos0 * sin0, sinZ * cos0 * cos0 - cosZ * sin0 * sin0);
	}

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

TEST_CASE("Multiplying works on const quaternions")
{
	const Quaternion a(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	const Quaternion b(Vector3f(1.0f, 0.0f, 0.0f), k_QuarterTurn);
	const Quaternion product = a * b;
	Vector3f v(0.0f, 1.0f, 0.0f);
	CheckSameVector(product * v, a * (b * v));
	CheckSameVector(a * Vector3f(1.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
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

TEST_CASE("Slerp gives the ends at t = 0 and t = 1")
{
	Quaternion start(Vector3f(0.3f, -0.7f, 1.2f));
	Quaternion end(Vector3f(-1.0f, 0.4f, 0.2f));
	Quaternion atStart = Quaternion::Slerp(start, end, 0.0);
	Quaternion atEnd = Quaternion::Slerp(start, end, 1.0);
	// q and -q are the same rotation
	CHECK(std::abs(Quaternion::Dot(atStart, start)) == doctest::Approx(1.0f));
	CHECK(std::abs(Quaternion::Dot(atEnd, end)) == doctest::Approx(1.0f));
}

TEST_CASE("Slerp between very close rotations interpolates by t")
{
	const float tiny = 0.0005f;	// well under 0.1 degrees
	Quaternion start(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
	Quaternion end(Vector3f(0.0f, 0.0f, 1.0f), tiny);
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 0.0)) == doctest::Approx(0.0f).scale(1.0f));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 0.25)) == doctest::Approx(tiny * 0.25f).epsilon(0.01));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, end, 1.0)) == doctest::Approx(tiny).epsilon(0.01));
	CHECK(Length(Quaternion::Slerp(start, end, 0.25)) == doctest::Approx(1.0f));

	Quaternion same = Quaternion::Slerp(end, end, 0.5);
	CHECK(std::abs(Quaternion::Dot(same, end)) == doctest::Approx(1.0f));
}

TEST_CASE("Slerp takes the shorter way round")
{
	Quaternion start(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
	Quaternion end(Vector3f(0.0f, 0.0f, 1.0f), k_QuarterTurn);
	Quaternion negatedEnd(-end.r, -end.i, -end.j, -end.k);
	CHECK(AngleAboutZ(Quaternion::Slerp(start, negatedEnd, 0.5)) == doctest::Approx(k_QuarterTurn * 0.5f));

	// 350 degrees apart one way is 10 the other
	Quaternion farEnd(Vector3f(0.0f, 0.0f, 1.0f), (float)DegToRad(350.0));
	CHECK(AngleAboutZ(Quaternion::Slerp(start, farEnd, 0.5)) == doctest::Approx((float)DegToRad(-5.0)));
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

TEST_CASE("EulerAnglesNear gives back the Euler angles a quaternion was made from, away from gimbal lock")
{
	std::mt19937 random(7);
	std::uniform_real_distribution<float> offset(-0.3f, 0.3f);
	for (int i = 0; i < 200; i++)
	{
		Vector3f angles = RandomAngles(random, 1.4f);
		Vector3f reference = angles + Vector3f(offset(random), offset(random), offset(random));
		CAPTURE(angles.x); CAPTURE(angles.y); CAPTURE(angles.z);
		CheckSameAngles(Quaternion(angles).EulerAnglesNear(reference), angles);
	}
}

TEST_CASE("EulerAnglesNear gives the same rotation for any angles, pitch exactly 90 degrees included")
{
	std::mt19937 random(11);
	std::vector<Vector3f> angles;
	for (int i = 0; i < 200; i++)
		angles.push_back(RandomAngles(random, 3.1f));
	angles.push_back(Vector3f(0.3f, k_QuarterTurn, 0.2f));
	angles.push_back(Vector3f(-1.0f, -k_QuarterTurn, 2.5f));
	angles.push_back(Vector3f(0.0f, k_QuarterTurn, 0.0f));

	for (const Vector3f& euler : angles)
	{
		CAPTURE(euler.x); CAPTURE(euler.y); CAPTURE(euler.z);
		Quaternion q(euler);
		for (const Vector3f& reference : { Vector3f(0.0f, 0.0f, 0.0f), RandomAngles(random, 3.1f), euler })
			CheckSameMatrix(Quaternion(q.EulerAnglesNear(reference)), q);
	}
}

TEST_CASE("EulerAnglesNear adds whole turns to stay next to the reference")
{
	Quaternion q(Vector3f(0.0f, 0.0f, -3.0f));
	CheckSameAngles(q.EulerAnglesNear(Vector3f(0.0f, 0.0f, 3.2f)), Vector3f(0.0f, 0.0f, 6.2831853f - 3.0f));
	CheckSameAngles(q.EulerAnglesNear(Vector3f(0.0f, 0.0f, 15.0f)), Vector3f(0.0f, 0.0f, 6.0f * 3.1415927f - 3.0f));
}

TEST_CASE("EulerAnglesNear picks the triple nearer the reference")
{
	// (180, 0, 180) and (0, 180, 0) degrees are the same rotation
	const float pi = 3.1415927f;
	Quaternion q(Vector3f(pi, 0.0f, pi));
	CheckSameAngles(q.EulerAnglesNear(Vector3f(3.0f, 0.1f, 3.0f)), Vector3f(pi, 0.0f, pi));
	CheckSameAngles(q.EulerAnglesNear(Vector3f(0.1f, 3.0f, -0.1f)), Vector3f(0.0f, pi, 0.0f));
}

TEST_CASE("At gimbal lock EulerAnglesNear keeps the reference's X")
{
	for (float pitch : { k_QuarterTurn, -k_QuarterTurn })
	{
		CAPTURE(pitch);
		Vector3f before(0.3f, pitch, 0.2f);
		Quaternion turned = Quaternion(Vector3f(0.0f, 0.0f, 0.05f)) * Quaternion(before);
		Vector3f after = turned.EulerAnglesNear(before);
		CHECK(after.x == before.x);
		CHECK(after.y == doctest::Approx(pitch).epsilon(1e-4));
		CHECK(after.z == doctest::Approx(0.25f).epsilon(1e-4));
	}
}

TEST_CASE("Euler angles about Z alone give the same quaternion as the general formula")
{
	for (float z : { 0.0f, 0.4f, -2.0f, 7.5f, 100.0f })
	{
		CAPTURE(z);
		Quaternion fast(Vector3f(0.0f, 0.0f, z));
		Quaternion general = GeneralEulerAboutZ(z);
		CHECK(fast.r == general.r);
		CHECK(fast.i == 0.0f);
		CHECK(fast.j == 0.0f);
		CHECK(fast.k == general.k);
	}
}

TEST_CASE("AngleBetween measures how far apart two rotations are")
{
	Quaternion a(Vector3f(0.0f, 0.0f, 1.0f), 0.2f);
	Quaternion b(Vector3f(0.0f, 0.0f, 1.0f), 0.7f);
	CHECK(Quaternion::AngleBetween(a, b) == doctest::Approx(0.5f));
	CHECK(Quaternion::AngleBetween(a, a) == doctest::Approx(0.0f).scale(1.0f));
	// q and -q are the same rotation
	CHECK(Quaternion::AngleBetween(b, b.Scale(-1.0f)) == doctest::Approx(0.0f).scale(1.0f));
}
